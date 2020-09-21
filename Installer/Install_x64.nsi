# 共通ヘッダ
!include FolderHScroller_Head.nsh

# 定数
!define InstallSource "..\x64\Release\FolderHScroller.exe"
!define ProductName "${AssemblyName} (x64)"
# インストール先
InstallDir "$PROGRAMFILES64\${AssemblyName}"
# 作成されるインストーラ
OutFile "Install_${AssemblyName}_x64.exe"

# メイン ヘッダ
!include FolderHScroller_Main.nsh
# アンインストーラ ヘッダ
!include Uninstall.nsh
