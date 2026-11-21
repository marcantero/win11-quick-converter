#Requires -Version 5.1
# Regression test for issue #46: file names with characters outside the ANSI
# code page (Japanese, emoji, ...) must convert successfully.
#
# Usage: unicode_path_test.ps1 -Cli <path\to\converter-cli.exe> -WorkDir <scratch dir>

param(
    [Parameter(Mandatory = $true)][string]$Cli,
    [Parameter(Mandatory = $true)][string]$WorkDir
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# Build the non-ASCII name from code points so this script stays pure ASCII
# (Windows PowerShell 5.1 misreads BOM-less UTF-8 sources).
$japanese = -join ([char[]](0x753B, 0x50CF))                  # "画像" (image)
$emoji = [char]::ConvertFromUtf32(0x1F5BC)                       # framed picture (surrogate pair)
$baseName = "test_${japanese}_${emoji}"

$dir = Join-Path $WorkDir "unicode_${japanese}"
if (Test-Path -LiteralPath $dir) { Remove-Item -LiteralPath $dir -Recurse -Force }
New-Item -ItemType Directory -Path $dir | Out-Null

$source = Join-Path $dir "$baseName.bmp"
New-TestBmp $source

$failures = 0
foreach ($format in 'png', 'webp') {
    $output = Join-Path $dir "$baseName.$format"
    & $Cli --input $source --output $output --format $format
    $exitCode = $LASTEXITCODE

    if ($exitCode -ne 0) {
        Write-Host "FAIL [$format]: converter-cli exited with $exitCode"
        $failures++
    } elseif (-not (Test-Path -LiteralPath $output) -or (Get-Item -LiteralPath $output).Length -eq 0) {
        Write-Host "FAIL [$format]: output file was not created"
        $failures++
    } else {
        Write-Host "PASS [$format]"
    }
}

Remove-Item -LiteralPath $dir -Recurse -Force
exit $failures
