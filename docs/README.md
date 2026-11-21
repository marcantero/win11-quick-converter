# SnapFormat Documentation

Welcome to the documentation for **SnapFormat** (repository: `win11-quick-converter`), a native Windows 11 File Explorer context-menu extension for instant image conversion.

## Start here

| If you want to… | Read |
|---|---|
| Understand what SnapFormat does | [Product overview](overview.md) |
| Install it and use it | [Installation](guides/installation.md) → [Usage](guides/usage.md) |
| Build it from source | [Building from source](guides/building.md) |
| Fix something that doesn't work | [Troubleshooting](guides/troubleshooting.md) |
| Understand how it works internally | [Architecture overview](architecture/overview.md) |
| Contribute code | [Contributing](development/contributing.md) |

## Contents

### Product
- [Product overview](overview.md) — features, supported formats, requirements.
- [Roadmap](roadmap.md) — delivered milestones, open feature requests, releases.

### User guides
- [Installation](guides/installation.md) — Inno Setup installer and MSIX sparse package.
- [Usage](guides/usage.md) — context menu, batch conversion, output naming rules.
- [Troubleshooting](guides/troubleshooting.md) — common problems, logs, diagnostics.
- [Building from source](guides/building.md) — toolchain, CMake, build outputs.

### Architecture
- [Architecture overview](architecture/overview.md) — components, boundaries, end-to-end flow.
- [CLI contract](architecture/cli-contract.md) — `converter-cli.exe` options and exit codes.
- [Conversion pipeline](architecture/conversion-pipeline.md) — WIC and libwebp encoding paths.
- [Shell extension](architecture/shell-extension.md) — COM server, `IExplorerCommand`, registration.

### Deployment
- [Packaging](deployment/packaging.md) — Inno Setup script, AppxManifest, signing certificate.
- [CI/CD](deployment/ci-cd.md) — GitHub Actions build and release pipeline.

### Development
- [Contributing](development/contributing.md) — repository layout, coding rules, branch workflow.
- [Testing](development/testing.md) — manual test plan and the `shell_ext_test_com` harness.
- [Project review](development/project-review.md) — known issues, risks and recommended improvements.

### Architecture Decision Records
- [ADR index](adr/README.md)
