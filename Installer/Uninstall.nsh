# タスク スケジューラから削除
Function un.DeleteScheduledTask
  # ExecWait "SCHTASKS /Delete /TN FolderHScroller /F"
  nsExec::Exec "SCHTASKS /Delete /TN FolderHScroller /F"
FunctionEnd

# アンインストーラ
Section "Uninstall"
  # アンインストーラを削除
  Delete "$INSTDIR\Uninstall.exe"
  # ファイルを削除
  Delete "$INSTDIR\FolderHScroller.exe"
  # ディレクトリを削除
  RMDir /r "$INSTDIR"

  # スタート メニューから削除
  RMDir /r "$SMPROGRAMS\${AssemblyName}"

  # レジストリ キーを削除
  DeleteRegKey HKCU "${UninstallRegistryKey}"

  # タスク スケジューラを削除
  Call un.DeleteScheduledTask
SectionEnd
