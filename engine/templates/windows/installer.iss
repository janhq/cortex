; Define the application name, version, and other details
[Setup]
AppName=cortexcpp
AppVersion=1.0
DefaultDirName={localappdata}\cortexcpp
DefaultGroupName=cortexcpp
OutputDir=.
OutputBaseFilename=setup
Compression=lzma
SolidCompression=yes
PrivilegesRequired=lowest
AllowNoIcons=yes

; Define the languages section
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

; Define the files to be installed
[Files]
Source: "cortex.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "msvcp140.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "vcruntime140.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "vcruntime140_1.dll"; DestDir: "{app}"; Flags: ignoreversion

; Define the icons to be created
[Icons]
Name: "{group}\cortexcpp"; Filename: "{app}\cortex.exe"

; Define the uninstall run section to execute commands before uninstallation
[UninstallRun]
Filename: "{app}\cortex.exe"; Parameters: "stop"; StatusMsg: "Stopping cortexcpp service..."; Flags: runhidden

; Combine all Pascal scripting code in one [Code] section
[Code]
procedure AddToUserPathAndInstallEngines;
var
  ExpandedAppDir: String;
  CmdLine, CortexInstallCmd: String;
  ResultCode: Integer;
begin
  ExpandedAppDir := ExpandConstant('{app}');
  
  // Add {app} to PATH
  CmdLine := Format('setx PATH "%s;%%PATH%%"', [ExpandedAppDir]);
  Exec('cmd.exe', '/C ' + CmdLine, '', SW_HIDE, ewWaitUntilTerminated, ResultCode);

  // Update status message for downloading llamacpp engine
  WizardForm.StatusLabel.Caption := 'Downloading llama.cpp engine and dependencies ...';
  WizardForm.StatusLabel.Update;

  // Download llamacpp engine by default
  CortexInstallCmd := Format('"%s\cortex.exe" engines install cortex.llamacpp', [ExpandedAppDir]);
  Exec('cmd.exe', '/C ' + CortexInstallCmd, '', SW_HIDE, ewWaitUntilTerminated, ResultCode);

  // Clear the status message after completion
  WizardForm.StatusLabel.Caption := '';
  WizardForm.StatusLabel.Update;
end;

procedure DeleteCurrentUserCortexFolderAndConfig;
var
  UserCortexFolder: String;
  UserCortexConfig: String;
  ShouldDelete: Integer;
begin
  UserCortexFolder := ExpandConstant('{%USERPROFILE}\cortexcpp');
  UserCortexConfig := ExpandConstant('{%USERPROFILE}\.cortexrc');
  
  if DirExists(UserCortexFolder) or FileExists(UserCortexConfig) then
  begin
    ShouldDelete := MsgBox('Do you want to delete the application data in cortexcpp and the .cortexrc config file (this will remove all user data)?', mbConfirmation, MB_YESNO);
    
    if ShouldDelete = idYes then
    begin
      if DirExists(UserCortexFolder) then
      begin
        DelTree(UserCortexFolder, True, True, True);
      end;

      if FileExists(UserCortexConfig) then
      begin
        DeleteFile(UserCortexConfig);
      end;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    AddToUserPathAndInstallEngines;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    DeleteCurrentUserCortexFolderAndConfig;
  end;
end;
