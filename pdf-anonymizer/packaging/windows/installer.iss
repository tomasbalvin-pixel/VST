; Inno Setup script for PDF Anonymizer.
; Installs per user into %LOCALAPPDATA%\Programs; no administrator rights needed.
; Build: iscc /DAppVersion=1.0.0 packaging\windows\installer.iss

#ifndef AppVersion
  #define AppVersion "1.0.0"
#endif

[Setup]
AppId={{6C1E4E0B-3F55-4D7C-9B8B-2B7E1C0F4A11}
AppName=PDF Anonymizer
AppVersion={#AppVersion}
AppPublisher=PDF Anonymizer
DefaultDirName={autopf}\PDF Anonymizer
DefaultGroupName=PDF Anonymizer
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\..\dist
OutputBaseFilename=PDF-Anonymizer-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\PDF Anonymizer.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "czech"; MessagesFile: "compiler:Languages\Czech.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "..\..\dist\PDF Anonymizer\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\PDF Anonymizer"; Filename: "{app}\PDF Anonymizer.exe"
Name: "{group}\Uninstall PDF Anonymizer"; Filename: "{uninstallexe}"
Name: "{autodesktop}\PDF Anonymizer"; Filename: "{app}\PDF Anonymizer.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\PDF Anonymizer.exe"; Description: "{cm:LaunchProgram,PDF Anonymizer}"; Flags: nowait postinstall skipifsilent
