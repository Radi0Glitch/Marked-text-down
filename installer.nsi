; Marked-text-down NSIS Installer Script
Unicode True
!include "LogicLib.nsh"
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
  File /nonfatal "tools\pandoc\windows-x86_64\pandoc.exe"

  SetOutPath "$INSTDIR\tools\typst\windows-x86_64"
  File /nonfatal "tools\typst\windows-x86_64\typst.exe"

  SetOutPath "$INSTDIR"
  WriteRegStr HKLM "Software\Marked-text-down" "Install_Dir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "DisplayName" "Marked-text-down"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "DisplayIcon" '"$INSTDIR\marked-text-down.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "Publisher" "Marked-text-down"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "DisplayVersion" "1.0.1"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Marked-text-down" "NoRepair" 1
  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateDirectory "$SMPROGRAMS\Marked-text-down"
  CreateShortcut "$SMPROGRAMS\Marked-text-down\Marked-text-down.lnk" "$INSTDIR\marked-text-down.exe"
  CreateShortcut "$SMPROGRAMS\Marked-text-down\Uninstall.lnk" "$INSTDIR\uninstall.exe"
  CreateShortcut "$DESKTOP\Marked-text-down.lnk" "$INSTDIR\marked-text-down.exe"
  CreateShortcut "$INSTDIR\Marked-text-down.lnk" "$INSTDIR\marked-text-down.exe" "" "" "Marked-text-down Markdown editor"

  ; --- File associations for .md / .markdown ---
  WriteRegStr HKLM "Software\Classes\MarkedTextDown.Markdown" "" "Markdown-документ (Marked-text-down)"
  WriteRegStr HKLM "Software\Classes\MarkedTextDown.Markdown\DefaultIcon" "" '"$INSTDIR\marked-text-down.exe",0'
  WriteRegStr HKLM "Software\Classes\MarkedTextDown.Markdown\shell\open\command" "" '"$INSTDIR\marked-text-down.exe" "%1"'
  WriteRegStr HKLM "Software\Classes\.md\OpenWithProgids" "MarkedTextDown.Markdown" ""
  WriteRegStr HKLM "Software\Classes\.markdown\OpenWithProgids" "MarkedTextDown.Markdown" ""
  ; Adopt the default handler only if the user has not chosen another app (or we already own it).
  ReadRegStr $0 HKLM "Software\Classes\.md" ""
  ${If} $0 == "" 
  ${OrIf} $0 == "MarkedTextDown.Markdown"
    WriteRegStr HKLM "Software\Classes\.md" "" "MarkedTextDown.Markdown"
  ${EndIf}
  ReadRegStr $0 HKLM "Software\Classes\.markdown" ""
  ${If} $0 == ""
  ${OrIf} $0 == "MarkedTextDown.Markdown"
    WriteRegStr HKLM "Software\Classes\.markdown" "" "MarkedTextDown.Markdown"
  ${EndIf}
  WriteRegStr HKLM "Software\Marked-text-down" "md_owner" "1"
  ; Tell Explorer to refresh icons/associations without a reboot.
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

Section "Uninstall"
  ; --- Remove file associations only if we still own them ---
  ReadRegStr $0 HKLM "Software\Classes\.md" ""
  ${If} $0 == "MarkedTextDown.Markdown"
    DeleteRegValue HKLM "Software\Classes\.md" ""
  ${EndIf}
  ReadRegStr $0 HKLM "Software\Classes\.markdown" ""
  ${If} $0 == "MarkedTextDown.Markdown"
    DeleteRegValue HKLM "Software\Classes\.markdown" ""
  ${EndIf}
  DeleteRegValue HKLM "Software\Classes\.md\OpenWithProgids" "MarkedTextDown.Markdown"
  DeleteRegValue HKLM "Software\Classes\.markdown\OpenWithProgids" "MarkedTextDown.Markdown"
  DeleteRegKey HKLM "Software\Classes\MarkedTextDown.Markdown"
  System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'

  Delete "$INSTDIR\marked-text-down.exe"
  Delete "$INSTDIR\SDL3.dll"
  Delete "$INSTDIR\SDL3_ttf.dll"
  Delete "$INSTDIR\README.md"
  Delete "$INSTDIR\uninstall.exe"
  Delete "$INSTDIR\Marked-text-down.lnk"

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
