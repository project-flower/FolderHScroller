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
#include "resource.h"

//////////////////////////////////////////////////////////////////////////////
// global variable

// Window Handles
HICON g_hIcon = nullptr;
HICON g_hIconFlash = nullptr;
HINSTANCE g_hinstThis = nullptr;
HANDLE g_hMutexSingleton = nullptr;
HWINEVENTHOOK g_hWinEventHook = nullptr;
HWND g_hwndMain = nullptr;

// Windows Structures
NOTIFYICONDATA g_nid;

// Flags
bool g_bEnabled = false;
bool g_bIconFlashing = false;
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

    if (IsWindowVisible(hwnd)) {
        // 一度ウィンドウに問い合わせを行わないと、
        // フォルダーペインの表示に支障が出る
        EnumChildWindows(hwnd, AnalyzeChildWindow, 0);
    }

    TurnoffIconAsync();
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

void SetIconTip(PNOTIFYICONDATA lpData)
{
    _tcscpy_s(lpData->szTip, Constants::APP_UI_NAME);

    if (!g_bEnabled) {
        _tcscat_s(lpData->szTip, _T(" ("));
        _tcscat_s(lpData->szTip, g_szDisabled);
        _tcscat_s(lpData->szTip, _T(")"));
    }
}

void CALLBACK TimerIconFlashEndProc(HWND unnamedParam1, UINT unnamedParam2, UINT_PTR unnamedParam3, DWORD unnamedParam4)
{
    TurnoffIcon();
}

void TurnoffIcon()
{
    if (!g_bMonitoring || g_bNoIcon) return;

    g_nid.hIcon = g_hIcon;
    Shell_NotifyIcon(NIM_MODIFY, &g_nid);
    g_bIconFlashing = false;
}

void TurnoffIconAsync()
{
    SetTimer(nullptr, WM_ICON_FLASHEND, 500, TimerIconFlashEndProc);
}

void TurnonIcon()
{
    if (g_bIconFlashing || !g_bMonitoring || g_bNoIcon) return;

    g_nid.hIcon = g_hIconFlash;
    Shell_NotifyIcon(NIM_MODIFY, &g_nid);
    g_bIconFlashing = true;
}

void UnregisterTaskTray()
{
    if (g_nid.cbSize) {
        Shell_NotifyIcon(NIM_DELETE, &g_nid);
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

bool DoPopupMenu(HWND hwnd)
{
    bool bResult = false;
    POINT ptMenu;
    GetCursorPos(&ptMenu);
    HMENU hmenuPopup = LoadMenu(
        g_hinstThis, MAKEINTRESOURCE(IDR_MENU_POPUP));

    if (hmenuPopup) {
        HMENU hmenuSub = GetSubMenu(hmenuPopup, 0);
        const UINT uCheck = (g_bEnabled ? MF_UNCHECKED : MF_CHECKED);
        CheckMenuItem(hmenuSub, IDM_APP_STOP, uCheck);

        if (hmenuSub) {
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

        DestroyMenu(hmenuPopup);
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
            EVENT_OBJECT_SHOW,
            EVENT_OBJECT_SHOW,
            nullptr,
            WinEventProc,
            0,
            0,
            (WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS));
        TurnonIcon();
        EnumWindows(EnumExplorerProc, 0);
        TurnoffIconAsync();
    }
    else {
        if (!g_hWinEventHook) {
            return;
        }

        UnhookWinEvent(g_hWinEventHook);
        g_hWinEventHook = nullptr;
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
        if (g_bKill) {
            EnumWindows(DestroyExistsProcess, 0);
            DestroyWindow(hwnd);
            return 0;
        }

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

            if (uiMessage)
            {
                WM_TASKBARCREATED = uiMessage;
            }
        }

        LoadString(g_hinstThis, IDS_DISABLED, g_szDisabled, MAX_LOADSTRING);
        SetHook(true);
        break;
    case WM_DESTROY:
        UnhookWinEvent(g_hWinEventHook);
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
