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

[Code]
const
  MsixPackageName = 'SnapFormat';
  RunOnceKey = 'Software\Microsoft\Windows\CurrentVersion\RunOnce';
  CleanupValueName = 'SnapFormatCleanup';

function RunPowerShell(const Command: String): Integer;
var
  ResultCode: Integer;
begin
  if Exec(ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
          '-NoProfile -NonInteractive -ExecutionPolicy Bypass -Command "' + Command + '"',
          '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
    Result := ResultCode
  else
    Result := -1;
end;

// The MSIX sparse package (install.bat) registers the same CLSID, so having
// both channels installed shows the menu entry twice. Offer to remove it.
function InitializeSetup(): Boolean;
begin
  Result := True;
  if RunPowerShell('if (Get-AppxPackage -Name ' + MsixPackageName + ') { exit 1 } else { exit 0 }') <> 1 then
    Exit;

  if SuppressibleMsgBox('SnapFormat is already installed as an MSIX package. ' +
       'Having both versions installed shows the menu entry twice.' + #13#10#13#10 +
       'Remove the MSIX package and continue?', mbConfirmation, MB_YESNO, IDYES) <> IDYES then
  begin
    Result := False;
    Exit;
  end;

  if RunPowerShell('Get-AppxPackage -Name ' + MsixPackageName + ' | Remove-AppxPackage -ErrorAction Stop') <> 0 then
  begin
    SuppressibleMsgBox('The MSIX package could not be removed. Remove SnapFormat from ' +
      'Settings > Apps > Installed apps and run Setup again.', mbError, MB_OK, IDOK);
    Result := False;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  // A cleanup queued by a previous uninstall must not delete this new install.
  if CurStep = ssInstall then
    RegDeleteValue(HKCU, RunOnceKey, CleanupValueName);
end;

// Explorer keeps shell-ext.dll loaded until it restarts, so the uninstaller
// cannot delete it. Queue the folder for deletion at the next sign-in instead
// (HKCU RunOnce works without administrator rights).
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  AppDir: String;
begin
  if CurUninstallStep <> usPostUninstall then
    Exit;

  AppDir := ExpandConstant('{app}');
  if not DirExists(AppDir) then
    Exit;

  RegWriteStringValue(HKCU, RunOnceKey, CleanupValueName,
    '"' + ExpandConstant('{cmd}') + '" /c rd /s /q "' + AppDir + '"');

  if FileExists(AppDir + '\shell-ext.dll') then
    SuppressibleMsgBox('SnapFormat has been removed. A file still in use by File Explorer ' +
      'will be deleted the next time you sign in.', mbInformation, MB_OK, IDOK);
end;
