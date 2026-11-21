# Shared helpers for converter-cli tests. Dot-source from a test script.

# Writes a minimal 2x2 24-bit BMP (bottom-up, rows padded to 4 bytes) to $Path.
function New-TestBmp([string]$Path) {
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
    [IO.File]::WriteAllBytes($Path, [byte[]]($header + $pixels))
}

# SHA-256 of a file as a hex string. Uses .NET directly instead of Get-FileHash:
# when ctest runs under PowerShell 7 (as in CI), the Windows PowerShell 5.1
# child inherits PSModulePath and cannot autoload that cmdlet.
function Get-TestFileHash([string]$Path) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return [BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($Path)))
    } finally {
        $sha.Dispose()
    }
}
