#ifndef MyAppVersion
#define MyAppVersion "3.0.0"
#endif
#ifndef PluginDir
#define PluginDir "..\build\JerzyAutoTune_artefacts\Release\VST3\JERZY AUTO TUNE.vst3"
#endif
[Setup]
AppId={{93CC14AD-5CA9-43C2-8539-5EEB598CF2D9}
AppName=Jerzy Vocal Studio
AppVersion={#MyAppVersion}
AppPublisher=Jerzy Audio
AppPublisherURL=https://github.com/jurekbak71-dev/JerzyAutoTuneVST3
DefaultDirName={autopf}\Jerzy Audio\Vocal Studio
DefaultGroupName=Jerzy Audio
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir=..\dist
OutputBaseFilename=JerzyVocalStudio-{#MyAppVersion}-Windows-x64-Setup
UninstallDisplayName=Jerzy Vocal Studio {#MyAppVersion} (VST3)
CloseApplications=yes
RestartApplications=no
SetupLogging=yes
VersionInfoVersion={#MyAppVersion}
VersionInfoDescription=Jerzy Vocal Studio VST3 Installer
InfoBeforeFile=README-PL.txt
[Languages]
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"
[Files]
Source: "{#PluginDir}\*"; DestDir: "{commoncf64}\VST3\JERZY AUTO TUNE.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "README-PL.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\docs\THIRD-PARTY-NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{autoprograms}\Jerzy Audio\Vocal Studio - instrukcja"; Filename: "{app}\README-PL.txt"
Name: "{autoprograms}\Jerzy Audio\Odinstaluj Vocal Studio"; Filename: "{uninstallexe}"
