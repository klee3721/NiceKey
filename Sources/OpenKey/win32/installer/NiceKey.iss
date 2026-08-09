#define MyAppName "NiceKey"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "NiceKey"
#define MyAppURL "https://github.com/klee3721/NiceKey"

#ifndef SourceX64
  #define SourceX64 "..\build\x64\NiceKey64.exe"
#endif
#ifndef SourceX86
  #define SourceX86 "..\build\x86\NiceKey32.exe"
#endif

[Setup]
AppId={{7F60605B-9295-44DF-9261-6790D70552E8}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={localappdata}\Programs\NiceKey
DefaultGroupName=NiceKey
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x86 x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.17763
OutputDir=output
OutputBaseFilename=NiceKey-{#MyAppVersion}-Windows-Setup
SetupIconFile=..\OpenKey\OpenKey\icon.ico
UninstallDisplayIcon={app}\NiceKey.exe
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
RestartApplications=no
VersionInfoVersion={#MyAppVersion}.0
VersionInfoCompany=NiceKey
VersionInfoDescription=NiceKey Windows Installer
VersionInfoProductName=NiceKey
VersionInfoProductVersion={#MyAppVersion}
LicenseFile=..\..\..\..\LICENSE

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Tạo biểu tượng ngoài màn hình"; GroupDescription: "Tùy chọn bổ sung:"; Flags: unchecked

[Files]
Source: "{#SourceX64}"; DestDir: "{app}"; DestName: "NiceKey.exe"; Flags: ignoreversion; Check: Is64BitInstallMode
Source: "{#SourceX86}"; DestDir: "{app}"; DestName: "NiceKey.exe"; Flags: ignoreversion; Check: not Is64BitInstallMode
Source: "..\..\..\..\LICENSE"; DestDir: "{app}\Licenses"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\..\..\..\NOTICE.md"; DestDir: "{app}\Licenses"; Flags: ignoreversion

[Icons]
Name: "{group}\NiceKey"; Filename: "{app}\NiceKey.exe"
Name: "{autodesktop}\NiceKey"; Filename: "{app}\NiceKey.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\NiceKey.exe"; Description: "Mở NiceKey"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{cmd}"; Parameters: "/C taskkill /F /IM NiceKey.exe"; Flags: runhidden; RunOnceId: "StopNiceKey"
