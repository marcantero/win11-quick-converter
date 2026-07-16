[Setup]
; Basic Application Information
AppName=SnapFormat
AppVersion=0.1.0
AppPublisher=Win11 Quick Converter
DefaultDirName={userappdata}\SnapFormat
DefaultGroupName=SnapFormat
OutputDir=..\build\installer
OutputBaseFilename=SnapFormat_Setup
; Install as current user to correctly register HKCU registry keys
PrivilegesRequired=lowest
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64

[Files]
; Copy binaries (make sure you built in Release mode before compiling this script)
Source: "..\build\bin\Release\converter-cli.exe"; DestDir: "{app}"; Flags: ignoreversion
; regserver flag automatically calls DllRegisterServer on install and DllUnregisterServer on uninstall
Source: "..\build\bin\Release\shell-ext.dll"; DestDir: "{app}"; Flags: ignoreversion regserver
Source: "..\src\shell-ext\snapformat.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Uninstall SnapFormat"; Filename: "{uninstallexe}"
