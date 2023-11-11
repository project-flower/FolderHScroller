//////////////////////////////////////////////////////////////////////////////
// version macro

#define _WIN32_IE 0x0500

//////////////////////////////////////////////////////////////////////////////
// include

#include <cstdlib>
#include <vector>

#include <Windows.h>
#include <CommCtrl.h>

#include "Constants.h"
#include "FolderHScroller.h"
#include "Inlines.hpp"
#include "resource.h"

//////////////////////////////////////////////////////////////////////////////
// global variable

// Window Handles
HICON g_hIcon = nullptr;
HICON g_hIconDisabled = nullptr;
HICON g_hIconFlash = nullptr;
HINSTANCE g_hinstThis = nullptr;
HMENU g_hMenuPopup = nullptr;
HANDLE g_hMutexSingleton = nullptr;
HWINEVENTHOOK g_hWinEventHook = nullptr;
HWND g_hwndMain = nullptr;

// Windows Structures
NOTIFYICONDATA g_nid;

// Flags
bool g_bEnabled = false;
bool g_bIconFlashing = false;
bool g_bIconVisible = false;
bool g_bKill = false;
bool g_bMonitoring = false;
bool g_bNoIcon = false;

// Windows Messages
UINT WM_TASKBARCREATED = 0;

// Messages
TCHAR g_szDisabled[MAX_LOADSTRING];

//////////////////////////////////////////////////////////////////////////////
// window operation

bool CheckWndClassName(HWND hwnd, const TCHAR* pszClassName)
{
    const int CLASSNAME_MAX = 128;
    TCHAR szClassName[CLASSNAME_MAX+1];
    szClassName[CLASSNAME_MAX] = _T('\0');

    if (!GetClassName(hwnd, szClassName, CLASSNAME_MAX)) {
        szClassName[0] = _T('\0');
    }

    return !_tcsicmp(szClassName, pszClassName);
}

BOOL CALLBACK DestroyExistsProcess(HWND hwnd, LPARAM lParam)
{
    if (hwnd == g_hwndMain) {
        return TRUE;
    }

    static TCHAR szClassName[MAX_LOADSTRING];

    if (!GetClassName(hwnd, szClassName, MAX_LOADSTRING)) {
        return TRUE;
    }

    if (_tcscmp(szClassName, Constants::APP_UNIQUE_NAME)) {
        return TRUE;
    }

    PostMessage(hwnd, WM_DESTROY, 0, 0);
    return TRUE;
}

BOOL CALLBACK AnalyzeChildWindow(HWND hwnd, LPARAM lParam)
{
    if (CheckWndClassName(hwnd, Constants::CLASSNAME_SYSTREEVIEW32)) {
        SetStyle(hwnd);
    }

    // 複数存在する可能性もあるので続行する
    return TRUE;
}

VOID CALLBACK WinEventProc(
    HWINEVENTHOOK hWinEventHook,
    DWORD         event,
    HWND          hwnd,
    LONG          idObject,
    LONG          idChild,
    DWORD         idEventThread,
    DWORD         dwmsEventTime)
{
    TurnonIcon();
    // 一度ウィンドウ プロシージャを呼び出さないと
    // フォルダーペインの表示に支障が出る
    // SendMessage では応答しないハンドルがある
    DWORD_PTR dwResult;

    if (SendMessageTimeout(hwnd, WM_NULL, 0, 0, 0, 5000, &dwResult)) {
        EnumChildWindows(hwnd, AnalyzeChildWindow, 0);
    }

    TurnoffIcon();
}

//////////////////////////////////////////////////////////////////////////////
// explorer operation

BOOL CALLBACK EnumExplorerProc(HWND hwnd, LPARAM lParam)
{
    EnumChildWindows(hwnd, AnalyzeChildWindow, 0);
    return TRUE;
}

void SetStyle(HWND hwnd)
{
    LONG_PTR nStyle = GetWindowLongPtr(hwnd, GWL_STYLE);

    if (nStyle & TVS_NOHSCROLL) {
        nStyle &= ~TVS_NOHSCROLL;
        SetWindowLongPtr(hwnd, GWL_STYLE, nStyle);
    }
}

//////////////////////////////////////////////////////////////////////////////
// tasktray operation

bool RegisterTaskTray(HWND hwnd)
{
    memset(&g_nid, 0, sizeof(g_nid));

    if (!g_bNoIcon) {
        g_nid.cbSize = sizeof(g_nid);
        g_nid.hWnd = hwnd;
        g_nid.uID = 1;
        g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        g_nid.uCallbackMessage = WM_NOTIFY_ICON;
        g_nid.hIcon = LoadIcon(
            g_hinstThis,
            MAKEINTRESOURCE(IDI_FOLDER_HSCROLLER));
        SetIconData(&g_nid);
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

        g_bIconVisible = bResult;

        if (!bResult) {
            memset(&g_nid, 0, sizeof(g_nid));
        }

        return bResult;
    }
    else {
        return true;
    }
}

void SetIconData(PNOTIFYICONDATA lpData)
{
    _tcscpy_s(lpData->szTip, Constants::APP_UI_NAME);

    if (!g_bEnabled) {
        _tcscat_s(lpData->szTip, _T(" ("));
        _tcscat_s(lpData->szTip, g_szDisabled);
        _tcscat_s(lpData->szTip, _T(")"));
        lpData->hIcon = g_hIconDisabled;
    }
    else {
        lpData->hIcon = g_hIcon;
    }
}

void TurnoffIcon()
{
    if (!g_bMonitoring || !g_bIconVisible) return;

    g_nid.hIcon = g_hIcon;
    UpdateIcon();
    PostMessage(nullptr, WM_NULL, 0, 0);
    g_bIconFlashing = false;
}

void TurnonIcon()
{
    if (g_bIconFlashing || !g_bMonitoring || !g_bIconVisible) return;

    g_nid.hIcon = g_hIconFlash;
    UpdateIcon();
    PostMessage(nullptr, WM_NULL, 0, 0);
    g_bIconFlashing = true;
}

void UnregisterTaskTray()
{
    if (g_nid.cbSize) {
        Shell_NotifyIcon(NIM_DELETE, &g_nid);
    }
}

void UpdateIcon()
{
    if (g_bIconVisible) {
        Shell_NotifyIcon(NIM_MODIFY, &g_nid);
    }
}

void UpdateIconData()
{
    if (g_bNoIcon) {
        return;
    }

    SetIconData(&g_nid);
    UpdateIcon();
}

//////////////////////////////////////////////////////////////////////////////
// menu

bool DoPopupMenu(HWND hwnd)
{
    bool bResult = false;
    POINT ptMenu;
    GetCursorPos(&ptMenu);

    if (g_hMenuPopup) {
        HMENU hmenuSub = GetSubMenu(g_hMenuPopup, 0);

        if (!hmenuSub) {
            return false;
        }

        const UINT uCheck = (g_bEnabled ? MF_UNCHECKED : MF_CHECKED);
        CheckMenuItem(hmenuSub, IDM_APP_STOP, uCheck);

        SetForegroundWindow(hwnd);

        bResult = TrackPopupMenu(
            hmenuSub,
            TPM_LEFTALIGN | TPM_RIGHTBUTTON,
            ptMenu.x, ptMenu.y,
            0,
            hwnd,
            nullptr);

        PostMessage(hwnd, WM_NULL, 0, 0);
    }

    return bResult;
}

void SetHook(bool bEnable)
{
    if (bEnable) {
        if (g_hWinEventHook) {
            return;
        }

        g_hWinEventHook = SetWinEventHook(
            // EVENT_OBJECT_SHOW はタスク マネージャ等で大量のイベントが発生してしまう
            EVENT_OBJECT_CREATE,
            EVENT_OBJECT_CREATE,
            nullptr,
            WinEventProc,
            0,
            0,
            (WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS));
        TurnonIcon();
        EnumWindows(EnumExplorerProc, 0);
        TurnoffIcon();
    }
    else {
        if (!g_hWinEventHook) {
            return;
        }

        UnhookWinEvent(g_hWinEventHook);
        g_hWinEventHook = nullptr;
    }

    g_bEnabled = bEnable;
    UpdateIconData();
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
        if (g_bKill) {
            EnumWindows(DestroyExistsProcess, 0);
            DestroyWindow(hwnd);
            return 0;
        }

        nResult = DefWindowProc(hwnd, nMessage, wParam, lParam);

        g_hMenuPopup = LoadMenu(
            g_hinstThis, MAKEINTRESOURCE(IDR_MENU_POPUP));

        g_hIcon = LoadIcon(
            g_hinstThis,
            MAKEINTRESOURCE(IDI_FOLDER_HSCROLLER));

        g_hIconDisabled = LoadIcon(
            g_hinstThis,
            MAKEINTRESOURCE(IDI_FOLDER_HSCROLLER_DISABLED));

        g_hIconFlash = LoadIcon(
            g_hinstThis,
            MAKEINTRESOURCE(IDI_FOLDER_HSCROLLER_FLASH));

        {
            const UINT uiMessage = RegisterWindowMessage(_T("TaskbarCreated"));

            if (uiMessage) {
                WM_TASKBARCREATED = uiMessage;
            }
        }

        LoadString(g_hinstThis, IDS_DISABLED, g_szDisabled, MAX_LOADSTRING);
        SetHook(true);

        if (!RegisterTaskTray(hwnd)) {
            DestroyWindow(hwnd);
        }

        break;
    case WM_DESTROY:
        UnhookWinEvent(g_hWinEventHook);
        UnregisterTaskTray();
        ExecIfNotNull(DestroyIcon, g_hIcon);
        ExecIfNotNull(DestroyIcon, g_hIconDisabled);
        ExecIfNotNull(DestroyIcon, g_hIconFlash);
        ExecIfNotNull(DestroyMenu, g_hMenuPopup);
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
            g_bIconVisible = false;

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

    for (int i = 1; i < __argc; ++i) {
        const TCHAR* szArgument = __targv[i];

        if (!_tcscmp(szArgument, LaunchOptions::NO_ICON)) {
            g_bNoIcon = true;
        }
        else if (!_tcscmp(szArgument, LaunchOptions::MONITOR)) {
            g_bMonitoring = true;
        }
        else if (!_tcscmp(szArgument, LaunchOptions::KILL)) {
            g_bKill = true;
        }
    }

    g_hMutexSingleton = CreateMutex(nullptr, FALSE, Constants::SINGLETON_NAME);

    if (!g_hMutexSingleton) {
        return 0;
    }
    else if ((GetLastError() == ERROR_ALREADY_EXISTS) && !g_bKill) {
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
    wc.lpszClassName = Constants::APP_UNIQUE_NAME;

    if (!RegisterClass(&wc)) {
        return 0;
    }

    g_hwndMain = CreateWindow(
        Constants::APP_UNIQUE_NAME, Constants::APP_UI_NAME,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr,
        hInstance, nullptr);

    if (!g_hwndMain) {
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
