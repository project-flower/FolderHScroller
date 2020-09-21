# 共通ヘッダ
!include FolderHScroller_Head.nsh

# 定数
!define InstallSource "..\Release\FolderHScroller.exe"
!define ProductName "${AssemblyName} (x86)"
# インストール先
InstallDir "$PROGRAMFILES32\${AssemblyName}"
# 作成されるインストーラ
OutFile "Install_${AssemblyName}_x86.exe"

# メイン ヘッダ
!include FolderHScroller_Main.nsh
# アンインストーラ ヘッダ
!include Uninstall.nsh
