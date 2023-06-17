#pragma once
#include <Windows.h>
#include <shellapi.h>
#include <tchar.h>

bool CheckWndClassName(HWND hwnd, const TCHAR* pszClassName);
BOOL CALLBACK DestroyExistsProcess(HWND hwnd, LPARAM lParam);
bool DoPopupMenu(HWND hwnd);
BOOL CALLBACK EnumExplorerProc(HWND hwnd, LPARAM lParam);
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT nMessage, WPARAM wParam, LPARAM lParam);
bool RegisterTaskTray(HWND hwnd);
void SetHook(bool bEnable);
void SetIconTip(PNOTIFYICONDATA);
void SetStyle(HWND);
void CALLBACK TimerIconFlashEndProc(HWND unnamedParam1, UINT unnamedParam2, UINT_PTR unnamedParam3, DWORD unnamedParam4);
void TurnoffIcon();
void TurnoffIconAsync();
void TurnonIcon();
void UnregisterTaskTray();
void UpdateIconTips();
VOID CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD idEventThread, DWORD dwmsEventTime);
