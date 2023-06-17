#pragma once
#include <tchar.h>
#include <Windows.h>

#define APP_UI_NAME _T("FolderHScroller")
#define APP_UNIQUE_NAME _T("PF:FolderHorizontalScroller")
#define CLASSNAME_SYSTREEVIEW32 _T("SysTreeView32")
#define MAX_LOADSTRING 100

// Window Messages
#define WM_NOTIFY_ICON (WM_APP+100)
#define WM_ICON_FLASHEND (WM_NOTIFY_ICON+1)

namespace Constants
{
    const TCHAR g_szAppUIName[] = APP_UI_NAME;
    const TCHAR g_szAppUniqueName[] = APP_UNIQUE_NAME;
    const TCHAR g_szSingletonName[] = APP_UNIQUE_NAME _T(":Singleton");
}
