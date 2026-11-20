#include <assert.h>
#include <stdio.h>
#include <wchar.h>
#include "adaptive.h"
#define IMPORT
#define __stdcall
#define GetModuleFileNameW mock_module_path
#define GetPrivateProfileIntW mock_ini_int
typedef unsigned long U32;
typedef void *HANDLE;
#include "settings.h"
static const wchar_t *mock_path;
static unsigned profile_calls, enable_logging;
U32 mock_module_path(HANDLE module,wchar_t *out,U32 capacity) {
    U32 n;(void)module;
    if(!mock_path)return 0;
    n=(U32)wcslen(mock_path);if(n>=capacity)return capacity;
    wcscpy(out,mock_path);return n;
}
unsigned mock_ini_int(const wchar_t *section,const wchar_t *key,int fallback,const wchar_t *path) {
    assert(wcscmp(path,L"C:\\GTA IV\\scripts\\\u00f6\\FastGPS.ini")==0);
    ++profile_calls;
    if(!wcscmp(section,L"Routing")&&!wcscmp(key,L"DetourPercent"))return 80;
    if(!wcscmp(section,L"Diagnostics")&&!wcscmp(key,L"Logging"))return enable_logging;
    return (unsigned)fallback;
}
int main(void) {
    mock_path=L"C:\\GTA IV\\scripts\\\u00f6\\FastGPS.asi";
    load_settings(0);
    assert(profile_calls==8&&settings.percent==80&&settings.logging==0);
    assert(!wcscmp(sibling_file(L"GTAIV-FastGPS.log"),L"C:\\GTA IV\\scripts\\\u00f6\\GTAIV-FastGPS.log"));
    assert(!wcscmp(sibling_file(L"FastGPS.ini"),L"C:\\GTA IV\\scripts\\\u00f6\\FastGPS.ini"));
    puts("PASS INI and log resolve beside nested Unicode module path");
    enable_logging=1;load_settings(0);assert(settings.logging==1);
    mock_path=L"FastGPS.asi";load_settings(0);
    assert(profile_calls==16&&settings.percent==50&&settings.logging==0&&!sibling_file(L"FastGPS.ini"));
    /* Simulate API failure after an earlier valid path. */
    mock_path=0;load_settings(0);assert(!sibling_file(L"GTAIV-FastGPS.log"));
    puts("PASS no working-directory or temporary-folder fallback");
    return 0;
}
