#pragma once

#include <Windows.h>

#include <string>

namespace win11qc::shell {

// AppUserModelID used for toasts when the extension runs without package
// identity (HKCU / DllRegisterServer install). Registered under
// HKCU\Software\Classes\AppUserModelId\<id> by DllRegisterServer.
inline constexpr wchar_t kToastAppId[] = L"SnapFormat.ImageConverter";

// Shows a toast built from `xml`. Uses the package identity when the process
// has one (MSIX install, COM surrogate), otherwise kToastAppId. Must be called
// on a thread with COM initialized. Failures are returned, never thrown.
HRESULT show_toast(const std::wstring& xml);

} // namespace win11qc::shell
