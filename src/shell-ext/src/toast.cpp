#include "win11qc/toast.h"

#include <appmodel.h>
#include <roapi.h>
#include <windows.data.xml.dom.h>
#include <windows.ui.notifications.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

namespace win11qc::shell {

namespace {

using ABI::Windows::Data::Xml::Dom::IXmlDocument;
using ABI::Windows::Data::Xml::Dom::IXmlDocumentIO;
using ABI::Windows::UI::Notifications::IToastNotification;
using ABI::Windows::UI::Notifications::IToastNotificationFactory;
using ABI::Windows::UI::Notifications::IToastNotificationManagerStatics;
using ABI::Windows::UI::Notifications::IToastNotifier;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Wrappers::HStringReference;

bool has_package_identity()
{
    UINT32 length = 0;
    return GetCurrentPackageFullName(&length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
}

} // namespace

HRESULT show_toast(const std::wstring& xml)
{
    ComPtr<IInspectable> inspectable;
    HRESULT hr = RoActivateInstance(HStringReference(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument).Get(), &inspectable);
    if (FAILED(hr)) return hr;

    ComPtr<IXmlDocumentIO> documentIo;
    hr = inspectable.As(&documentIo);
    if (FAILED(hr)) return hr;

    hr = documentIo->LoadXml(HStringReference(xml.c_str(), static_cast<unsigned int>(xml.size())).Get());
    if (FAILED(hr)) return hr;

    ComPtr<IXmlDocument> document;
    hr = documentIo.As(&document);
    if (FAILED(hr)) return hr;

    ComPtr<IToastNotificationManagerStatics> manager;
    hr = RoGetActivationFactory(
        HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager).Get(), IID_PPV_ARGS(&manager));
    if (FAILED(hr)) return hr;

    ComPtr<IToastNotifier> notifier;
    hr = has_package_identity()
        ? manager->CreateToastNotifier(&notifier)
        : manager->CreateToastNotifierWithId(HStringReference(kToastAppId).Get(), &notifier);
    if (FAILED(hr)) return hr;

    ComPtr<IToastNotificationFactory> factory;
    hr = RoGetActivationFactory(
        HStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification).Get(), IID_PPV_ARGS(&factory));
    if (FAILED(hr)) return hr;

    ComPtr<IToastNotification> toast;
    hr = factory->CreateToastNotification(document.Get(), &toast);
    if (FAILED(hr)) return hr;

    return notifier->Show(toast.Get());
}

} // namespace win11qc::shell
