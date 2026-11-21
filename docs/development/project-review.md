# Project Review

A review of the code base as of November 2026 (commit `ee774f6`, tag `v1.3.0`). Items are grouped by severity. Each lists the location and a suggested fix. Nothing here has been changed in code yet — treat this as a backlog.

## 🔴 High

### ~~Development certificate private key committed to the repository~~ — fixed in [#43](https://github.com/marcantero/win11-quick-converter/issues/43)
- **Where:** `Win11QuickConverter_DevCert.pfx`, `packaging/Win11QuickConverter_DevCert.pfx`, password in `packaging/register-msix.ps1`.
- **Risk:** the script installs this certificate into `LocalMachine\Root`. On any machine that trusts it, anyone with the public PFX can sign binaries (or other certificates) that Windows will trust.
- **Fix:** delete both PFX files from the repo (and history if possible), add `*.pfx` to `.gitignore`, ask affected users to remove `CN=MarcAntero` from Trusted Root, generate per-machine certificates and install them into `LocalMachine\TrustedPeople` instead of `Root`. See [Packaging](../deployment/packaging.md#signing-certificate).

### ~~Detached worker thread can outlive the DLL~~ — fixed in [#44](https://github.com/marcantero/win11-quick-converter/issues/44)
- **Where:** `FormatSubCommand::Invoke`, `src/shell-ext/src/dummy.cpp` (batch branch, `std::thread(...).detach()`).
- **Risk:** the thread runs DLL code but does not increment `g_objectCount` / `g_lockCount`. Once Explorer releases the command objects, `DllCanUnloadNow` returns `S_OK` and the DLL can be unloaded while the thread is still converting → Explorer crash.
- **Fix:** increment the lock count at thread start and decrement at exit, or pin the module with `GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)` + `FreeLibraryAndExitThread`. Alternatively use `SHCreateThread` with `CTF_REF_COUNTED`.

### ~~Same-format conversion targets the input file~~ — fixed in [#45](https://github.com/marcantero/win11-quick-converter/issues/45)
- **Where:** `FormatSubCommand::Invoke` — output path is the input with its extension replaced.
- **Risk:** *To PNG* on `a.png` (or *To TIFF* on `a.tiff`) makes input and output the same path. The CLI opens the output for writing while decoding the input → sharing violation, or a truncated/corrupted source in the WebP path (`std::ofstream` truncates the file after decoding).
- **Fix:** in the CLI, reject `--input` == `--output` (`std::filesystem::equivalent`) with exit `2`; in the DLL, hide or disable the subcommand matching the source format, or generate a unique name (`a (1).png`).

## 🟠 Medium

### ~~Unicode paths~~ — fixed in [#46](https://github.com/marcantero/win11-quick-converter/issues/46)
- **Where:** `main(int argc, char* argv[])` in `src/converter-cli/src/main.cpp`.
- **Risk:** `argv` is in the ANSI code page. File names with characters outside it (e.g. Japanese, emoji) become `?` and the conversion fails with exit `2`, although the DLL passes a proper UTF-16 command line.
- **Fix:** use `wmain(int argc, wchar_t* argv[])` and `std::wstring` paths, or add `<activeCodePage>UTF-8</activeCodePage>` to an application manifest.

### Existing files are overwritten silently
- **Where:** DLL output naming + CLI `InitializeFromFilename(GENERIC_WRITE)` / `std::ofstream`.
- **Fix:** add a `--no-overwrite` (or `--overwrite`) CLI flag and let the DLL choose a non-colliding name.

### ~~Errors are invisible to Explorer users~~ — fixed in [#39](https://github.com/marcantero/win11-quick-converter/issues/39)
- **Where:** the DLL ignores the CLI exit code; single-file conversions are fire-and-forget.
- **Fix:** collect exit codes and show a summary (toast notification — issue [#39](https://github.com/marcantero/win11-quick-converter/issues/39) — or a final line in the progress dialog).

### ~~Unchecked HRESULTs in the WebP path~~ — fixed in [#47](https://github.com/marcantero/win11-quick-converter/issues/47)
- **Where:** `convert_to_webp()` — `GetSize` and `CreateFormatConverter` results are ignored (a failed `CreateFormatConverter` dereferences a null pointer); `width * height * 4` is computed in 32-bit `UINT` and can overflow for very large images; `ofstream` write errors are not checked; error messages omit the `HRESULT`.
- **Fix:** mirror the checks used in `convert_with_wic()`, compute the size in `size_t` with an overflow check, check `outFile.good()` after writing. Consider sharing the decode/validate prologue between both paths.

### Batch conversion is sequential and waits with `INFINITE`
The `INFINITE` wait and mid-file cancel are fixed in [#44](https://github.com/marcantero/win11-quick-converter/issues/44); parallel conversion remains open in [#52](https://github.com/marcantero/win11-quick-converter/issues/52).
- **Where:** batch thread in `FormatSubCommand::Invoke`.
- **Risk:** a hung CLI blocks the batch forever; cancel only takes effect between files.
- **Fix:** wait with a timeout and poll `HasUserCancelled()`; optionally run 2–4 conversions in parallel.

## 🟡 Low / housekeeping

- **Stray files tracked in git:** `test_output.webp` (empty), `build/tools/win11qc_convert.bat` (inside the ignored `build/` folder, hard-codes a local absolute path and an obsolete output directory). Remove them.
- **File naming:** `src/shell-ext/src/dummy.cpp` holds the real implementation — rename to e.g. `shell_extension.cpp` and split registration/commands into separate files. Remove the two duplicated trailing comments at the end of the file.
- **Language rule:** several code comments (`dummy.cpp`), the CI workflow comments, `install.bat` and `register-msix.ps1` messages are in Catalan, contrary to the "English only" project rule. Either translate them or relax the rule for user-facing scripts.
- **Classic-menu icon:** the registry `Icon` value points to `shell-ext.dll`, which has no icon resource; point it to `snapformat.ico` or embed the icon as a resource.
- **Version drift:** `0.1.0` (CMake, Inno) vs `1.0.0.0` (manifest) vs tag `v1.3.0`. Derive all from one source (e.g. CMake `configure_file`, `iscc /DAppVersion=`).
- **`MaxVersionTested="10.0.22000.0"`** in the manifest is old; bump to the latest tested build.
- **Two installation channels can be installed simultaneously** and both register the same CLSID. Make the installer detect/remove the MSIX package (or vice versa).
- **`test_com` fallback paths** reference the pre-unification output directory (`build/src/shell-ext/<Config>`).
- **Inno uninstall** may leave `shell-ext.dll` locked; add `restartreplace uninsrestartdelete` flags or close Explorer via the Restart Manager.
- **`--quality` silently ignored** for PNG/BMP/TIFF; consider a warning, or map it to PNG compression / TIFF compression options.
- **Metadata & color profiles** are dropped; copying EXIF orientation at least would avoid rotated JPEG→PNG results from phone photos.

## CI/CD suggestions

- ~~Skip docs-only changes, cancel superseded runs, least-privilege permissions~~ — done.
- Build Debug as well as Release (matrix).
- Add CTest-based CLI tests with fixture images.
- Treat warnings as errors (`/WX`) once the code is clean.
- Sign `shell-ext.dll`, `converter-cli.exe` and `SnapFormat_Setup.exe` in the release job.
- Pin third-party actions to commit SHAs.

## Strengths

- Clean separation between the Explorer-hosted DLL and the out-of-process engine.
- Consistent RAII and `ComPtr` usage; careful `HRESULT` checking in the WIC path.
- Per-user registration (no admin needed) with the Inno installer.
- Small, documented, stable CLI contract with deterministic exit codes.
- Automated release pipeline producing both distribution formats.
