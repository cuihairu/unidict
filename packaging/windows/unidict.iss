; Unidict Windows installer (Inno Setup 6).
; Input: the staged daily-build tree (see "Package (Windows)" in
; .github/workflows/daily-build.yml) passed via /DSourceRoot=<abs path>.
; Output: unidict-windows-x64-setup.exe (via /DOutputDir=<abs path>).
;
; Per-machine install to Program Files\Unidict with Start menu shortcuts,
; optional desktop icon and an Add/Remove Programs uninstaller entry.
; GUI main program unidict_qml.exe is the WIN32 (GUI subsystem) build:
; launching it (from Start menu or double-click) shows no console window
; (BUGS.md BUG-001). The bundle dict.json next to the exe is auto-loaded
; when UNIDICT_DICTS is unset (BUGS.md BUG-002).

#ifndef SourceRoot
#define SourceRoot "..\..\stage"
#endif
#ifndef OutputDir
#define OutputDir "..\.."
#endif

[Setup]
; Fixed AppId: stable upgrade/uninstall identity across daily builds
AppId={{6F1A2C7B-4E8D-4B9A-9C3E-1A5D7F0B2C48}
AppName=Unidict
AppVersion=1.0
AppPublisher=cuihairu
AppPublisherURL=https://github.com/cuihairu/unidict
DefaultDirName={autopf}\Unidict
DefaultGroupName=Unidict
DisableProgramGroupPage=yes
LicenseFile=..\..\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename=unidict-windows-x64-setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayIcon={app}\unidict_qml.exe

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; \
    GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Whole staged tree: unidict_qml.exe + windeployqt output (Qt DLLs and
; plugin subdirs), unidict_cli*.exe, dict.json, PLATFORM-NOTES.txt
Source: "{#SourceRoot}\*"; DestDir: "{app}"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Unidict"; Filename: "{app}\unidict_qml.exe"
Name: "{group}\Unidict CLI"; Filename: "{app}\unidict_cli_std.exe"
Name: "{group}\Unidict Notes"; Filename: "{app}\PLATFORM-NOTES.txt"
Name: "{autodesktop}\Unidict"; Filename: "{app}\unidict_qml.exe"; \
    Tasks: desktopicon

[Run]
Filename: "{app}\unidict_qml.exe"; Description: "{cm:LaunchProgram,Unidict}"; \
    Flags: nowait postinstall skipifsilent
