# Testing

The project currently has **no automated unit or integration tests** and nothing is registered with CTest. Verification is manual plus one diagnostic executable.

## `shell_ext_test_com`

Source: `src/shell-ext/test_com/main.cpp` · Output: `build\tests\Release\shell_ext_test_com.exe`.

It checks that the shell extension can be activated through COM:

1. `CoCreateInstance(kShellExtensionClsid, IExplorerCommand)` — requires the DLL to be registered (`regsvr32`).
2. On success, prints `GetTitle: Convert with SnapFormat` and exits `0`.
3. On failure, it tries to `LoadLibrary` the DLL directly and call `DllGetClassObject` → `CreateInstance` → `GetTitle`, printing each `HRESULT`, then exits `2`.

> The fallback searches `build\src\shell-ext\<Config>\shell-ext.dll`, an output path that no longer exists since outputs were unified under `build\bin\<Config>`. Update the candidate list if you rely on the fallback.

```powershell
regsvr32 /s build\bin\Release\shell-ext.dll
build\tests\Release\shell_ext_test_com.exe
```

## CLI smoke tests

```powershell
$cli = "build\bin\Release\converter-cli.exe"
& $cli --help;                                                  $LASTEXITCODE   # 0
& $cli --input x.png;                                           $LASTEXITCODE   # 1 (missing options)
& $cli --input x.png --output y.png --format png --quality 101; $LASTEXITCODE   # 1
& $cli --input missing.png --output y.png --format png;         $LASTEXITCODE   # 2
& $cli --input in.png --output y.gif --format gif;              $LASTEXITCODE   # 2 (unsupported)
foreach ($f in 'png','jpg','bmp','tiff','webp') {
    & $cli --input sample.jpg --output "out\sample.$f" --format $f; $LASTEXITCODE  # 0
}
```

## Manual test plan (Explorer)

| # | Scenario | Expected |
|---|---|---|
| 1 | Right-click a single `.jpg` | "Convert with SnapFormat" with icon and 5 sub-items |
| 2 | *To PNG* on one file | `.png` appears next to it, no window |
| 3 | Select 5 files → *To WebP* | Progress dialog, 5 `.webp` files |
| 4 | Select 10 files → Cancel mid-way | Stops after current file |
| 5 | Select 16 files | Menu entry hidden |
| 6 | Right-click `.webp`, `.heic`, `.avif` | Entry visible; HEIC/AVIF convert if codecs installed |
| 7 | Right-click a `.txt` / folder | Entry not shown |
| 8 | PNG with transparency → *To JPG* / *To WebP* | JPG flattened, WebP keeps alpha |
| 9 | File in a path with spaces and accents | Converts correctly |
| 10 | Uninstall | Entry disappears after Explorer restart |

## Suggested automation

- Add a CTest target that runs the CLI against a small set of fixture images under `tests/fixtures/` and checks exit codes plus output dimensions.
- Extract `parse_arguments` into a library so it can be unit-tested (e.g. with doctest or GoogleTest via `FetchContent`).
- Run both in CI.
