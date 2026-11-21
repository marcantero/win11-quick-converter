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
