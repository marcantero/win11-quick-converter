#define NOMINMAX

#include "win11qc/shell_extension.h"

#include <Windows.h>
#include <ShlObj.h>
#include <ShObjIdl_core.h>
#include <strsafe.h>

#include <atomic>
#include <string>

#include <wrl/client.h>
#include <wrl/implements.h>
#include <filesystem>
#include <vector>

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
		KEY_WRITE,
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

	hr = write_reg_string(HKEY_CURRENT_USER, shellVerbKey.c_str(), L"ExplorerCommandHandler", clsid);
	if (FAILED(hr)) {
		return hr;
	}

	const std::wstring specificExts[] = { L".webp", L".heic", L".avif" };
	for (const auto& ext : specificExts) {
		const std::wstring verbKey = L"Software\\Classes\\SystemFileAssociations\\" + ext + L"\\shell\\" + std::wstring{kCommandVerb};
		hr = write_reg_string(HKEY_CURRENT_USER, verbKey.c_str(), nullptr, kCommandTitle);
		if (FAILED(hr)) return hr;
		hr = write_reg_string(HKEY_CURRENT_USER, verbKey.c_str(), L"MUIVerb", kCommandTitle);
		if (FAILED(hr)) return hr;
		hr = write_reg_string(HKEY_CURRENT_USER, verbKey.c_str(), L"Icon", modulePath);
		if (FAILED(hr)) return hr;
		hr = write_reg_string(HKEY_CURRENT_USER, verbKey.c_str(), L"ExplorerCommandHandler", clsid);
		if (FAILED(hr)) return hr;
	}

	return S_OK;
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

	const std::wstring specificExts[] = { L".webp", L".heic", L".avif" };
	for (const auto& ext : specificExts) {
		const std::wstring verbKey = L"Software\\Classes\\SystemFileAssociations\\" + ext + L"\\shell\\" + std::wstring{kCommandVerb};
		hr = delete_reg_tree(HKEY_CURRENT_USER, verbKey.c_str());
		if (FAILED(hr)) return hr;
	}

	return delete_reg_tree(HKEY_CURRENT_USER, clsidRoot.c_str());
}

// -------------------------------------------------------------
// 1. Classe per a cada opció del submenú (PNG, JPG, etc.)
// -------------------------------------------------------------
class FormatSubCommand final : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IExplorerCommand> {
	std::wstring formatExt_;
	std::wstring title_;
public:
	FormatSubCommand(std::wstring formatExt, std::wstring title) 
		: formatExt_(std::move(formatExt)), title_(std::move(title)) {
		g_objectCount.fetch_add(1, std::memory_order_relaxed);
	}
	~FormatSubCommand() override {
		g_objectCount.fetch_sub(1, std::memory_order_relaxed);
	}

	IFACEMETHODIMP GetTitle(IShellItemArray*, LPWSTR* name) override { return duplicate_string(title_.c_str(), name); }
	IFACEMETHODIMP GetIcon(IShellItemArray*, LPWSTR* /*icon*/) override { return E_NOTIMPL; /* Sense icona als fills queda més net */ }
	IFACEMETHODIMP GetToolTip(IShellItemArray*, LPWSTR* tooltip) override { return duplicate_string(title_.c_str(), tooltip); }
	IFACEMETHODIMP GetCanonicalName(GUID* commandName) override { 
		if (!commandName) return E_POINTER;
		*commandName = GUID_NULL; 
		return S_OK; 
	}
	IFACEMETHODIMP GetState(IShellItemArray*, BOOL, EXPCMDSTATE* state) override {
		if (!state) return E_POINTER;
		*state = ECS_ENABLED; 
		return S_OK;
	}
	IFACEMETHODIMP GetFlags(EXPCMDFLAGS* flags) override {
		if (!flags) return E_POINTER;
		*flags = ECF_DEFAULT; return S_OK;
	}
	IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** enumCommands) override {
		if (!enumCommands) return E_POINTER;
		*enumCommands = nullptr; return E_NOTIMPL;
	}

	// Tota la lògica màgica de conversió ara la fa el fill!
	IFACEMETHODIMP Invoke(IShellItemArray* items, IBindCtx* /*bindCtx*/) override {
		if (items == nullptr) return E_POINTER;
		DWORD count = 0;
		HRESULT hr = items->GetCount(&count);
		if (FAILED(hr) || count == 0) return hr;

		const std::wstring modulePath = get_module_path();
		if (modulePath.empty()) return E_FAIL;
		std::filesystem::path cliPath = std::filesystem::path(modulePath).remove_filename() / L"converter-cli.exe";
		
		std::error_code pathError;
		if (!std::filesystem::exists(cliPath, pathError)) return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

		HRESULT launchResult = S_OK;
		for (DWORD i = 0; i < count; ++i) {
			ComPtr<IShellItem> item;
			if (FAILED(items->GetItemAt(i, &item)) || !item) continue;
			LPWSTR filePath = nullptr;
			if (FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, &filePath)) || !filePath) continue;

			std::filesystem::path inputPath{filePath};
			CoTaskMemFree(filePath);
			std::filesystem::path outputPath = inputPath;
			
			// Apliquem l'extensió específica del botó que hem clicat
			outputPath.replace_extension(L"." + formatExt_);
			std::wstring commandLine = L"\"" + cliPath.wstring() + L"\" --input \"" + inputPath.wstring() + L"\" --output \"" + outputPath.wstring() + L"\" --format " + formatExt_;

			std::vector<wchar_t> cmdVec(commandLine.begin(), commandLine.end());
			cmdVec.push_back(0);
			STARTUPINFOW si{ sizeof(si) };
			PROCESS_INFORMATION pi{};
			if (CreateProcessW(nullptr, cmdVec.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
				CloseHandle(pi.hThread);
				CloseHandle(pi.hProcess);
			} else if (SUCCEEDED(launchResult)) {
				launchResult = HRESULT_FROM_WIN32(GetLastError());
			}
		}
		return launchResult;
	}
};

// -------------------------------------------------------------
// 2. Classe enumeradora (L'engranatge que mostra la llista)
// -------------------------------------------------------------
class CommandEnumerator final : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IEnumExplorerCommand> {
	std::vector<ComPtr<IExplorerCommand>> commands_;
	size_t current_ = 0;
public:
	CommandEnumerator(std::vector<ComPtr<IExplorerCommand>> commands) : commands_(std::move(commands)) {
		g_objectCount.fetch_add(1, std::memory_order_relaxed);
	}
	~CommandEnumerator() override {
		g_objectCount.fetch_sub(1, std::memory_order_relaxed);
	}
	IFACEMETHODIMP Next(ULONG celt, IExplorerCommand** pUICommand, ULONG* pceltFetched) override {
		if (!pUICommand) return E_POINTER;
		ULONG fetched = 0;
		while (fetched < celt && current_ < commands_.size()) {
			commands_[current_].CopyTo(&pUICommand[fetched]);
			current_++;
			fetched++;
		}
		if (pceltFetched) *pceltFetched = fetched;
		return (fetched == celt) ? S_OK : S_FALSE;
	}
	IFACEMETHODIMP Skip(ULONG celt) override {
		current_ += celt;
		if (current_ > commands_.size()) current_ = commands_.size();
		return S_OK;
	}
	IFACEMETHODIMP Reset() override { current_ = 0; return S_OK; }
	IFACEMETHODIMP Clone(IEnumExplorerCommand** ppenum) override {
		if (!ppenum) return E_POINTER;
		auto clone = Make<CommandEnumerator>(commands_);
		clone->current_ = current_;
		return clone.CopyTo(ppenum);
	}
};

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
		if (modulePath.empty()) return E_FAIL;

		std::filesystem::path iconPath = std::filesystem::path(modulePath).remove_filename() / L"snapformat.ico";
		
		return duplicate_string(iconPath.c_str(), icon);
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

	IFACEMETHODIMP GetState(IShellItemArray* items, BOOL /*okToBeSlow*/, EXPCMDSTATE* state) override
	{
		if (state == nullptr) {
			return E_POINTER;
		}

		// 1. Comprovem que tenim elements seleccionats
		if (items == nullptr) {
			*state = ECS_HIDDEN;
			return S_OK;
		}

		// 2. Obtenim la quantitat d'arxius seleccionats
		DWORD count = 0;
		HRESULT hr = items->GetCount(&count);
		
		if (FAILED(hr) || count == 0) {
			*state = ECS_HIDDEN;
			return S_OK;
		}

		// 3. Límit de seguretat (ex: màxim 15 imatges alhora)
		// Si en seleccionen més, amaguem l'opció per no penjar el sistema
		if (count > 15) {
			*state = ECS_HIDDEN; 
			// Nota: També es podria fer servir ECS_DISABLED perquè
			// el botó es veiés gris i no es pogués clicar.
			return S_OK;
		}

		// Si passem totes les validacions, mostrem i habilitem el botó
		*state = ECS_ENABLED;
		return S_OK;
	}

	IFACEMETHODIMP Invoke(IShellItemArray* items, IBindCtx* /*bindCtx*/) override
	{
		return S_OK;
	}

	IFACEMETHODIMP GetFlags(EXPCMDFLAGS* flags) override
	{
		if (flags == nullptr) return E_POINTER;
		*flags = ECF_HASSUBCOMMANDS; 
		return S_OK;
	}

	IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** enumCommands) override
	{
		if (enumCommands == nullptr) return E_POINTER;

		std::vector<ComPtr<IExplorerCommand>> formats;
		formats.push_back(Make<FormatSubCommand>(L"png", L"To PNG"));
		formats.push_back(Make<FormatSubCommand>(L"jpg", L"To JPG"));
		formats.push_back(Make<FormatSubCommand>(L"webp", L"To WebP"));
		formats.push_back(Make<FormatSubCommand>(L"bmp", L"To BMP"));
		formats.push_back(Make<FormatSubCommand>(L"tiff", L"To TIFF"));

		auto enumerator = Make<CommandEnumerator>(std::move(formats));
		return enumerator.CopyTo(enumCommands);
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

extern "C" HRESULT STDAPICALLTYPE DllRegisterServer(void)
{
	HMODULE module = nullptr;
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
							 reinterpret_cast<LPCWSTR>(&DllRegisterServer), &module)) {
		return HRESULT_FROM_WIN32(GetLastError());
	}

	wchar_t buffer[MAX_PATH] = {};
	const DWORD length = GetModuleFileNameW(module, buffer, static_cast<DWORD>(std::size(buffer)));
	if (length == 0 || length >= std::size(buffer)) {
		return E_FAIL;
	}

	const std::wstring modulePath = buffer;
	HRESULT hr = register_shell_metadata(modulePath);
	if (SUCCEEDED(hr)) {
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
	}

	// Diagnostic: write HRESULT to temp file for troubleshooting
	wchar_t tempPath[MAX_PATH] = {};
	if (GetTempPathW(static_cast<DWORD>(std::size(tempPath)), tempPath) > 0) {
		std::wstring log = std::wstring(tempPath) + L"win11qc_register.log";
		HANDLE h = CreateFileW(log.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h != INVALID_HANDLE_VALUE) {
			DWORD written = 0;
			wchar_t buf[128] = {};
			StringCchPrintfW(buf, std::size(buf), L"DllRegisterServer HRESULT=0x%08X\r\n", static_cast<UINT>(hr));
			WriteFile(h, buf, static_cast<DWORD>(wcslen(buf) * sizeof(wchar_t)), &written, nullptr);
			CloseHandle(h);
		}
	}

	return hr;
}

extern "C" HRESULT STDAPICALLTYPE DllUnregisterServer(void)
{
	const HRESULT hr = unregister_shell_metadata();
	if (SUCCEEDED(hr)) {
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
	}

	return hr;
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

// DllRegisterServer / DllUnregisterServer are implemented inside the shell namespace below.

// DllRegisterServer / DllUnregisterServer are implemented above.

