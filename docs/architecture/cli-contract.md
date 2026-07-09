# CLI Contract

## Purpose

The CLI is the headless conversion engine for win11-quick-converter.
It will be invoked directly by the shell extension and must remain testable without Explorer.

## Supported options

- `--input <path>`: Source image file.
- `--output <path>`: Destination file path.
- `--format <name>`: Target output format, such as `png`, `jpg`, or `bmp`.
- `--quality <0-100>`: Optional codec quality parameter.
- `--help`: Display usage information.

## Exit codes

- `0`: Success.
- `1`: Invalid arguments or unsupported command usage.
- `2`: Input validation failed.
- `3`: Conversion failed.

## Notes

This document defines the initial command contract and will evolve as the conversion pipeline is implemented.
