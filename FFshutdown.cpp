// FFshutdown.cpp --- Log-off, restart, or shutdown Windows
// License: MIT
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

void version(void)
{
    std::puts("FFshutdown version 0.0 by katahiromz");
}

void usage(void)
{
    std::puts(
        "Usage: FFshutdown [Options]\n"
        "Options:\n"
        "  -log-off     Log off Windows.\n"
        "  -restart     Restart Windows.\n"
        "  -shutdown    Shutdown Windows.\n"
        "  -force       Force mode (not recommended).\n"
        "  -t SECONDS   The timeout (default: 10).\n"
        "  -can-cancel  Enable user cancel.\n"
        "  -help        Display this message.\n"
        "  -version     Display version info.\n");
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
            return 0;
        }

        if (lstrcmpiW(arg, L"-version") == 0 || lstrcmpiW(arg, L"--version") == 0)
        {
            version();
            return 0;
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
                fprintf(stderr, "ERROR: Option -y needs an option.");
                return 1;
            }
        }

        if (lstrcmpiW(arg, L"-can-cancel") == 0 || lstrcmpiW(arg, L"--can-cancel") == 0)
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
        fprintf(stderr, "ERROR: No action specified.\n");
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
        MessageBoxA(NULL, "Failed to create an event object.", "FFshutdown", MB_ICONERROR);
        return 1;
    }

    HANDLE hThread = CreateThread(NULL, 0, ShutdownThreadProc, this, 0, NULL);
    if (!hThread)
    {
        MessageBoxA(NULL, "Failed to create a thread.", "FFshutdown", MB_ICONERROR);
        CloseHandle(m_hCancelEvent);
        m_hCancelEvent = NULL;
        return 1;
    }

    const char *action = NULL;
    switch (m_action)
    {
    case ACTION_NONE: assert(0); break;
    case ACTION_LOGOFF: action = "log-off"; break;
    case ACTION_REBOOT: action = "reboot"; break;
    case ACTION_SHUTDOWN: action = "shutdown"; break;
    }

    TCHAR szText[256];
    wsprintf(szText, TEXT("The system will %s in %d seconds."), action, m_timeout);

    if (m_can_cancel)
    {
        if (MessageBox(NULL, szText, TEXT("FFshutdown"), MB_ICONWARNING | MB_OKCANCEL) == IDCANCEL)
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
        MessageBox(NULL, szText, TEXT("FFshutdown"), MB_ICONWARNING | MB_OK);
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

int main(void)
{
    INT argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    INT ret = FFshutdown_main(GetModuleHandle(NULL), argc, argv, SW_SHOWNORMAL);
    LocalFree(argv);
    return ret;
}
