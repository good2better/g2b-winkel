!define APPNAME "G2B Ocho"
!define VERSION "6.9"
Unicode True
!include "MUI2.nsh"

Name "G2B Ocho v${VERSION}"
!ifdef GIO
OutFile "G2B-Ocho-Setup-GIO.exe"
!else
OutFile "G2B-Ocho-Setup.exe"
!endif
InstallDir "$PROGRAMFILES64\G2B\Ocho"
RequestExecutionLevel admin

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "Dutch"

Section "G2B Ocho" SecMain
    SetOutPath "$INSTDIR"
    File "g2b_ocho.exe"

    SetOutPath "$COMMONFILES64\VST3"
    File /r "g2b_ocho.vst3"

    SetOutPath "$COMMONFILES64\CLAP"
    File "g2b_ocho.clap"

    CreateShortCut "$DESKTOP\G2B Ocho.lnk" "$INSTDIR\g2b_ocho.exe" "" "$INSTDIR\g2b_ocho.exe" 0

!ifdef GIO
    SetOutPath "$DOCUMENTS"
    File /r "G2B-SMAAK-SETS"
!endif

    WriteUninstaller "$INSTDIR\uninstall.exe"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\G2BOcho" "DisplayName" "G2B Ocho"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\G2BOcho" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\G2BOcho" "Publisher" "good2better"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\G2BOcho" "UninstallString" '"$INSTDIR\uninstall.exe"'
SectionEnd

Section "Uninstall"
    RMDir /r "$COMMONFILES64\VST3\g2b_ocho.vst3"
    Delete "$COMMONFILES64\CLAP\g2b_ocho.clap"
    Delete "$DESKTOP\G2B Ocho.lnk"
!ifdef GIO
    RMDir /r "$DOCUMENTS\G2B-SMAAK-SETS"
!endif
    RMDir /r "$INSTDIR"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\G2BOcho"
SectionEnd
