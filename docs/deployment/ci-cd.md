# CI/CD

Workflow: `.github/workflows/build.yml` — **Windows C++ Build & Release**.

## Triggers

| Event | Effect |
|---|---|
| Push to `main` | Build + upload workflow artifact |
| Pull request to `main` | Build + upload workflow artifact |
| Push of a tag `vX.Y.Z` | Build + artifact + **GitHub Release** `vX.Y.Z` |
| Manual (*Actions → Run workflow*), version empty | Build + upload workflow artifact |
| Manual, version `X.Y.Z` (from `main`) | Build + artifact + creates tag `vX.Y.Z` + **GitHub Release** |

Pushes and PRs that only change documentation (`docs/**`, any `*.md`, `.github/agents/**`) are skipped. Tag pushes always run.

If a newer commit is pushed to the same branch/PR while a run is in progress, the older run is cancelled (`concurrency` group per ref and event). Release runs (tags and manual runs) are never cancelled.

The workflow defaults to read-only `contents` permission; only the `release` job gets `contents: write`.

## Job `build` (`windows-latest`)

1. **Checkout** (`actions/checkout@v7`).
2. **Determine version** — from the tag (`v1.5.0` → `1.5.0`) or the manual input; `0.0.0` for ordinary builds. A manual release must run on `main` and fails if the tag already exists.
3. **Configure** — `cmake -S . -B build -DSNAPFORMAT_VERSION=<version>` (fetches libwebp). The version goes into the version resource of `converter-cli.exe` and `shell-ext.dll`.
4. **Build** — `cmake --build build --config Release`.
5. **Prepare release structure** — copies the three binaries, the `packaging/` folder and `install.bat` into `ReleaseDir/` with the layout expected by `register-msix.ps1`, and sets the version in the copied `AppxManifest.xml` (`<version>.0`).
6. **Create ZIP** — `SnapFormat.zip`.
7. **Fetch LibreOffice** — `scripts/fetch-libreoffice.ps1 -SmokeTest` downloads the pinned LibreOffice MSI, **verifies its SHA-256**, extracts and trims it into `build/libreoffice`, and converts a test document with it. The MSI is cached (`actions/cache`, key = hash of the script), so it is only downloaded again when the pinned version changes.
8. **Compile installer** — `iscc /DAppVersion=<version> packaging/installer.iss` (Inno Setup is preinstalled on GitHub's Windows images) → `build/installer/SnapFormat_Setup.exe`, including LibreOffice as the optional *Document conversion* component.
9. **Upload artifact** `SnapFormat-Binaries` (ZIP + setup), kept for **14 days**.

## Job `release` (`ubuntu-latest`, releases only, `contents: write`)

Runs after `build` succeeds, for a tag push or a manual run with a version. Downloads the `SnapFormat-Binaries` artifact and publishes the GitHub Release `vX.Y.Z` with `softprops/action-gh-release@v3`, attaching both files, marking it as latest and generating the release notes. For a manual run it also creates the tag on the commit that was built.

## Making a release

The git tag is the only place the version lives ([#54](https://github.com/marcantero/win11-quick-converter/issues/54)). The version fields in `CMakeLists.txt`, `packaging/installer.iss` and `packaging/AppxManifest.xml` stay at `0.0.0` and are never edited by hand.

Either:

- **GitHub:** *Actions → Windows C++ Build & Release → Run workflow*, branch `main`, enter the version (e.g. `1.5.0`); or
- **Command line:** `gh workflow run build.yml --ref main -f version=1.5.0`, or push a tag: `git tag v1.5.0 && git push origin v1.5.0`.

The release appears on the GitHub Releases page a few minutes later. Use [semantic versioning](https://semver.org/): a new feature bumps the minor version, fixes only bump the patch.

### Release notes

GitHub writes them from the pull requests merged since the previous release, grouped by `.github/release.yml` using the PRs' type labels (`type: feature`, `type: bug`, `type: security`, `type: docs`, `type: chore`). Unlabelled PRs go under *Other changes*, so give each PR the same `type:` label as its issue. You can still edit the notes on the release page afterwards.

## Gaps

- Only the **Release** configuration is built (issue #26 asked for Debug too).
- No compiler warnings-as-errors, static analysis or formatting check.
- Artifacts are **not code-signed**.
- The MSIX package itself is not built in CI; it is built on the user's machine by `install.bat`.

Suggestions are listed in [Project review](../development/project-review.md).
