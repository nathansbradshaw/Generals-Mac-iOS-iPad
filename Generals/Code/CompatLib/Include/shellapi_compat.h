#pragma once

// GeneralsX @build Win32 ShellExecute family → POSIX. Used only by the
// GeneralsOnline auto-updater path (non-MVP): opening a URL in the browser and
// launching an external patcher executable. On POSIX we can open URLs/files via
// the platform opener, but we cannot do Windows-style elevated ("runas") exec,
// so ShellExecuteExA reports failure and callers fall back to a message box.

#ifndef _WIN32

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "types_compat.h"

#if defined(__APPLE__)
#define GENERALSX_OPEN_CMD "open"
#else
#define GENERALSX_OPEN_CMD "xdg-open"
#endif

// Returns a value > 32 on success, matching the Win32 ShellExecuteA convention.
inline HINSTANCE ShellExecuteA(HWND /*hwnd*/, LPCSTR /*lpVerb*/, LPCSTR lpFile,
                               LPCSTR /*lpParameters*/, LPCSTR /*lpDirectory*/, int /*nShowCmd*/)
{
#if defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__)
    // GeneralsX @build BenderAI 11/07/2026 iOS forbids process creation and
    // this compatibility API is only used by the non-MVP browser/updater path.
    (void)lpFile;
    return (HINSTANCE)(uintptr_t)32;
#else
    if (lpFile == nullptr || strchr(lpFile, '\'') != nullptr)
        return (HINSTANCE)(uintptr_t)2; // SE_ERR_FNF-ish; refuse anything unquotable

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "%s '%s' >/dev/null 2>&1 &", GENERALSX_OPEN_CMD, lpFile);
    int rc = system(cmd);
    return (HINSTANCE)(uintptr_t)(rc == 0 ? 33 : 32);
#endif
}

typedef struct _SHELLEXECUTEINFOA
{
    DWORD   cbSize;
    ULONG   fMask;
    HWND    hwnd;
    LPCSTR  lpVerb;
    LPCSTR  lpFile;
    LPCSTR  lpParameters;
    LPCSTR  lpDirectory;
    int     nShow;
    HINSTANCE hInstApp;
    void*   lpIDList;
    LPCSTR  lpClass;
    HKEY    hkeyClass;
    DWORD   dwHotKey;
    HANDLE  hIcon;
    HANDLE  hProcess;
} SHELLEXECUTEINFOA;

// POSIX cannot perform an elevated ("runas") launch of an external Windows exe;
// report failure so the updater UI shows its fallback path.
inline BOOL ShellExecuteExA(SHELLEXECUTEINFOA* /*pExecInfo*/)
{
    return FALSE;
}

#endif // !_WIN32
