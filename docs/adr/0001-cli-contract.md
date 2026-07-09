# ADR 0001: CLI Contract for Image Conversion

## Status

Accepted

## Context

The shell extension must invoke a headless, predictable CLI entry point for image conversion.
The command line contract needs to be stable enough for the COM layer to pass file paths and output preferences without depending on Explorer UI state.

## Decision

The CLI exposes a small explicit contract:

- `--input <path>` for the source image.
- `--output <path>` for the destination file.
- `--format <name>` for the target container format.
- `--quality <0-100>` as an optional codec tuning parameter.
- `--help` for usage discovery.

The executable returns deterministic exit codes for argument validation, input validation, and conversion failures.

## Consequences

- The CLI can be invoked from the shell extension without additional interactive prompts.
- The contract can be tested independently from Explorer.
- Future formats and options must preserve backward compatibility unless a versioned contract is introduced.
