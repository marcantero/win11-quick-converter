# AGENTS.md

Rules for AI coding agents (Claude Code, Copilot, Codex, …) working on this repository.
Humans follow the same rules; see also [docs/development/contributing.md](docs/development/contributing.md).

## Project in one paragraph

SnapFormat adds a **Convert with SnapFormat** entry to the Windows 11 context menu.
It has two components that must stay separate:

- `src/converter-cli/`: a headless C++20 CLI that converts images with WIC and libwebp. No UI or shell code.
- `src/shell-ext/`: a COM DLL (`IExplorerCommand`) that builds the menu and spawns the CLI. No image processing.

Packaging lives in `packaging/` (Inno Setup installer and MSIX sparse package). Docs live in `docs/`.

## Git workflow

### One issue, one branch

1. Before starting, update `main` and clean up: `git switch main && git pull --ff-only && git fetch --prune`.
2. Create a new branch from `main` for **every issue**. Never commit or push directly to `main`.
3. Name it `<type>/<issue>-<short-slug>`, with the type taken from the issue's `type:` label:

   | Label | Prefix | Example |
   |---|---|---|
   | `type: bug` | `fix/` | `fix/47-webp-hresults` |
   | `type: feature` | `feat/` | `feat/52-parallel-batch` |
   | `type: chore` | `chore/` | `chore/54-single-version` |
   | `type: docs` | `docs/` | `docs/60-usage-guide` |
   | `type: security` | `fix/` | `fix/55-code-signing` |
   | area `ci` | `ci/` | `ci/49-cli-tests` |

4. Push only to that branch. If the issue is on the GitHub project board, move it to **In progress** when you start and to **Done** after merging.
5. One issue per branch. If an issue turns out to cover unrelated work, split it into a new issue and branch.
6. Changes without an issue (typos, small doc fixes) also go through a branch, without a number: `docs/fix-typo-readme`.

### Commits

- Use [Conventional Commits](https://www.conventionalcommits.org/) with a scope: `fix(cli): …`, `feat(shell-ext): …`, `chore(packaging): …`, `docs: …`.
- Keep each commit focused on one change, and explain *why* in the body when it is not obvious.
- The commit that completes an issue includes `Fixes #N`.

### Pull requests and merging

1. Open a pull request to `main` with `gh pr create`. Put `Fixes #N` in the description so the issue closes on merge.
2. Wait for CI to pass (`gh pr checks --watch`). Doc-only changes skip CI.
3. The agent merges the PR itself once CI is green: `gh pr merge --squash --delete-branch`.
4. If CI fails, fix it on the same branch. Never merge a red PR, and never bypass checks or hooks.

### No branches for finished work

A branch exists only while its issue is open and being worked on.

- `gh pr merge --delete-branch` deletes the remote branch and the local one.
- If an issue is closed without merging (won't fix, duplicate), delete its branch locally (`git branch -D`) and on the remote (`git push origin --delete <branch>`).
- At the start of each task, list branches (`git branch -a`). Delete any whose issue is closed and whose commits are already in `main` (`git branch -d` refuses to delete unmerged work). Ask before deleting a branch that has unmerged commits.

### Never

- Force-push to `main`, or rewrite history that is already on `main`.
- Commit build outputs (`build/`), `.msix` packages, certificates or private keys (`*.pfx`, `*.p12`, `*.cer`).
- Leave the repository on a branch other than `main` when a task is finished.

## Coding rules

1. **Modern C++20.** Use RAII and smart pointers; no raw `new`/`delete`.
2. **COM.** Always use `Microsoft::WRL::ComPtr`, and check every `HRESULT`. Error messages include the `HRESULT`.
3. **Architecture boundary.** Keep menu or UI logic out of the CLI, and image processing out of the DLL.
4. **Validate before acting.** Produce clear diagnostics, and never leave a partial output file behind.
5. **English only** for code, comments, identifiers, docs and commit messages.
6. **Builds must be warning-free** (`/W4 /permissive-`). Do not add new warnings.
7. **Style:** namespaces `win11qc` (CLI) and `win11qc::shell` (DLL); helpers in an anonymous namespace; `snake_case` functions, `PascalCase` types, `camelCase` locals, `k` prefix for constants.
8. A new output format goes in **both** the CLI and the menu. A new input extension also goes in the registry registration and in `packaging/AppxManifest.xml`.

## Build and test

CMake may not be on `PATH`; Visual Studio bundles it under
`<VS install>\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\` (find VS with `vswhere`).

```powershell
cmake -S . -B build                          # first time only (downloads libwebp)
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

- Run the build and `ctest` before every push. All tests must pass.
- Every bug fix comes with a regression test when it can be tested. CLI tests are PowerShell scripts in `src/converter-cli/tests/`, registered in `src/converter-cli/CMakeLists.txt`.
- Explorer behaviour can only be checked manually. `scripts/dev-install.ps1` installs the local build (and restarts Explorer), and `docs/development/testing.md` has the manual test plan. Do not run it without asking the user, because it closes Explorer.
- Packaging changes (`installer.iss`) cannot be built without Inno Setup. Say so when they are not verified.

## Documentation

- Update the relevant page in `docs/` in the same branch as the code change.
- When an issue from `docs/development/project-review.md` is fixed, strike it through there and link the issue.
- Record significant, hard-to-reverse decisions as an ADR in `docs/adr/`.

## Reporting back

When a task is finished, report the issue, the PR link, whether CI passed, and anything not verified, such as manual Explorer tests or installer builds.
