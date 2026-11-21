#Requires -Version 5.1
# Regression test for issue #47: the WebP path must reject images larger than
# WEBP_MAX_DIMENSION with exit code 3 and a clear message, without leaving an
# output file behind, and must still convert normal images (lossless and lossy).
#
# Usage: webp_limits_test.ps1 -Cli <path\to\converter-cli.exe> -WorkDir <scratch dir>

param(
    [Parameter(Mandatory = $true)][string]$Cli,
    [Parameter(Mandatory = $true)][string]$WorkDir
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'common.ps1')

# Writes a solid grey 24-bit BMP of the given size (rows padded to 4 bytes).
function New-SolidBmp([string]$Path, [int]$Width, [int]$Height) {
    $rowSize = [int]([Math]::Ceiling($Width * 3 / 4) * 4)
    $pixels = New-Object byte[] ($rowSize * $Height)
    for ($i = 0; $i -lt $pixels.Length; $i++) { $pixels[$i] = 128 }
    $header = New-Object byte[] 54
    $header[0] = 0x42; $header[1] = 0x4D
    [BitConverter]::GetBytes([int](54 + $pixels.Length)).CopyTo($header, 2)
    [BitConverter]::GetBytes([int]54).CopyTo($header, 10)
    [BitConverter]::GetBytes([int]40).CopyTo($header, 14)
    [BitConverter]::GetBytes([int]$Width).CopyTo($header, 18)
    [BitConverter]::GetBytes([int]$Height).CopyTo($header, 22)
    [BitConverter]::GetBytes([int16]1).CopyTo($header, 26)
    [BitConverter]::GetBytes([int16]24).CopyTo($header, 28)
    [BitConverter]::GetBytes([int]$pixels.Length).CopyTo($header, 34)
    [IO.File]::WriteAllBytes($Path, [byte[]]($header + $pixels))
}

$dir = Join-Path $WorkDir 'webp_limits'
if (Test-Path -LiteralPath $dir) { Remove-Item -LiteralPath $dir -Recurse -Force }
New-Item -ItemType Directory -Path $dir | Out-Null

$failures = 0

# 1. Wider than WebP allows (16383): must fail cleanly.
$wide = Join-Path $dir 'wide.bmp'
$wideOut = Join-Path $dir 'wide.webp'
New-SolidBmp $wide 16384 1
$output = & $Cli --input $wide --output $wideOut --format webp 2>&1 | Out-String
if ($LASTEXITCODE -ne 3) {
    Write-Host "FAIL [too wide]: expected exit 3, got $LASTEXITCODE"
    $failures++
} elseif ($output -notmatch '16383') {
    Write-Host "FAIL [too wide]: message does not mention the limit: $output"
    $failures++
} elseif (Test-Path -LiteralPath $wideOut) {
    Write-Host "FAIL [too wide]: an output file was left behind"
    $failures++
} else {
    Write-Host "PASS [too wide]"
}

# 2. Exactly at the limit still converts.
$edge = Join-Path $dir 'edge.bmp'
New-SolidBmp $edge 16383 1
& $Cli --input $edge --output (Join-Path $dir 'edge.webp') --format webp | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL [at limit]: expected exit 0, got $LASTEXITCODE"
    $failures++
} else {
    Write-Host "PASS [at limit]"
}

# 3. Normal image, lossless and lossy.
$small = Join-Path $dir 'small.bmp'
New-TestBmp $small
foreach ($case in @(@{ Name = 'lossless'; Args = @() }, @{ Name = 'lossy'; Args = @('--quality', '80') })) {
    $out = Join-Path $dir "small_$($case.Name).webp"
    & $Cli --input $small --output $out --format webp @($case.Args) | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "FAIL [$($case.Name)]: expected exit 0, got $LASTEXITCODE"
        $failures++
    } elseif ((Get-Item -LiteralPath $out).Length -eq 0) {
        Write-Host "FAIL [$($case.Name)]: output is empty"
        $failures++
    } else {
        Write-Host "PASS [$($case.Name)]"
    }
}

Remove-Item -LiteralPath $dir -Recurse -Force
exit $failures
