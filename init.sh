#!/usr/bin/env bash
set -euo pipefail

# ============================================================
# GitHub Pro Roadmap Bootstrap Script
# Project: win11-quick-converter
# ============================================================

# ---- Safety checks -----------------------------------------------------------
if ! command -v gh >/dev/null 2>&1; then
  echo "Error: GitHub CLI (gh) is not installed."
  exit 1
fi

REPO="$(gh repo view --json nameWithOwner -q .nameWithOwner)"
OWNER="$(gh repo view --json owner -q .owner.login)"
if [[ -z "${REPO}" ]]; then
  echo "Error: Could not detect repository from current directory."
  exit 1
fi

echo "🚀 Target repository: ${REPO}"

# ---- Create Kanban Project (GitHub Projects V2) ------------------------------
echo "📊 Setting up modern Kanban board..."
PROJECT_ID="$(gh project create --owner "@me" --title "Win11 Quick Converter - Kanban" --format json --jq '.id' 2>/dev/null || true)"

if [[ -n "${PROJECT_ID}" ]]; then
    # Link the project to the current repository
    gh project item-add "${PROJECT_ID}" --owner "@me" --repo "${REPO}" 2>/dev/null || true
    PROJECT_URL="$(gh project view "${PROJECT_ID}" --owner "@me" --format json --jq '.url')"
    echo "✅ Kanban board created successfully at: ${PROJECT_URL}"
else
    echo "⚠️  Project board might already exist or needs manual creation via web."
fi

# ---- Helper functions --------------------------------------------------------
ensure_milestone() {
  local title="$1"
  local description="$2"

  local existing_number
  existing_number="$(gh api "repos/${REPO}/milestones?state=all&per_page=100" --jq ".[] | select(.title == \"${title}\") | .number" | head -n1 || true)"

  if [[ -n "${existing_number}" ]]; then
    echo "🎯 Milestone already exists: ${title} (#${existing_number})"
  else
    gh api -X POST "repos/${REPO}/milestones" \
      -f title="${title}" \
      -f description="${description}" >/dev/null
    echo "🎯 Milestone created: ${title}"
  fi
}

ensure_label() {
  local name="$1"
  local color="$2"
  local description="$3"

  gh label create "${name}" \
    --color "${color}" \
    --description "${description}" \
    --force >/dev/null

  echo "🏷️  Label ensured: ${name}"
}

create_issue() {
  local title="$1"
  local milestone="$2"
  local body="$3"
  shift 3

  local label_args=()
  local label
  for label in "$@"; do
    label_args+=(--label "${label}")
  done

  gh issue create \
    --title "${title}" \
    --body "${body}" \
    --milestone "${milestone}" \
    "${label_args[@]}" >/dev/null

  echo "✅ Issue created: ${title}"
}

# ---- Milestones --------------------------------------------------------------
ensure_milestone "M1: Conversion Engine (CLI Core)" "Build a production-grade CLI conversion engine with reliable format handling."
ensure_milestone "M2: Native Win11 Extension" "Implement a native COM shell extension (IExplorerCommand) for context menu integration."
ensure_milestone "M3: Integration & Packaging" "Integrate CLI and shell extension end-to-end and deliver MSIX deployment."
ensure_milestone "M4: CI/CD & Automation" "Establish automated build, quality gates, and release pipelines."

# ---- Labels (Types, Core, Priority, Size) ------------------------------------
# Technical domains
ensure_label "core-logic" "0E8A16" "Core conversion engine logic."
ensure_label "windows-api" "1D76DB" "Windows Shell, COM, Registry API work."
ensure_label "msix" "FBCA04" "MSIX, AppxManifest, deployment."
ensure_label "ci-cd" "5319E7" "Build automation & pipelines."

# Priorities
ensure_label "priority: high" "B60205" "Critical path task. Blocks other work."
ensure_label "priority: medium" "D93F0B" "Important, but non-blocking."
ensure_label "priority: low" "F9D0C4" "Nice to have, can be deferred."

# Sizes (T-Shirt)
ensure_label "size: S" "7FDEC6" "Quick task (< 2 hours)."
ensure_label "size: M" "23A482" "Standard task (half day)."
ensure_label "size: L" "0B624A" "Complex task (1-2 days)."

# Types
ensure_label "type: feature" "C2E0C6" "New functionality."
ensure_label "type: docs" "0075CA" "Documentation and architecture records."

echo "⏳ Generating issues (this takes a moment to avoid API rate limits)..."

# ---- Issues: Milestone 1 -----------------------------------------------------
create_issue \
  "Define CLI architecture and command contract" \
  "M1: Conversion Engine (CLI Core)" \
"## Technical Context
Design the robust CLI structure, command syntax, and argument model for the conversion engine.

## Acceptance Criteria
- [ ] Command grammar and argument precedence rules are documented.
- [ ] Supported input/output format matrix is defined.
- [ ] Exit code taxonomy (success, validation, runtime failures) is established.
- [ ] Architecture Decision Record (ADR) added to `/docs`.
" \
  "core-logic" "type: docs" "priority: high" "size: M"

create_issue \
  "Implement WIC-based conversion pipeline" \
  "M1: Conversion Engine (CLI Core)" \
"## Technical Context
Build the core conversion flow using Windows Imaging Component (WIC), focusing on C++ memory safety and performance.

## Acceptance Criteria
- [ ] Input file probing and codec resolution implemented.
- [ ] Decode-transform-encode pipeline handles streaming safely.
- [ ] Output options configured (quality, compression).
- [ ] Baseline integration tests pass for JPG to PNG.
" \
  "core-logic" "windows-api" "type: feature" "priority: high" "size: L"

sleep 2

# ---- Issues: Milestone 2 -----------------------------------------------------
create_issue \
  "Scaffold COM in-proc server & IExplorerCommand" \
  "M2: Native Win11 Extension" \
"## Technical Context
Create the native shell extension foundation as an in-proc COM server.

## Acceptance Criteria
- [ ] COM class structure and CLSID map defined.
- [ ] Initial IExplorerCommand interface stubbed out.
- [ ] Registration metadata configured.
- [ ] DLL successfully loads in Explorer process boundaries without crashing.
" \
  "windows-api" "type: feature" "priority: high" "size: L"

create_issue \
  "Implement context menu behavior and selection filtering" \
  "M2: Native Win11 Extension" \
"## Technical Context
Handle verb visibility rules so the option only appears for valid image files.

## Acceptance Criteria
- [ ] Selection inspection implemented for supported file extensions.
- [ ] Visibility rules applied dynamically.
- [ ] Canonical verb naming and localized labels added.
" \
  "windows-api" "type: feature" "priority: medium" "size: M"

sleep 2

# ---- Issues: Milestone 3 -----------------------------------------------------
create_issue \
  "Integrate COM invocation with CLI execution" \
  "M3: Integration & Packaging" \
"## Technical Context
Connect the Explorer right-click command to the actual CLI engine.

## Acceptance Criteria
- [ ] Sub-process invocation configured securely.
- [ ] Safe path quoting and Unicode argument handling implemented.
- [ ] End-to-end test: Right click -> Convert -> File appears.
" \
  "core-logic" "windows-api" "priority: high" "size: M"

create_issue \
  "Author MSIX Sparse Package (AppxManifest)" \
  "M3: Integration & Packaging" \
"## Technical Context
Package the extension for modern Windows 11 deployment.

## Acceptance Criteria
- [ ] AppxManifest.xml created with correct shell command extensions.
- [ ] Sparse package mapping configured to external binary locations.
- [ ] Installation flow validated locally.
" \
  "msix" "windows-api" "priority: high" "size: L"

sleep 2

# ---- Issues: Milestone 4 -----------------------------------------------------
create_issue \
  "Implement GitHub Actions C++ build workflow" \
  "M4: CI/CD & Automation" \
"## Technical Context
Automate the build process for the CLI and DLL binaries using MSBuild/CMake.

## Acceptance Criteria
- [ ] Windows runner configured with required SDKs.
- [ ] Pipeline builds both Release and Debug configurations.
- [ ] Workflow triggers correctly on PR and main branch pushes.
" \
  "ci-cd" "priority: medium" "size: M"

echo "🎉 Pro Roadmap bootstrap completed successfully!"