# XPマニフェスト
XPStyle on
# 圧縮メソッド
SetCompressor lzma
# インターフェース 設定
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_NOAUTOCLOSE
!define MUI_UNFINISHPAGE_NOAUTOCLOSE
# ページ
!insertmacro MUI_PAGE_WELCOME
Page custom OptionPage OptionPageLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
# アンインストーラ ページ
!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH
# 日本語UI
!insertmacro MUI_LANGUAGE "Japanese"
# 定数
!define Manufacturer "Project Flower"
!define ProductVersion "2.0.0.0"
!define UninstallRegistryKey "Software\Microsoft\Windows\CurrentVersion\Uninstall\${AssemblyName}"
# 変数
Var CheckBox_Startup
Var Dialog_Options
Var LaunchOnStartup

# 初期化時コールバック
Function .onInit
  # オプション値を初期化します。
  StrCpy $LaunchOnStartup ${BST_CHECKED}
FunctionEnd

# タスク スケジューラに登録
Function CreateScheduledTask
  ExecWait 'SCHTASKS /Create /SC ONLOGON /TN ${AssemblyName} /TR "\"$INSTDIR\FolderHScroller.exe\"" /IT /RL HIGHEST' $0

  ${If} $0 == "0"
    DetailPrint "タスク スケジューラに登録しました。"
  ${Else}
    DetailPrint "タスク スケジューラに登録できませんでした。"
    MessageBox MB_ICONSTOP|MB_OK "タスク スケジューラに登録できませんでした。"
  ${EndIf}
FunctionEnd

# オプション ページ
Function OptionPage
  !insertmacro MUI_HEADER_TEXT "インストール オプション" "オプションを選択してください。"
  nsDialogs::Create 1018
  Pop $Dialog_Options

  ${If} $Dialog_Options == error
    Abort
  ${EndIf}

  ${NSD_CreateCheckBox} 0 0 100% 12u "Windows ログオン時に実行する。"
  Pop $CheckBox_Startup

  ${If} $LaunchOnStartup == ${BST_CHECKED}
    ${NSD_Check} $CheckBox_Startup
  ${Else}
    ${NSD_Uncheck} $CheckBox_Startup
  ${EndIf}
  nsDialogs::Show
FunctionEnd

# オプション ページ退出コールバック
Function OptionPageLeave
  ${NSD_GetState} $CheckBox_Startup $LaunchOnStartup
FunctionEnd

# デフォルト セクション
Section
  # 実行ファイル
  SetOutPath "$INSTDIR"

  # インストールされるファイル
  File ${InstallSource}

  # ドキュメント
  CreateDirectory "$INSTDIR\Documents"
  SetOutPath "$INSTDIR\Documents"
  File "..\Documents\FolderHScroller-en.txt"
  File "..\Documents\FolderHScroller-ja.txt"

  # アンインストーラ
  WriteUninstaller "$INSTDIR\Uninstall.exe"

  # スタート メニューにショートカットを登録
  CreateDirectory "$SMPROGRAMS\${AssemblyName}"
  SetOutPath "$INSTDIR"
  CreateShortcut "$SMPROGRAMS\${AssemblyName}\${AssemblyName}.lnk" "$INSTDIR\FolderHScroller.exe" ""

  # レジストリ
  WriteRegStr HKCU "${UninstallRegistryKey}" "DisplayIcon" '"$INSTDIR\FolderHScroller.exe"'
  WriteRegStr HKCU "${UninstallRegistryKey}" "DisplayName" "${ProductName}"
  WriteRegStr HKCU "${UninstallRegistryKey}" "DisplayVersion" "${ProductVersion}"
  WriteRegStr HKCU "${UninstallRegistryKey}" "Publisher" "${Manufacturer}"
  WriteRegStr HKCU "${UninstallRegistryKey}" "UninstallString" '"$INSTDIR\Uninstall.exe"'

  ${If} $LaunchOnStartup == ${BST_CHECKED}
    # ログオン時に実行する
    Call CreateScheduledTask
  ${EndIf}
SectionEnd
