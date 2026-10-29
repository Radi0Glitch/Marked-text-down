; Marked-text-down NSIS Installer Script
Unicode True
!include "MUI2.nsh"

Name "Marked-text-down"
OutFile "..\Marked-text-down-Setup-x86_64.exe"
InstallDir "$PROGRAMFILES64\Marked-text-down"
InstallDirRegKey HKLM "Software\Marked-text-down" "Install_Dir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\marked-text-down.exe"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "Russian"
!insertmacro MUI_LANGUAGE "English"

Section "Marked-text-down" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"

  File "marked-text-down.exe"
  File "SDL3.dll"
  File "SDL3_ttf.dll"
  File "README.md"

  SetOutPath "$INSTDIR\fonts"
  File "fonts\*.*"

  SetOutPath "$INSTDIR\tools\pandoc\windows-x86_64"
  File "tools\pandoc\windows-x86_64\pandoc.exe"

  SetOutPath "$INSTDIR"
  WriteRegStr HKLM "Software\Marked-text-down" "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "DisplayName" "Marked-text-down"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "DisplayIcon" '"$INSTDIR\marked-text-down.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "Publisher" "Marked-text-down"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "DisplayVersion" "0.1.0"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "NoRepair" 1
  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateDirectory "$SMPROGRAMS\Marked-text-down"
  CreateShortcut "$SMPROGRAMS\Marked-text-down\Marked-text-down.lnk" "$INSTDIR\marked-text-down.exe"
  CreateShortcut "$SMPROGRAMS\Marked-text-down\Uninstall.lnk" "$INSTDIR\uninstall.exe"
  CreateShortcut "$DESKTOP\Marked-text-down.lnk" "$INSTDIR\marked-text-down.exe"
SectionEnd

Section "Uninstall"
  Delete "$INSTDIR\marked-text-down.exe"
  Delete "$INSTDIR\SDL3.dll"
  Delete "$INSTDIR\SDL3_ttf.dll"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\uninstall.exe"

  RMDir /r "$INSTDIR\fonts"
  RMDir /r "$INSTDIR\tools"
  RMDir "$INSTDIR"

  Delete "$SMPROGRAMS\Marked-text-down\Marked-text-down.lnk"
  Delete "$SMPROGRAMS\Marked-text-down\Uninstall.lnk"
  RMDir "$SMPROGRAMS\Marked-text-down"
  Delete "$DESKTOP\Marked-text-down.lnk"

  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down"
  DeleteRegKey HKLM "Software\Marked-text-down"
SectionEnd
