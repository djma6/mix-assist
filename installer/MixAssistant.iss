[Setup]
AppName=Mix Assistant
AppVersion=0.1.0
DefaultDirName={commoncf64}\VST3\Mix Assistant.vst3
DisableDirPage=yes
OutputBaseFilename=MixAssistant-Setup
OutputDir=Output
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
Compression=lzma2

[Files]
Source: "..\build\MixAssistant_artefacts\Release\VST3\Mix Assistant.vst3\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion
