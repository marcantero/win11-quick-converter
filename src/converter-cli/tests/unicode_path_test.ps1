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

# Build the non-ASCII name from code points so this script stays pure ASCII
# (Windows PowerShell 5.1 misreads BOM-less UTF-8 sources).
$japanese = -join ([char[]](0x753B, 0x50CF))                  # "画像" (image)
$emoji = [char]::ConvertFromUtf32(0x1F5BC)                       # framed picture (surrogate pair)
$baseName = "test_${japanese}_${emoji}"

$dir = Join-Path $WorkDir "unicode_${japanese}"
if (Test-Path -LiteralPath $dir) { Remove-Item -LiteralPath $dir -Recurse -Force }
New-Item -ItemType Directory -Path $dir | Out-Null

# Minimal 2x2 24-bit BMP (bottom-up, rows padded to 4 bytes).
$pixels = [byte[]](
    0, 0, 255,   0, 255, 0,     0, 0,
    255, 0, 0,   255, 255, 255, 0, 0)
$header = New-Object byte[] 54
$header[0] = 0x42; $header[1] = 0x4D               # "BM"
[BitConverter]::GetBytes([int](54 + $pixels.Length)).CopyTo($header, 2)
[BitConverter]::GetBytes([int]54).CopyTo($header, 10)
[BitConverter]::GetBytes([int]40).CopyTo($header, 14)
[BitConverter]::GetBytes([int]2).CopyTo($header, 18)
[BitConverter]::GetBytes([int]2).CopyTo($header, 22)
[BitConverter]::GetBytes([int16]1).CopyTo($header, 26)
[BitConverter]::GetBytes([int16]24).CopyTo($header, 28)
[BitConverter]::GetBytes([int]$pixels.Length).CopyTo($header, 34)

$source = Join-Path $dir "$baseName.bmp"
[IO.File]::WriteAllBytes($source, [byte[]]($header + $pixels))

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
