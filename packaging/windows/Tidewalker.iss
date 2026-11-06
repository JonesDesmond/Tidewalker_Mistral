; Inno Setup script for Tidewalker. Compile with Inno Setup 6:
;   ISCC.exe packaging\windows\Tidewalker.iss
; Expects the game exe at build\Release\tidewalker.exe (VS) or build\tidewalker.exe (MinGW).

#define MyAppName "Tidewalker"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "Tidewalker Dev"
#define MyAppExeName "tidewalker.exe"

[Setup]
AppId={{A6F2C1D4-268E-42F1-9C3B-TIDEWALKER01}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
OutputBaseFilename=Tidewalker_Setup_{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
Source: "build\Release\tidewalker.exe"; DestDir: "{app}"; DestName: "{#MyAppExeName}"; Flags: ignoreversion; Check: FileExists('build\Release\tidewalker.exe')
Source: "build\tidewalker.exe"; DestDir: "{app}"; DestName: "{#MyAppExeName}"; Flags: ignoreversion; Check: not FileExists('build\Release\tidewalker.exe')
Source: "README.md"; DestDir: "{app}"; DestName: "README.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch {#MyAppName} now"; Flags: nowait postinstall skipifsilent
