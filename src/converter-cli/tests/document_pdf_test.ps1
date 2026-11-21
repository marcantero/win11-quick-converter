#Requires -Version 5.1
# Tests for issue #58: office documents convert to PDF through LibreOffice.
# A fake soffice.exe stands in for LibreOffice, so this runs on machines
# (and CI runners) without it.
#
# Usage: document_pdf_test.ps1 -Cli <converter-cli.exe> -FakeSoffice <fake-soffice.exe> -WorkDir <scratch dir>

param(
    [Parameter(Mandatory = $true)][string]$Cli,
    [Parameter(Mandatory = $true)][string]$FakeSoffice,
    [Parameter(Mandatory = $true)][string]$WorkDir
)

$ErrorActionPreference = 'Stop'

$dir = Join-Path $WorkDir 'document_pdf'
if (Test-Path -LiteralPath $dir) { Remove-Item -LiteralPath $dir -Recurse -Force }
New-Item -ItemType Directory -Path $dir | Out-Null

# Isolate temporary files and the LibreOffice profile inside the work directory.
$tempDir = Join-Path $dir 'temp'
$appData = Join-Path $dir 'appdata'
New-Item -ItemType Directory -Path $tempDir, $appData | Out-Null
$env:TEMP = $tempDir
$env:TMP = $tempDir
$env:LOCALAPPDATA = $appData
$env:SNAPFORMAT_SOFFICE = $FakeSoffice
$env:SNAPFORMAT_SOFFICE_TIMEOUT = '3'
$argsFile = Join-Path $dir 'soffice-args.txt'
$env:FAKE_SOFFICE_ARGS_FILE = $argsFile

$source = Join-Path $dir 'report.docx'
[IO.File]::WriteAllText($source, 'not a real document; the fake soffice does not read it')

$script:failures = 0
function Fail([string]$name, [string]$why) { Write-Host "FAIL [$name]: $why"; $script:failures++ }
function Pass([string]$name) { Write-Host "PASS [$name]" }

function Invoke-Cli([string]$mode, [string[]]$arguments) {
    $env:FAKE_SOFFICE_MODE = $mode
    if (Test-Path -LiteralPath $argsFile) { Remove-Item -LiteralPath $argsFile -Force }
    $output = & $Cli @arguments 2>&1 | Out-String
    return @{ ExitCode = $LASTEXITCODE; Output = $output }
}

function Test-TempEmpty([string]$name) {
    $left = @(Get-ChildItem -LiteralPath $tempDir -Force)
    if ($left.Count -ne 0) {
        Fail $name "temporary files left behind: $($left.Name -join ', ')"
        $left | Remove-Item -Recurse -Force
        return $false
    }
    return $true
}

function Test-NoFakeProcesses([string]$name) {
    $name2 = [IO.Path]::GetFileNameWithoutExtension($FakeSoffice)
    $alive = @(Get-Process -Name $name2 -ErrorAction SilentlyContinue)
    if ($alive.Count -ne 0) {
        $alive | Stop-Process -Force
        Fail $name "$($alive.Count) soffice process(es) still running"
        return $false
    }
    return $true
}

function Test-IsPdf([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return $false }
    $bytes = [IO.File]::ReadAllBytes($path)
    return $bytes.Length -gt 4 -and [Text.Encoding]::ASCII.GetString($bytes, 0, 5) -eq '%PDF-'
}

# 1. Basic conversion and the arguments passed to LibreOffice.
$output = Join-Path $dir 'report.pdf'
$r = Invoke-Cli 'ok' @('--input', $source, '--output', $output, '--format', 'pdf')
if ($r.ExitCode -ne 0) { Fail 'basic' "expected exit 0, got $($r.ExitCode): $($r.Output)" }
elseif (-not (Test-IsPdf $output)) { Fail 'basic' 'output is missing or not a PDF' }
elseif (Test-TempEmpty 'basic') {
    $passed = @(Get-Content -LiteralPath $argsFile -Encoding UTF8)
    $expected = @('--headless', '--convert-to', 'pdf', '--outdir', $source)
    $missing = @($expected | Where-Object { $passed -notcontains $_ })
    $profileArg = @($passed | Where-Object { $_ -like '-env:UserInstallation=file:///*' })
    $settings = Join-Path $appData 'SnapFormat\libreoffice-profile\user\registrymodifications.xcu'
    if ($missing.Count -ne 0) { Fail 'basic' "soffice arguments missing: $($missing -join ', ')" }
    elseif ($profileArg.Count -ne 1) { Fail 'basic' 'soffice did not get a private profile' }
    elseif (-not (Select-String -LiteralPath $settings -Pattern 'DisableMacrosExecution' -Quiet)) {
        Fail 'basic' 'profile does not disable macros'
    }
    else { Pass 'basic' }
}

# 2. Non-ASCII names and a missing output directory.
$japanese = -join ([char[]](0x5831, 0x544A))                      # report in Japanese
$emoji = [char]::ConvertFromUtf32(0x1F4C4)                          # page facing up
$unicodeSource = Join-Path $dir "doc_${japanese}_${emoji} n.docx"
Copy-Item -LiteralPath $source -Destination $unicodeSource
$unicodeOutput = Join-Path $dir "out ${japanese}\doc_${japanese}_${emoji}.pdf"
$r = Invoke-Cli 'ok' @('--input', $unicodeSource, '--output', $unicodeOutput, '--format', 'PDF')
if ($r.ExitCode -ne 0) { Fail 'unicode' "expected exit 0, got $($r.ExitCode): $($r.Output)" }
elseif (-not (Test-IsPdf $unicodeOutput)) { Fail 'unicode' 'output is missing or not a PDF' }
elseif (Test-TempEmpty 'unicode') { Pass 'unicode' }

# 3. Validation errors (exit 2) never start LibreOffice.
$validation = @(
    @{ Name = 'document to png'; Args = @('--input', $source, '--output', (Join-Path $dir 'x.png'), '--format', 'png') },
    @{ Name = 'same file';       Args = @('--input', $source, '--output', $source, '--format', 'pdf') },
    @{ Name = 'missing input';   Args = @('--input', (Join-Path $dir 'missing.docx'), '--output', (Join-Path $dir 'm.pdf'), '--format', 'pdf') }
)
$sourceBytes = [IO.File]::ReadAllBytes($source)
foreach ($case in $validation) {
    $r = Invoke-Cli 'ok' $case.Args
    if ($r.ExitCode -ne 2) { Fail $case.Name "expected exit 2, got $($r.ExitCode): $($r.Output)" }
    elseif (Test-Path -LiteralPath $argsFile) { Fail $case.Name 'LibreOffice was started' }
    elseif ([Convert]::ToBase64String([IO.File]::ReadAllBytes($source)) -ne [Convert]::ToBase64String($sourceBytes)) {
        Fail $case.Name 'source file was modified'
    }
    else { Pass $case.Name }
}

# 4. Failures (exit 3) leave no output and keep an existing output intact.
$existing = Join-Path $dir 'existing.pdf'
foreach ($mode in 'no-output', 'fail', 'hang') {
    [IO.File]::WriteAllText($existing, 'previous content')
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $r = Invoke-Cli $mode @('--input', $source, '--output', $existing, '--format', 'pdf')
    $watch.Stop()
    $fresh = Join-Path $dir "fresh-$mode.pdf"
    $r2 = Invoke-Cli $mode @('--input', $source, '--output', $fresh, '--format', 'pdf')

    if ($r.ExitCode -ne 3 -or $r2.ExitCode -ne 3) { Fail $mode "expected exit 3, got $($r.ExitCode)/$($r2.ExitCode): $($r.Output)" }
    elseif ([IO.File]::ReadAllText($existing) -ne 'previous content') { Fail $mode 'existing output was changed' }
    elseif (Test-Path -LiteralPath $fresh) { Fail $mode 'a partial output was left behind' }
    elseif (@(Get-ChildItem -LiteralPath $dir -Filter '*.snapformat-*' -Force).Count -ne 0) { Fail $mode 'staging file left behind' }
    elseif ($mode -eq 'hang' -and $r.Output -notmatch 'did not finish') { Fail $mode "unexpected message: $($r.Output)" }
    elseif ($mode -eq 'hang' -and $watch.Elapsed.TotalSeconds -gt 30) { Fail $mode 'timeout was not enforced' }
    elseif ((Test-TempEmpty $mode) -and (Test-NoFakeProcesses $mode)) { Pass $mode }
}

# 5. Processes left behind by the launcher are killed; restart requests are retried.
foreach ($mode in 'spawn-hang', 'restart-once') {
    $out = Join-Path $dir "$mode.pdf"
    $r = Invoke-Cli $mode @('--input', $source, '--output', $out, '--format', 'pdf')
    if ($r.ExitCode -ne 0) { Fail $mode "expected exit 0, got $($r.ExitCode): $($r.Output)" }
    elseif (-not (Test-IsPdf $out)) { Fail $mode 'output is missing or not a PDF' }
    elseif ((Test-TempEmpty $mode) -and (Test-NoFakeProcesses $mode)) { Pass $mode }
}

# 6. An override pointing nowhere is reported.
$env:SNAPFORMAT_SOFFICE = Join-Path $dir 'nope\soffice.exe'
$r = Invoke-Cli 'ok' @('--input', $source, '--output', (Join-Path $dir 'none.pdf'), '--format', 'pdf')
if ($r.ExitCode -ne 3 -or $r.Output -notmatch 'SNAPFORMAT_SOFFICE') { Fail 'missing soffice' "expected exit 3, got $($r.ExitCode): $($r.Output)" }
else { Pass 'missing soffice' }

# 7. Without the override, a LibreOffice bundled next to the CLI is used first.
Remove-Item Env:\SNAPFORMAT_SOFFICE
$bundle = Join-Path $dir 'bundle'
$program = Join-Path $bundle 'libreoffice\program'
New-Item -ItemType Directory -Path $program | Out-Null
Copy-Item -LiteralPath $Cli -Destination $bundle
Copy-Item -LiteralPath $FakeSoffice -Destination (Join-Path $program 'soffice.exe')
$bundledCli = Join-Path $bundle ([IO.Path]::GetFileName($Cli))
$out = Join-Path $dir 'bundled.pdf'
$env:FAKE_SOFFICE_MODE = 'ok'
if (Test-Path -LiteralPath $argsFile) { Remove-Item -LiteralPath $argsFile -Force }
$text = & $bundledCli --input $source --output $out --format pdf 2>&1 | Out-String
if ($LASTEXITCODE -ne 0) { Fail 'bundled' "expected exit 0, got ${LASTEXITCODE}: $text" }
elseif (-not (Test-Path -LiteralPath $argsFile) -or -not (Test-IsPdf $out)) { Fail 'bundled' 'bundled soffice was not used' }
else { Pass 'bundled' }

# 8. Images still go through WIC and reject pdf.
$image = Join-Path $dir 'image.bmp'
[IO.File]::WriteAllBytes($image, [byte[]](0x42, 0x4D))
$r = Invoke-Cli 'ok' @('--input', $image, '--output', (Join-Path $dir 'image.pdf'), '--format', 'pdf')
if ($r.ExitCode -ne 2) { Fail 'image to pdf' "expected exit 2, got $($r.ExitCode): $($r.Output)" }
else { Pass 'image to pdf' }

if ($script:failures -eq 0) { Remove-Item -LiteralPath $dir -Recurse -Force }
exit $script:failures
