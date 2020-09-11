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

//////////////////////////////////////////////////////////////////////////////
// type

struct ChildInfo {
	const TCHAR* pszClassName;
	const TCHAR* pszCaption;
	int nID;
};

//////////////////////////////////////////////////////////////////////////////
// global constant

#define APP_UI_NAME         _T("FolderHScroller")
#define APP_UNIQUE_NAME     _T("Manuke:FolderHorizontalScroller")

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

bool g_bNoIcon = false;

NOTIFYICONDATA g_nid;

UINT WM_TASKBARCREATED = 0;

//////////////////////////////////////////////////////////////////////////////
// global function

void SetStyle(HWND);

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

auto CheckWndCaption = [](HWND hwnd, const TCHAR* pszCaption) -> bool {
	const int CAPTION_MAX = 128;
	TCHAR szCaption[CAPTION_MAX+1];
	szCaption[CAPTION_MAX] = _T('\0');
	if (GetWindowText(hwnd, szCaption, CAPTION_MAX) == 0) {
		szCaption[0] = _T('\0');
	}
	return _tcsicmp(szCaption, pszCaption) == 0;
};

HWND FindChild(HWND hwndParent, const ChildInfo* pChilds) {
	HWND hwndResult = nullptr;
	for (
		HWND hwndChild = GetWindow(hwndParent, GW_CHILD);
		hwndChild != nullptr;
		hwndChild = GetWindow(hwndChild, GW_HWNDNEXT))
	{
		if (IsWindowVisible(hwndChild) &&
			((pChilds->pszClassName == nullptr) ||
				CheckWndClassName(hwndChild, pChilds->pszClassName)) &&
			((pChilds->pszCaption == nullptr) ||
				CheckWndCaption(hwndChild, pChilds->pszCaption)) &&
			((pChilds->nID == -1) ||
				(GetDlgCtrlID(hwndChild) == pChilds->nID)))
		{
			if (((pChilds+1)->pszClassName == nullptr) &&
				((pChilds+1)->pszCaption == nullptr) &&
				((pChilds+1)->nID == -1))
			{
				hwndResult = hwndChild;
			} else {
				hwndResult = FindChild(hwndChild, pChilds+1);
			}
			if (hwndResult != nullptr) {
				break;
			}
		}
	}
	return hwndResult;
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

	if (!CheckWndClassName(hwnd, _T("SysTreeView32"))) {
		return;
	}

	SetStyle(hwnd);
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
	static const ChildInfo aciChilds[] = {
		{ _T("ShellTabWindowClass"), nullptr, 0 },
		{ _T("DUIViewWndClassName"), nullptr, 0 },
		{ _T("DirectUIHWND"), nullptr, 0 },
		{ _T("CtrlNotifySink"), nullptr, 0 },
		{ _T("NamespaceTreeControl"), nullptr, 0 },
		{ _T("SysTreeView32"), nullptr, 100 },
		{ nullptr, nullptr, -1 }
	};

	if (!IsWindowVisible(hwnd)) {
		return TRUE;
	}

	const int CLASSNAME_MAX = 128;
	TCHAR szClassName[CLASSNAME_MAX + 1];
	szClassName[CLASSNAME_MAX] = _T('\0');

	if (GetClassName(hwnd, szClassName, CLASSNAME_MAX) == 0) {
		return TRUE;
	}

	HWND hwndTree = nullptr;

	if (_tcsicmp(szClassName, _T("CabinetWClass")) == 0) {
		hwndTree = FindChild(hwnd, aciChilds);
	}
	else if (_tcsicmp(szClassName, _T("#32770")) == 0) {
		hwndTree = FindChild(hwnd, &aciChilds[1]);
	}

	if (hwndTree != nullptr) {
		SetStyle(hwndTree);
	}

	return TRUE;
}

auto AdjustExplorer = []() {
	EnumWindows(EnumExplorerProc, 0);
};

//////////////////////////////////////////////////////////////////////////////
// tasktray operation

auto RegisterTaskTray = [](HWND hwnd) {
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
		_tcscpy_s(g_nid.szTip, g_szAppUIName);
		if (!Shell_NotifyIcon(NIM_ADD, &g_nid)) {
			memset(&g_nid, 0, sizeof(g_nid));
		}
	}
};

auto UnregisterTaskTray = []() {
	if (g_nid.cbSize != 0) {
		Shell_NotifyIcon(NIM_DELETE, &g_nid);
	}
};

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
		RegisterTaskTray(hwnd);

		{
			const UINT uiMessage = RegisterWindowMessage(_T("TaskbarCreated"));

			if (uiMessage != 0)
			{
				WM_TASKBARCREATED = uiMessage;
			}
		}

		hWinEventHook = SetWinEventHook(
			EVENT_OBJECT_CREATE,
			EVENT_OBJECT_CREATE,
			nullptr,
			WinEventProc,
			0,
			0,
			(WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS));
		AdjustExplorer();
		break;
	case WM_DESTROY:
		UnhookWinEvent(hWinEventHook);
		UnregisterTaskTray();
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
		}
		break;
	default:
		if ((nMessage == WM_TASKBARCREATED) && (WM_TASKBARCREATED > 0)) {
			RegisterTaskTray(hwnd);
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
