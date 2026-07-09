#define NOMINMAX

#include "win11qc/shell_extension.h"

#include <Windows.h>
#include <ShObjIdl_core.h>
#include <strsafe.h>

#include <atomic>
#include <string>

#include <wrl/client.h>
#include <wrl/implements.h>

namespace win11qc::shell {
namespace {

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

class UniqueRegKey final {
public:
	UniqueRegKey() = default;
	~UniqueRegKey()
	{
		reset();
	}

	UniqueRegKey(const UniqueRegKey&) = delete;
	UniqueRegKey& operator=(const UniqueRegKey&) = delete;

	HKEY* put()
	{
		reset();
		return &key_;
	}

	HKEY get() const noexcept
	{
		return key_;
	}

	void reset(HKEY key = nullptr) noexcept
	{
		if (key_ != nullptr) {
			RegCloseKey(key_);
		}

		key_ = key;
	}

private:
	HKEY key_ = nullptr;
};

std::atomic<ULONG> g_objectCount{0};
std::atomic<ULONG> g_lockCount{0};
HINSTANCE g_moduleHandle = nullptr;

void set_module_handle(HINSTANCE instance)
{
	g_moduleHandle = instance;
}

BOOL can_unload_now()
{
	return g_objectCount.load(std::memory_order_relaxed) == 0 &&
		g_lockCount.load(std::memory_order_relaxed) == 0;
}

std::wstring guid_to_string(REFGUID guid)
{
	wchar_t buffer[64] = {};
	const int length = StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer)));
	if (length <= 0) {
		return {};
	}

	return buffer;
}

HRESULT write_reg_string(HKEY root, const wchar_t* subkey, const wchar_t* valueName, const std::wstring& value)
{
	UniqueRegKey key;
	DWORD disposition = 0;
	const LONG createResult = RegCreateKeyExW(
		root,
		subkey,
		0,
		nullptr,
		REG_OPTION_NON_VOLATILE,
		KEY_SET_VALUE,
		nullptr,
		key.put(),
		&disposition);
	if (createResult != ERROR_SUCCESS) {
		return HRESULT_FROM_WIN32(createResult);
	}

	const BYTE* data = reinterpret_cast<const BYTE*>(value.c_str());
	const DWORD dataSize = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
	const LONG setResult = RegSetValueExW(key.get(), valueName, 0, REG_SZ, data, dataSize);
	if (setResult != ERROR_SUCCESS) {
		return HRESULT_FROM_WIN32(setResult);
	}

	return S_OK;
}

HRESULT delete_reg_tree(HKEY root, const wchar_t* subkey)
{
	const LONG deleteResult = RegDeleteTreeW(root, subkey);
	if (deleteResult == ERROR_FILE_NOT_FOUND) {
		return S_OK;
	}

	if (deleteResult != ERROR_SUCCESS) {
		return HRESULT_FROM_WIN32(deleteResult);
	}

	return S_OK;
}

std::wstring get_module_path()
{
	wchar_t buffer[MAX_PATH] = {};
	const DWORD length = GetModuleFileNameW(g_moduleHandle, buffer, static_cast<DWORD>(std::size(buffer)));
	if (length == 0 || length >= std::size(buffer)) {
		return {};
	}

	return buffer;
}

HRESULT duplicate_string(const wchar_t* source, LPWSTR* destination)
{
	if (destination == nullptr) {
		return E_POINTER;
	}

	*destination = nullptr;

	if (source == nullptr) {
		return E_INVALIDARG;
	}

	const size_t characters = wcslen(source) + 1;
	auto* buffer = static_cast<wchar_t*>(CoTaskMemAlloc(characters * sizeof(wchar_t)));
	if (buffer == nullptr) {
		return E_OUTOFMEMORY;
	}

	HRESULT copyResult = StringCchCopyW(buffer, characters, source);
	if (FAILED(copyResult)) {
		CoTaskMemFree(buffer);
		return copyResult;
	}

	*destination = buffer;
	return S_OK;
}

HRESULT register_shell_metadata(const std::wstring& modulePath)
{
	const std::wstring clsid = guid_to_string(kShellExtensionClsid);
	if (clsid.empty()) {
		return E_FAIL;
	}

	const std::wstring clsidRoot = L"Software\\Classes\\CLSID\\" + clsid;
	const std::wstring inprocKey = clsidRoot + L"\\InprocServer32";
	const std::wstring shellVerbKey = L"Software\\Classes\\SystemFileAssociations\\image\\shell\\" + std::wstring{kCommandVerb};

	HRESULT hr = write_reg_string(HKEY_CURRENT_USER, clsidRoot.c_str(), nullptr, kCommandTitle);
	if (FAILED(hr)) {
		return hr;
	}

	hr = write_reg_string(HKEY_CURRENT_USER, inprocKey.c_str(), nullptr, modulePath);
	if (FAILED(hr)) {
		return hr;
	}

	hr = write_reg_string(HKEY_CURRENT_USER, inprocKey.c_str(), L"ThreadingModel", L"Apartment");
	if (FAILED(hr)) {
		return hr;
	}

	hr = write_reg_string(HKEY_CURRENT_USER, shellVerbKey.c_str(), nullptr, kCommandTitle);
	if (FAILED(hr)) {
		return hr;
	}

	hr = write_reg_string(HKEY_CURRENT_USER, shellVerbKey.c_str(), L"MUIVerb", kCommandTitle);
	if (FAILED(hr)) {
		return hr;
	}

	hr = write_reg_string(HKEY_CURRENT_USER, shellVerbKey.c_str(), L"Icon", modulePath);
	if (FAILED(hr)) {
		return hr;
	}

	return write_reg_string(HKEY_CURRENT_USER, shellVerbKey.c_str(), L"ExplorerCommandHandler", clsid);
}

HRESULT unregister_shell_metadata()
{
	const std::wstring clsid = guid_to_string(kShellExtensionClsid);
	if (clsid.empty()) {
		return E_FAIL;
	}

	const std::wstring clsidRoot = L"Software\\Classes\\CLSID\\" + clsid;
	const std::wstring shellVerbKey = L"Software\\Classes\\SystemFileAssociations\\image\\shell\\" + std::wstring{kCommandVerb};

	HRESULT hr = delete_reg_tree(HKEY_CURRENT_USER, shellVerbKey.c_str());
	if (FAILED(hr)) {
		return hr;
	}

	return delete_reg_tree(HKEY_CURRENT_USER, clsidRoot.c_str());
}

class QuickConvertCommand final
	: public RuntimeClass<RuntimeClassFlags<ClassicCom>, IExplorerCommand> {
public:
	QuickConvertCommand()
	{
		g_objectCount.fetch_add(1, std::memory_order_relaxed);
	}

	~QuickConvertCommand() override
	{
		g_objectCount.fetch_sub(1, std::memory_order_relaxed);
	}

	IFACEMETHODIMP GetTitle(IShellItemArray*, LPWSTR* name) override
	{
		return duplicate_string(kCommandTitle, name);
	}

	IFACEMETHODIMP GetIcon(IShellItemArray*, LPWSTR* icon) override
	{
		const std::wstring modulePath = get_module_path();
		if (modulePath.empty()) {
			return E_FAIL;
		}

		return duplicate_string(modulePath.c_str(), icon);
	}

	IFACEMETHODIMP GetCanonicalName(GUID* commandName) override
	{
		if (commandName == nullptr) {
			return E_POINTER;
		}

		*commandName = kShellExtensionClsid;
		return S_OK;
	}

	IFACEMETHODIMP GetToolTip(IShellItemArray*, LPWSTR* tooltip) override
	{
		return duplicate_string(kCommandDescription, tooltip);
	}

	IFACEMETHODIMP GetState(IShellItemArray*, BOOL, EXPCMDSTATE* state) override
	{
		if (state == nullptr) {
			return E_POINTER;
		}

		*state = ECS_ENABLED;
		return S_OK;
	}

	IFACEMETHODIMP Invoke(IShellItemArray*, IBindCtx*) override
	{
		OutputDebugStringW(L"win11-quick-converter shell command invoked.\n");
		return S_OK;
	}

	IFACEMETHODIMP GetFlags(EXPCMDFLAGS* flags) override
	{
		if (flags == nullptr) {
			return E_POINTER;
		}

		*flags = ECF_DEFAULT;
		return S_OK;
	}

	IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** enumCommands) override
	{
		if (enumCommands == nullptr) {
			return E_POINTER;
		}

		*enumCommands = nullptr;
		return E_NOTIMPL;
	}
};

class QuickConvertClassFactory final
	: public RuntimeClass<RuntimeClassFlags<ClassicCom>, IClassFactory> {
public:
	QuickConvertClassFactory()
	{
		g_objectCount.fetch_add(1, std::memory_order_relaxed);
	}

	~QuickConvertClassFactory() override
	{
		g_objectCount.fetch_sub(1, std::memory_order_relaxed);
	}

	IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** object) override
	{
		if (object == nullptr) {
			return E_POINTER;
		}

		*object = nullptr;

		if (outer != nullptr) {
			return CLASS_E_NOAGGREGATION;
		}

		ComPtr<QuickConvertCommand> command = Make<QuickConvertCommand>();
		if (!command) {
			return E_OUTOFMEMORY;
		}

		return command.CopyTo(riid, object);
	}

	IFACEMETHODIMP LockServer(BOOL lock) override
	{
		if (lock) {
			g_lockCount.fetch_add(1, std::memory_order_relaxed);
		} else {
			g_lockCount.fetch_sub(1, std::memory_order_relaxed);
		}

		return S_OK;
	}
};

HRESULT get_class_object(REFIID riid, void** object)
{
	ComPtr<QuickConvertClassFactory> factory = Make<QuickConvertClassFactory>();
	if (!factory) {
		return E_OUTOFMEMORY;
	}

	return factory.CopyTo(riid, object);
}

} // namespace

} // namespace win11qc::shell

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH) {
		win11qc::shell::set_module_handle(instance);
		DisableThreadLibraryCalls(instance);
	}

	return TRUE;
}

extern "C" HRESULT STDAPICALLTYPE DllCanUnloadNow(void)
{
	if (win11qc::shell::can_unload_now()) {
		return S_OK;
	}

	return S_FALSE;
}

extern "C" HRESULT STDAPICALLTYPE DllGetClassObject(REFCLSID clsid, REFIID riid, void** object)
{
	if (object == nullptr) {
		return E_POINTER;
	}

	*object = nullptr;

	if (clsid != win11qc::shell::kShellExtensionClsid) {
		return CLASS_E_CLASSNOTAVAILABLE;
	}

	return win11qc::shell::get_class_object(riid, object);
}

extern "C" HRESULT STDAPICALLTYPE DllRegisterServer(void)
{
	const std::wstring modulePath = win11qc::shell::get_module_path();
	if (modulePath.empty()) {
		return E_FAIL;
	}

	return win11qc::shell::register_shell_metadata(modulePath);
}

extern "C" HRESULT STDAPICALLTYPE DllUnregisterServer(void)
{
	return win11qc::shell::unregister_shell_metadata();
}

