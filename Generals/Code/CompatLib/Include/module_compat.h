#pragma once

class CComModule
{
  public:
    void Init(void*, HINSTANCE hInstance)
    {
      m_hInstance = hInstance;
    }
    void Term() {}

  private:
    HINSTANCE m_hInstance;
};

bool GetModuleFileName(HINSTANCE hInstance, char* buffer, int size);

typedef uintptr_t (*FARPROC)();
typedef HANDLE HMODULE;

HMODULE LoadLibrary(const char* lpFileName);
FARPROC GetProcAddress(HMODULE hModule, const char* lpProcName);
void FreeLibrary(HMODULE hModule);

// GeneralsX @build ANSI-suffixed aliases (Windows resolves *A to the char variant)
#include <stdlib.h>
inline HMODULE LoadLibraryA(const char* lpFileName) { return LoadLibrary(lpFileName); }
inline bool SetEnvironmentVariableA(const char* name, const char* value)
{
    return setenv(name, value ? value : "", 1) == 0;
}