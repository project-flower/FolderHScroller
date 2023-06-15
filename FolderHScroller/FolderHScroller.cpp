//////////////////////////////////////////////////////////////////////////////
// version macro

#define _WIN32_IE	0x0500

//////////////////////////////////////////////////////////////////////////////
// include

#include <windows.h>
#include <tchar.h>
#include <commctrl.h>
#include <shellapi.h>

#include <cstdlib>
#include <vector>

#include "resource.h"

//////////////////////////////////////////////////////////////////////////////
// constant

#define WM_NOTIFY_ICON		(WM_APP+100)
#define WM_ICON_FLASHEND	(WM_NOTIFY_ICON+1)

//////////////////////////////////////////////////////////////////////////////
// global constant

#define APP_UI_NAME         _T("FolderHScroller")
#define APP_UNIQUE_NAME     _T("PF:FolderHorizontalScroller")

#define CLASSNAME_SYSTREEVIEW32 _T("SysTreeView32")

#define MAX_LOADSTRING 100

const TCHAR g_szAppUIName[] = APP_UI_NAME;
const TCHAR g_szAppUniqueName[] = APP_UNIQUE_NAME;
const TCHAR g_szSingletonName[] = APP_UNIQUE_NAME _T(":Singleton");
const TCHAR g_szTerminateName[] = APP_UNIQUE_NAME _T(":Terminate");

//////////////////////////////////////////////////////////////////////////////
// global variable

HINSTANCE g_hinstThis = nullptr;
HWND g_hwndMain = nullptr;

HANDLE g_hEventTerminate = nullptr;
HANDLE g_hMutexSingleton = nullptr;
HWINEVENTHOOK hWinEventHook = nullptr;

bool g_bMonitoring = false;
bool g_bNoIcon = false;
bool g_bEnabled = false;

NOTIFYICONDATA g_nid;
bool g_bIconFlashing = false;
HICON g_hIcon = nullptr;
HICON g_hIconFlash = nullptr;

UINT WM_TASKBARCREATED = 0;

TCHAR g_szDisabled[MAX_LOADSTRING];

//////////////////////////////////////////////////////////////////////////////
// global function

void SetIconTip(PNOTIFYICONDATA);
void SetStyle(HWND);
void TimerIconFlashEndProc(HWND unnamedParam1, UINT unnamedParam2, UINT_PTR unnamedParam3, DWORD unnamedParam4);
void TurnoffIcon();
void TurnoffIconAsync();
void TurnonIcon();

//////////////////////////////////////////////////////////////////////////////
// window operation

auto CheckWndClassName = [](HWND hwnd, const TCHAR* pszClassName) -> bool {
	const int CLASSNAME_MAX = 128;
	TCHAR szClassName[CLASSNAME_MAX+1];
	szClassName[CLASSNAME_MAX] = _T('\0');
	if (GetClassName(hwnd, szClassName, CLASSNAME_MAX) == 0) {
		szClassName[0] = _T('\0');
	}
	return _tcsicmp(szClassName, pszClassName) == 0;
};

HWND FindChild(HWND hwnd, const TCHAR* szClassName) {
	for (HWND hwndChild = GetWindow(hwnd, GW_CHILD);
		hwndChild;
		hwndChild = GetWindow(hwndChild, GW_HWNDNEXT)) {
		if (CheckWndClassName(hwndChild, szClassName)) {
			return hwndChild;
		}

		const HWND hwndFound = FindChild(hwndChild, szClassName);

		if (hwndFound) {
			return hwndFound;
		}
	}

	return nullptr;
}

VOID CALLBACK WinEventProc(
	HWINEVENTHOOK hWinEventHook,
	DWORD         event,
	HWND          hwnd,
	LONG          idObject,
	LONG          idChild,
	DWORD         idEventThread,
	DWORD         dwmsEventTime) {
	if (WaitForSingleObject(g_hEventTerminate, 1) != WAIT_TIMEOUT) {
		if (g_hwndMain != nullptr) {
			DestroyWindow(g_hwndMain);
		}

		return;
	}

	TurnonIcon();

	if (CheckWndClassName(hwnd, CLASSNAME_SYSTREEVIEW32)) {
		SetStyle(hwnd);
	}

	TurnoffIconAsync();
}

//////////////////////////////////////////////////////////////////////////////
// explorer operation

void SetStyle(HWND hwnd) {
    LONG_PTR nStyle = GetWindowLongPtr(hwnd, GWL_STYLE);

    if ((nStyle & TVS_NOHSCROLL) != 0) {
        nStyle &= ~TVS_NOHSCROLL;
        SetWindowLongPtr(hwnd, GWL_STYLE, nStyle);
    }
}

BOOL CALLBACK EnumExplorerProc(HWND hwnd, LPARAM /*lParam*/) {
	const HWND hwndFound = FindChild(hwnd, CLASSNAME_SYSTREEVIEW32);

	if (hwndFound) {
		SetStyle(hwndFound);
	}

	return TRUE;
}

auto AdjustExplorer = []() {
	EnumWindows(EnumExplorerProc, 0);
};

//////////////////////////////////////////////////////////////////////////////
// tasktray operation

void TimerIconFlashEndProc(HWND unnamedParam1, UINT unnamedParam2, UINT_PTR unnamedParam3, DWORD unnamedParam4) {
	TurnoffIcon();
}

void TurnoffIcon() {
	if (!g_bMonitoring || g_bNoIcon) return;

	g_nid.hIcon = g_hIcon;
	Shell_NotifyIcon(NIM_MODIFY, &g_nid);
	g_bIconFlashing = false;
}

void TurnoffIconAsync() {
	if (!g_bMonitoring || g_bNoIcon) return;

	SetTimer(nullptr, WM_ICON_FLASHEND, 500, TimerIconFlashEndProc);
}

void TurnonIcon() {
	if (g_bIconFlashing || !g_bMonitoring || g_bNoIcon) return;

	g_nid.hIcon = g_hIconFlash;
	Shell_NotifyIcon(NIM_MODIFY, &g_nid);
	g_bIconFlashing = true;
}

bool RegisterTaskTray (HWND hwnd) {
	memset(&g_nid, 0, sizeof(g_nid));
	if (!g_bNoIcon) {
		g_nid.cbSize = sizeof(g_nid);
		g_nid.hWnd = hwnd;
		g_nid.uID = 1;
		g_nid.uFlags =  NIF_MESSAGE | NIF_ICON | NIF_TIP;
		g_nid.uCallbackMessage = WM_NOTIFY_ICON;
		g_nid.hIcon = LoadIcon(
			g_hinstThis,
			MAKEINTRESOURCE(IDI_FOLDER_HSCROLLER));
		SetIconTip(&g_nid);
		bool bResult = false;

		for (int i = 0; i < 100; ++i, Sleep(1000)) {
			Shell_NotifyIcon(NIM_ADD, &g_nid);

			if (GetLastError() == ERROR_TIMEOUT) {
				continue;
			}

			if (Shell_NotifyIcon(NIM_MODIFY, &g_nid)) {
				bResult = true;
				break;
			}
		}

		if (!bResult) {
			memset(&g_nid, 0, sizeof(g_nid));
		}

		return bResult;
	}
	else {
		return true;
	}
}

auto UnregisterTaskTray = []() {
	if (g_nid.cbSize != 0) {
		Shell_NotifyIcon(NIM_DELETE, &g_nid);
	}
};

void SetIconTip(PNOTIFYICONDATA lpData)
{
	_tcscpy_s(lpData->szTip, g_szAppUIName);

	if (!g_bEnabled) {
		_tcscat_s(lpData->szTip, _T(" ("));
		_tcscat_s(lpData->szTip, g_szDisabled);
		_tcscat_s(lpData->szTip, _T(")"));
	}
}

void UpdateIconTips()
{
	if (g_bNoIcon) {
		return;
	}

	SetIconTip(&g_nid);
	Shell_NotifyIcon(NIM_MODIFY, &g_nid);
}

//////////////////////////////////////////////////////////////////////////////
// menu

auto DoPopupMenu = [](HWND hwnd) -> bool {
	bool bResult = false;
	POINT ptMenu;
	GetCursorPos(&ptMenu);
	HMENU hmenuPopup = LoadMenu(
		g_hinstThis, MAKEINTRESOURCE(IDR_MENU_POPUP));
	if (hmenuPopup != nullptr) {
		HMENU hmenuSub = GetSubMenu(hmenuPopup, 0);
		const UINT uCheck = (g_bEnabled ? MF_UNCHECKED : MF_CHECKED);
		CheckMenuItem(hmenuSub, IDM_APP_STOP, uCheck);

		if (hmenuSub != nullptr) {
			SetForegroundWindow(hwnd);
			bResult = (TrackPopupMenu(
				hmenuSub,
				TPM_LEFTALIGN | TPM_RIGHTBUTTON,
				ptMenu.x, ptMenu.y,
				0,
				hwnd,
				nullptr) != FALSE);
			PostMessage(hwnd, WM_NULL, 0, 0);
		}
		DestroyMenu(hmenuPopup);
	}
	return bResult;
};

void SetHook(bool bEnable)
{
	if (bEnable) {
		if (hWinEventHook) {
			return;
		}

		hWinEventHook = SetWinEventHook(
			EVENT_OBJECT_CREATE,
			EVENT_OBJECT_CREATE,
			nullptr,
			WinEventProc,
			0,
			0,
			(WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS));
		TurnonIcon();
		AdjustExplorer();
		TurnoffIconAsync();
	}
	else {
		if (!hWinEventHook) {
			return;
		}

		UnhookWinEvent(hWinEventHook);
		hWinEventHook = nullptr;
	}

	g_bEnabled = bEnable;
	UpdateIconTips();
}

//////////////////////////////////////////////////////////////////////////////
// main window procedure

LRESULT CALLBACK MainWndProc(
	HWND hwnd,
	UINT nMessage,
	WPARAM wParam, LPARAM lParam)
{
	LRESULT nResult = 0;
	switch (nMessage) {
	case WM_CREATE:
		nResult = DefWindowProc(hwnd, nMessage, wParam, lParam);

		g_hIcon = LoadIcon(
			g_hinstThis,
			MAKEINTRESOURCE(IDI_FOLDER_HSCROLLER));

		g_hIconFlash = LoadIcon(
			g_hinstThis,
			MAKEINTRESOURCE(IDI_FOLDER_HSCROLLER_FLASH));

		if (!RegisterTaskTray(hwnd)) {
			DestroyWindow(hwnd);
			break;
		}

		{
			const UINT uiMessage = RegisterWindowMessage(_T("TaskbarCreated"));

			if (uiMessage != 0)
			{
				WM_TASKBARCREATED = uiMessage;
			}
		}

		LoadString(g_hinstThis, IDS_DISABLED, g_szDisabled, MAX_LOADSTRING);
		SetHook(true);
		break;
	case WM_DESTROY:
		UnhookWinEvent(hWinEventHook);
		UnregisterTaskTray();

		if (g_hIcon) DestroyIcon(g_hIcon);
		if (g_hIconFlash) DestroyIcon(g_hIconFlash);

		nResult = DefWindowProc(hwnd, nMessage, wParam, lParam);
		PostQuitMessage(0);
		break;
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_ENDSESSION:
		CloseWindow(hwnd);
		break;
	case WM_NOTIFY_ICON:
		switch (lParam) {
		case WM_LBUTTONUP:
			SetHook(!g_bEnabled);
			break;
		case WM_RBUTTONUP:
			DoPopupMenu(hwnd);
			break;
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDM_APP_EXIT:
			DestroyWindow(hwnd);
			break;
		case IDM_APP_STOP:
			SetHook(!g_bEnabled);
			break;
		}
		break;
	default:
		if ((nMessage == WM_TASKBARCREATED) && (WM_TASKBARCREATED > 0)) {
			if (!RegisterTaskTray(hwnd)) {
				DestroyWindow(hwnd);
			}

			break;
		}

		nResult = DefWindowProc(hwnd, nMessage, wParam, lParam);
		break;
	}
	return nResult;
}

//////////////////////////////////////////////////////////////////////////////
// main

int WINAPI _tWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE /*hPrevInstance*/,
	_In_ LPTSTR /*pszCmdLine*/,
	_In_ int /*nShowCmd*/)
{
	g_hinstThis = hInstance;
	bool bKill = false;
	for (int i = 1; i < __argc; i++) {
		if (_tcscmp(__targv[i], _T("/noicon")) == 0) {
			g_bNoIcon = true;
		} else if (_tcscmp(__targv[i], _T("/monitor")) == 0) {
			g_bMonitoring = true;
		} else if (_tcscmp(__targv[i], _T("/kill")) == 0) {
			bKill = true;
		}
	}
	g_hEventTerminate = CreateEvent(NULL, TRUE, FALSE, g_szTerminateName);
	if (g_hEventTerminate == nullptr) {
		return 0;
	}
	if (bKill) {
		SetEvent(g_hEventTerminate);
		return 0;
	}
	g_hMutexSingleton = CreateMutex(NULL, TRUE, g_szSingletonName);
	if ((g_hMutexSingleton == nullptr) ||
		(GetLastError() == ERROR_ALREADY_EXISTS))
	{
		return 0;
	}
	WNDCLASS wc;
	std::memset(&wc, 0, sizeof(wc));
	wc.style = CS_VREDRAW | CS_HREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground =(HBRUSH)COLOR_WINDOW;
	wc.lpszClassName = g_szAppUniqueName;
	if (RegisterClass(&wc) == 0) {
		return 0;
	}
	g_hwndMain = CreateWindow(
		g_szAppUniqueName, g_szAppUIName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		nullptr, nullptr,
		hInstance, nullptr);
	if (g_hwndMain == nullptr) {
		return 0;
	}
	MSG msg;
	memset(&msg, 0, sizeof(msg));
	while (GetMessage(&msg, nullptr, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return static_cast<int>(msg.wParam);
}
