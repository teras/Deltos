; Installer for the portable tree the workflow assembles beside this file.
Unicode true
!define NAME "Deltos"
!define PUBLISHER "Panayotis Katsaloulis"
!ifndef VERSION
  !define VERSION "1.0"
!endif
!define REGKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}"

Name "${NAME} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$PROGRAMFILES64\${NAME}"
InstallDirRegKey HKLM "Software\${NAME}" "InstallDir"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

VIProductVersion "${VERSION}.0.0"
VIAddVersionKey "ProductName" "${NAME}"
VIAddVersionKey "FileDescription" "Turn photos of documents into flat, upright, correctly-sized scans"
VIAddVersionKey "LegalCopyright" "${PUBLISHER}"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"

Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Section "Install"
    SetOutPath "$INSTDIR"
    File /r "${SOURCEDIR}\*.*"

    CreateShortCut "$SMPROGRAMS\${NAME}.lnk" "$INSTDIR\deltos.exe"
    WriteRegStr HKLM "Software\${NAME}" "InstallDir" "$INSTDIR"
    WriteRegStr HKLM "${REGKEY}" "DisplayName" "${NAME}"
    WriteRegStr HKLM "${REGKEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "${REGKEY}" "Publisher" "${PUBLISHER}"
    WriteRegStr HKLM "${REGKEY}" "DisplayIcon" "$INSTDIR\deltos.exe"
    WriteRegStr HKLM "${REGKEY}" "UninstallString" "$INSTDIR\uninstall.exe"
    WriteRegDWORD HKLM "${REGKEY}" "NoModify" 1
    WriteRegDWORD HKLM "${REGKEY}" "NoRepair" 1
    WriteUninstaller "$INSTDIR\uninstall.exe"
SectionEnd

Section "Uninstall"
    Delete "$SMPROGRAMS\${NAME}.lnk"
    RMDir /r "$INSTDIR"
    DeleteRegKey HKLM "${REGKEY}"
    DeleteRegKey HKLM "Software\${NAME}"
SectionEnd
