; RuntimeBroker Universal Windows Installer
#define AppName "RuntimeBroker"
#define AppVersion "2.4.0"
#define AppPublisher "Microsoft Corporation"
#define AppExeName "RuntimeBroker.exe"
#define AppId "{{B9A45678-1234-4567-8901-CDEF12345678}"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
AppId={#AppId}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
OutputDir=website\downloads
OutputBaseFilename=RuntimeBroker_Setup
Compression=lzma2/max
SolidCompression=no
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
; UI settings
WizardStyle=modern
PrivilegesRequired=admin
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
