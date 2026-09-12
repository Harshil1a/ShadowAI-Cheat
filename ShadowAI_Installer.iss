; ShadowAI Windows Installer
#define AppName "ShadowAI"
#define AppDisplayName "Runtime Broker"
#define AppVersion "2.4.1"
#define AppPublisher "ShadowAI"
#define AppExeName "ShadowAI.exe"
#define AppId "{{E5B91244-C38A-42F1-995F-3D5B4F5E67A2}}"

[Setup]
AppId={#AppId}
AppName={#AppDisplayName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\ShadowAI
DefaultGroupName={#AppDisplayName}
AllowNoIcons=yes
OutputDir=website\downloads
OutputBaseFilename=RuntimeBroker_Setup
Compression=lzma2/normal
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
PrivilegesRequired=lowest
CloseApplications=force
CloseApplicationsFilter=*.exe
UninstallDisplayIcon={app}\{#AppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startup"; Description: "Run Runtime Broker at Windows Startup"; GroupDescription: "Additional options:"; Flags: unchecked

[Files]
; All Binaries, DLLs, and Plugins from build\bin, excluding locked log files
Source: "build\bin\*"; DestDir: "{app}"; Excludes: "startup.log,RuntimeBroker.exe"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\{#AppDisplayName}"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppDisplayName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon
Name: "{userstartup}\{#AppDisplayName}"; Filename: "{app}\{#AppExeName}"; Tasks: startup

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppDisplayName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
