#pragma once
#include <tchar.h>
#include <Windows.h>

#define MAX_LOADSTRING 100

// Window Messages
#define WM_NOTIFY_ICON (WM_APP+100)
#define WM_ICON_FLASHEND (WM_NOTIFY_ICON+1)

namespace Constants
{
    static const TCHAR* APP_UI_NAME = _T("FolderHScroller");
    static const TCHAR* APP_UNIQUE_NAME = _T("PF:FolderHorizontalScroller");
    static const TCHAR* CLASSNAME_SYSTREEVIEW32 = _T("SysTreeView32");
    static const TCHAR* SINGLETON_NAME = _T("FolderHScroller:Singleton");
}

namespace LaunchOptions
{
    static const TCHAR* KILL = _T("/kill");
    static const TCHAR* MONITOR = _T("/monitor");
    static const TCHAR* NO_ICON = _T("/noicon");
}
