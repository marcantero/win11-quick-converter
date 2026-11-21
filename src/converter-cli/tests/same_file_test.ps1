#Requires -Version 5.1
# Regression test for issue #45: converting a file onto itself must be rejected
# with exit code 2 and must leave the source untouched (both WIC and WebP paths).
#
# Usage: same_file_test.ps1 -Cli <path\to\converter-cli.exe> -WorkDir <scratch dir>

param(
    [Parameter(Mandatory = $true)][string]$Cli,
    [Parameter(Mandatory = $true)][string]$WorkDir
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

$dir = Join-Path $WorkDir 'same_file'
if (Test-Path -LiteralPath $dir) { Remove-Item -LiteralPath $dir -Recurse -Force }
New-Item -ItemType Directory -Path $dir | Out-Null

$source = Join-Path $dir 'image.bmp'
$webpSource = Join-Path $dir 'image.webp'
New-TestBmp $source

# A real WebP source, produced by the CLI itself, for the libwebp path.
& $Cli --input $source --output $webpSource --format webp | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Host "FAIL: could not prepare WebP source"; exit 1 }

# Each case spells the output differently but points at the source file.
$cases = @(
    @{ Name = 'identical path';  Input = $source;     Output = $source;                                        Format = 'bmp' },
    @{ Name = 'different case';  Input = $source;     Output = (Join-Path $dir 'IMAGE.BMP');                  Format = 'bmp' },
    @{ Name = 'dot segments';    Input = $source;     Output = (Join-Path $dir "..\same_file\.\image.bmp");   Format = 'bmp' },
    @{ Name = 'webp onto itself'; Input = $webpSource; Output = $webpSource;                                   Format = 'webp' }
)

$failures = 0
foreach ($case in $cases) {
    $before = (Get-FileHash -LiteralPath $case.Input).Hash
    & $Cli --input $case.Input --output $case.Output --format $case.Format
    $exitCode = $LASTEXITCODE
    $after = (Get-FileHash -LiteralPath $case.Input).Hash

    if ($exitCode -ne 2) {
        Write-Host "FAIL [$($case.Name)]: expected exit 2, got $exitCode"
        $failures++
    } elseif ($before -ne $after) {
        Write-Host "FAIL [$($case.Name)]: source file was modified"
        $failures++
    } else {
        Write-Host "PASS [$($case.Name)]"
    }
}

# Sanity check: a genuinely different output must still convert.
& $Cli --input $source --output (Join-Path $dir 'image.png') --format png | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL [different output]: expected exit 0, got $LASTEXITCODE"
    $failures++
} else {
    Write-Host "PASS [different output]"
}

Remove-Item -LiteralPath $dir -Recurse -Force
exit $failures
