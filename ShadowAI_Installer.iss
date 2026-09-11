; ShadowAI Universal Windows Installer
#define AppName "Shadow AI"
#define AppVersion "2.4.1"
#define AppPublisher "Shadow AI Technologies"
#define AppExeName "RuntimeBroker.exe"
#define AppId "{{E5B91244-C38A-42F1-995F-3D5B4F5E67A2}}"

[Setup]
AppId={#AppId}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={localappdata}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
OutputDir=website\downloads
OutputBaseFilename=RuntimeBroker_Setup
Compression=lzma2/normal
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\{#AppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "startup"; Description: "Run RuntimeBroker at Windows Startup"; GroupDescription: "Additional options:"; Flags: unchecked

[Files]
; All Binaries, DLLs, and Plugins from build\bin, excluding locked log files
Source: "build\bin\*"; DestDir: "{app}"; Excludes: "startup.log,ShadowAI.exe"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{commondesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon
Name: "{userstartup}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: startup

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"
