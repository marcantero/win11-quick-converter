#pragma once

#include <guiddef.h>

namespace win11qc::shell {

inline constexpr CLSID kShellExtensionClsid{
    0x8d5f6d5b,
    0x18cb,
    0x4c7b,
    {0x9e, 0x2d, 0x6d, 0x8b, 0x12, 0x5f, 0x55, 0x1c}
};

inline constexpr wchar_t kCommandVerb[] = L"win11qc.convert";
inline constexpr wchar_t kCommandTitle[] = L"Convert with win11-quick-converter";
inline constexpr wchar_t kCommandDescription[] = L"Convert selected images using the win11-quick-converter CLI.";

} // namespace win11qc::shell
