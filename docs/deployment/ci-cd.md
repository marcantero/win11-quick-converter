# CI/CD

Workflow: `.github/workflows/build.yml` — **Windows C++ Build & Release**.

## Triggers

| Event | Effect |
|---|---|
| Push to `main` | Build + upload workflow artifact |
| Pull request to `main` | Build + upload workflow artifact |
| Push of a tag `v*` | Build + artifact + **GitHub Release** |
| Manual (*Actions → Run workflow*) | Build + upload workflow artifact |

Pushes and PRs that only change documentation (`docs/**`, any `*.md`, `.github/agents/**`) are skipped. Tag pushes always run.

If a newer commit is pushed to the same branch/PR while a run is in progress, the older run is cancelled (`concurrency` group per ref).

The workflow defaults to read-only `contents` permission; only the `release` job gets `contents: write`.

## Job `build` (`windows-latest`)

1. **Checkout** (`actions/checkout@v4`).
2. **Configure** — `cmake -S . -B build` (fetches libwebp).
3. **Build** — `cmake --build build --config Release`.
4. **Prepare release structure** — copies the three binaries, the `packaging/` folder and `install.bat` into `ReleaseDir/` with the layout expected by `register-msix.ps1`.
5. **Create ZIP** — `SnapFormat.zip`.
6. **Compile installer** — `iscc packaging/installer.iss` (Inno Setup is preinstalled on GitHub's Windows images) → `build/installer/SnapFormat_Setup.exe`.
7. **Upload artifact** `SnapFormat-Binaries` (ZIP + setup), kept for **14 days**.

## Job `release` (`ubuntu-latest`, tags only, `contents: write`)

Runs after `build` succeeds. Downloads the `SnapFormat-Binaries` artifact and publishes a GitHub Release with `softprops/action-gh-release@v2`, attaching both files and auto-generating release notes.

## Making a release

```bash
# 1. Update the version in CMakeLists.txt, packaging/installer.iss, packaging/AppxManifest.xml
# 2. Merge to main, then:
git tag v1.4.0
git push origin v1.4.0
```

The release appears on the GitHub Releases page a few minutes later.

## Gaps

- Only the **Release** configuration is built (issue #26 asked for Debug too).
- No automated tests run (there is no test target registered with CTest).
- No compiler warnings-as-errors, static analysis or formatting check.
- Artifacts are **not code-signed**.
- The MSIX package itself is not built in CI; it is built on the user's machine by `install.bat`.

Suggestions are listed in [Project review](../development/project-review.md).
