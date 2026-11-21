# Contributing

## Repository layout

See [Architecture overview](../architecture/overview.md#architecture-boundary) for the full tree. Short version:

| Path | What lives there |
|---|---|
| `src/converter-cli/` | Conversion engine (no UI, no shell code) |
| `src/shell-ext/` | COM shell extension (no image processing) |
| `src/shell-ext/test_com/` | COM activation smoke test |
| `packaging/` | Installer script, MSIX manifest, registration script |
| `.github/workflows/` | CI |
| `AGENTS.md` | Rules for AI coding agents (`CLAUDE.md` imports it) |
| `docs/` | Documentation and ADRs |
| `init.sh` | One-off script that bootstrapped milestones, labels and issues on GitHub |

## Coding rules

These come from [`AGENTS.md`](../../AGENTS.md) and apply to humans and AI assistants alike:

1. **Modern C++ (C++20)** — RAII, smart pointers, no raw `new`/`delete`.
2. **COM** — always `Microsoft::WRL::ComPtr`; check every `HRESULT`.
3. **Architecture boundary** — no UI/menu logic in the CLI; the DLL only builds the menu and spawns the CLI.
4. **Error handling** — validate before acting; produce clear diagnostic messages.
5. **Language** — code, comments, identifiers and documentation in **English**.

Style observed in the code base:

- Namespace `win11qc` (CLI) and `win11qc::shell` (DLL); internal helpers in an anonymous namespace.
- `snake_case` functions, `PascalCase` types, `camelCase` locals, trailing `_` for members, `k` prefix for constants.
- Warnings level `/W4` — keep builds warning-free.

## Workflow

The full rules are in [`AGENTS.md`](../../AGENTS.md#git-workflow). In short:

1. Pick or open a GitHub issue; move it to **In progress** on the project board.
2. Create a new branch from `main` for that issue, named `<type>/<issue>-<slug>` (e.g. `fix/47-webp-hresults`). Never push to `main` directly.
3. Make focused commits using [Conventional Commits](https://www.conventionalcommits.org/), e.g. `feat(shell-ext): …`, `fix(cli): …`, `docs: …`; reference the issue with `Fixes #NN`.
4. Push the branch and open a pull request to `main` with the same `type:` label as the issue (`gh pr create --label "type: feature"`); the release notes are grouped by it. CI must pass.
5. Squash-merge and delete the branch (`gh pr merge --squash --delete-branch`). No branch may outlive its finished issue.
6. Move the issue to **Done**.

Releases are made from GitHub Actions, without editing any version number: see [CI/CD → Making a release](../deployment/ci-cd.md#making-a-release).

## Documentation

- Update the relevant page in `docs/` in the same PR as the code change (CLI options, formats, registry keys, etc.).
- Record significant, hard-to-reverse decisions as a new ADR in `docs/adr/` using the existing template (Status / Context / Decision / Consequences), numbered sequentially.

## Checklist before opening a PR

- [ ] `cmake --build build --config Release` succeeds with no new warnings.
- [ ] Manual test plan items relevant to the change pass ([Testing](testing.md)).
- [ ] Docs updated.
- [ ] New formats are added in **both** the CLI and the menu (and the MSIX manifest if it's a new input extension).
