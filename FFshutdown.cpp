﻿// FFshutdown.cpp --- Log-off, restart, or shutdown Windows
// License: MIT
#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <tchar.h>

inline WORD get_lang_id(void)
{
    return PRIMARYLANGID(LANGIDFROMLCID(GetThreadLocale()));
}

// localization
LPCTSTR get_text(INT id)
{
#ifdef JAPAN
    if (get_lang_id() == LANG_JAPANESE) // Japone for Japone
    {
        switch (id)
        {
        case -1: return TEXT("FFshutdown バージョン 0.3 by 片山博文MZ");
        case 0:
            return TEXT("使用方法: FFshutdown [オプション]\n")
                   TEXT("オプション:\n")
                   TEXT("  -log-off     Windowsをログオフする。\n")
                   TEXT("  -restart     Windowsを再起動する。\n")
                   TEXT("  -shutdown    Windowsをシャットダウンする。\n")
                   TEXT("  -force       強制モード (非推奨)。\n")
                   TEXT("  -t 秒        タイムアウト (デフォルト: 10).\n")
                   TEXT("  -can_cancel  ユーザーキャンセルを有効にする。\n")
                   TEXT("  -help        このメッセージを表示する。\n")
                   TEXT("  -version     バージョン情報を表示する。");
        case 1: return TEXT("エラー: オプション -t は引数が必要です。");
        case 2: return TEXT("エラー: アクションが未指定です。\n");
        case 3: return TEXT("イベントオブジェクト作成に失敗しました。");
        case 4: return TEXT("FFshutdown");
        case 5: return TEXT("スレッド作成に失敗しました。");
        case 6: return TEXT("ログオフ");
        case 7: return TEXT("再起動");
        case 8: return TEXT("シャットダウン");
        case 9: return TEXT("システムは %s します（%d秒後）...");
        }
    }
    else // The others are Let's la English
#endif
    {
        switch (id)
        {
        case -1: return TEXT("FFshutdown version 0.3 by katahiromz");
        case 0:
            return TEXT("Usage: FFshutdown [Options]\n")
                   TEXT("Options:\n")
                   TEXT("  -log-off     Log off Windows.\n")
                   TEXT("  -restart     Restart Windows.\n")
                   TEXT("  -shutdown    Shutdown Windows.\n")
                   TEXT("  -force       Force mode (not recommended).\n")
                   TEXT("  -t SECONDS   The timeout (default: 10).\n")
                   TEXT("  -can_cancel  Enable user cancel.\n")
                   TEXT("  -help        Display this message.\n")
                   TEXT("  -version     Display version info.");
        case 1: return TEXT("ERROR: Option -t needs an argument.");
        case 2: return TEXT("ERROR: No action specified.\n");
        case 3: return TEXT("Failed to create an event object.");
        case 4: return TEXT("FFshutdown");
        case 5: return TEXT("Failed to create a thread.");
        case 6: return TEXT("log-off");
        case 7: return TEXT("reboot");
        case 8: return TEXT("shutdown");
        case 9: return TEXT("The system will %s in %d seconds...");
        }
    }

    assert(0);
    return nullptr;
}

void version(void)
{
    _putts(get_text(-1));
}

void usage(void)
{
    _putws(get_text(0));
}

BOOL EnableProcessPriviledge(LPCTSTR pszSE_)
{
    BOOL ret = FALSE;
    HANDLE hToken, hProcess = GetCurrentProcess();
    if (OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES, &hToken))
    {
        LUID luid;
        if (LookupPrivilegeValue(NULL, pszSE_, &luid))
        {
            TOKEN_PRIVILEGES tp;
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            tp.Privileges[0].Luid = luid;
            ret = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL);
        }
        CloseHandle(hToken);
    }
    return ret;
}

struct FFSHUTDOWN
{
    bool m_can_cancel;
    int m_timeout;
    enum ACTION {
        ACTION_NONE = 0,
        ACTION_LOGOFF,
        ACTION_REBOOT,
        ACTION_SHUTDOWN,
    };
    ACTION m_action;
    bool m_force;
    HANDLE m_hCancelEvent;

    FFSHUTDOWN()
    {
        m_can_cancel = false;
        m_timeout = 10;
        m_force = false;
        m_action = ACTION_NONE;
        m_hCancelEvent = NULL;
    }

    INT parse_cmdline(INT argc, LPWSTR *argv);
    INT run(HINSTANCE hInstance, INT nCmdShow);
    void exit_system();
};

INT FFSHUTDOWN::parse_cmdline(INT argc, LPWSTR *argv)
{
    for (INT iarg = 1; iarg < argc; ++iarg)
    {
        LPWSTR arg = argv[iarg];
        if (lstrcmpiW(arg, L"-help") == 0 || lstrcmpiW(arg, L"--help") == 0)
        {
            usage();
            return 1;
        }

        if (lstrcmpiW(arg, L"-version") == 0 || lstrcmpiW(arg, L"--version") == 0)
        {
            version();
            return 1;
        }

        if (lstrcmpiW(arg, L"-t") == 0)
        {
            arg = argv[++iarg];
            if (iarg < argc)
            {
                m_timeout = (INT)wcstoul(arg, NULL, 0);
                continue;
            }
            else
            {
                _ftprintf(stderr, get_text(1));
                return 1;
            }
        }

        if (lstrcmpiW(arg, L"-can_cancel") == 0 || lstrcmpiW(arg, L"--can_cancel") == 0)
        {
            m_can_cancel = true;
            continue;
        }

        if (lstrcmpiW(arg, L"-log-off") == 0 || lstrcmpiW(arg, L"--log-off") == 0)
        {
            m_action = ACTION_LOGOFF;
            continue;
        }

        if (lstrcmpiW(arg, L"-restart") == 0 || lstrcmpiW(arg, L"--restart") == 0)
        {
            m_action = ACTION_REBOOT;
            continue;
        }

        if (lstrcmpiW(arg, L"-shutdown") == 0 || lstrcmpiW(arg, L"--shutdown") == 0)
        {
            m_action = ACTION_SHUTDOWN;
            continue;
        }

        if (lstrcmpiW(arg, L"-force") == 0 || lstrcmpiW(arg, L"--force") == 0)
        {
            m_force = true;
            continue;
        }
    }

    if (m_action == ACTION_NONE)
    {
        _ftprintf(stderr, get_text(2));
        return 1;
    }

    return 0;
}

void FFSHUTDOWN::exit_system()
{
    WaitForSingleObject(m_hCancelEvent, m_timeout * 1000);
    EnableProcessPriviledge(SE_SHUTDOWN_NAME);
    switch (m_action)
    {
    case ACTION_NONE:
        break;
    case ACTION_LOGOFF:
        ExitWindowsEx(EWX_LOGOFF | (m_force ? EWX_FORCE : 0), 0);
        break;
    case ACTION_REBOOT:
        ExitWindowsEx(EWX_REBOOT | (m_force ? EWX_FORCE : 0), 0);
        break;
    case ACTION_SHUTDOWN:
        ExitWindowsEx(EWX_SHUTDOWN | (m_force ? EWX_FORCE : 0), 0);
        break;
    }
}

static DWORD WINAPI ShutdownThreadProc(LPVOID arg)
{
    FFSHUTDOWN *pThis = (FFSHUTDOWN *)arg;
    pThis->exit_system();
    return 0;
}

INT FFSHUTDOWN::run(HINSTANCE hInstance, INT nCmdShow)
{
    m_hCancelEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!m_hCancelEvent)
    {
        MessageBox(NULL, get_text(3), get_text(4), MB_ICONERROR);
        return 1;
    }

    HANDLE hThread = CreateThread(NULL, 0, ShutdownThreadProc, this, 0, NULL);
    if (!hThread)
    {
        MessageBox(NULL, get_text(5), get_text(4), MB_ICONERROR);
        CloseHandle(m_hCancelEvent);
        m_hCancelEvent = NULL;
        return 1;
    }

    FreeConsole();

    const TCHAR *action = NULL;
    switch (m_action)
    {
    case ACTION_NONE: assert(0); break;
    case ACTION_LOGOFF: action = get_text(6); break;
    case ACTION_REBOOT: action = get_text(7); break;
    case ACTION_SHUTDOWN: action = get_text(8); break;
    }

    TCHAR szText[256];
    wsprintf(szText, get_text(9), action, m_timeout);

    if (m_can_cancel)
    {
        if (MessageBox(NULL, szText, get_text(4), MB_ICONWARNING | MB_OKCANCEL) == IDCANCEL)
        {
            // Cancelled
            m_action = ACTION_NONE;
            PulseEvent(m_hCancelEvent);
            CloseHandle(m_hCancelEvent);
            m_hCancelEvent = NULL;
        }
    }
    else
    {
        MessageBox(NULL, szText, get_text(4), MB_ICONWARNING | MB_OK);
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);

    CloseHandle(m_hCancelEvent);
    m_hCancelEvent = NULL;

    return 0;
}

INT FFshutdown_main(
    HINSTANCE hInstance,
    INT argc,
    LPWSTR *argv,
    INT nCmdShow)
{
    FFSHUTDOWN ffshutdown;
    INT ret = ffshutdown.parse_cmdline(argc, argv);
    if (ret)
        return ret;

    return ffshutdown.run(hInstance, nCmdShow);
}

#include <clocale>

int main(void)
{
    // Unicode console output support
    std::setlocale(LC_ALL, "");

    INT argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    INT ret = FFshutdown_main(GetModuleHandle(NULL), argc, argv, SW_SHOWNORMAL);
    LocalFree(argv);
    return ret;
}
