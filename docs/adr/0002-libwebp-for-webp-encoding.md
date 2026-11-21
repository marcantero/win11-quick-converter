# ADR 0002: Use libwebp for WebP Encoding

## Status

Accepted — recorded retroactively from commits `929ef70` and `9ca0ced`.

## Context

WebP output was a frequently requested format. Windows ships a WIC WebP **decoder**, but no WebP **encoder**; `CreateEncoder(GUID_ContainerFormatWebp)` fails on stock Windows 11. A first attempt relying on WIC did not produce WebP files reliably, and the "To WebP" menu entry had to be removed until a working encoder was available.

## Decision

- Fetch Google's **libwebp v1.4.0** with CMake `FetchContent` and link it statically into `converter-cli.exe`, with all libwebp command-line tools disabled.
- Keep decoding in WIC (for every input format), convert the frame to 32bpp RGBA, and encode with `WebPEncodeRGBA` (lossy, when `--quality < 100`) or `WebPEncodeLosslessRGBA` (default).
- Restore the "To WebP" context-menu entry.

## Consequences

- WebP output works on every Windows 11 machine without extra codecs.
- The CLI binary grows slightly and the first configure needs network access to GitHub.
- WebP uses a separate code path (`convert_to_webp`) that must be kept consistent with the WIC path's validation and error handling.
- Default WebP output from the menu is lossless, which can be larger than the source for photos; a settings UI (#36) is the planned way to expose quality.
