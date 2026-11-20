/* Windows-only module-relative paths. Never fall back to TEMP or cwd. */
#ifndef FASTGPS_SETTINGS_H
#define FASTGPS_SETTINGS_H
#include <stddef.h>
IMPORT U32 __stdcall GetModuleFileNameW(HANDLE,wchar_t *,U32);
IMPORT unsigned __stdcall GetPrivateProfileIntW(const wchar_t *,const wchar_t *,int,const wchar_t *);
static Settings settings;
static wchar_t sibling[32768];
static U32 folder_length;
static int module_folder(HANDLE module) {
    U32 n=GetModuleFileNameW(module,sibling,sizeof sibling/sizeof sibling[0]);
    folder_length=0;
    if(!n||n>=sizeof sibling/sizeof sibling[0])return 0;
    while(n&&sibling[n-1]!=L'\\'&&sibling[n-1]!=L'/')--n;
    folder_length=n;return n!=0;
}
static const wchar_t *sibling_file(const wchar_t *name) {
    U32 i=folder_length;
    if(!i)return 0;
    while(*name&&i+1<sizeof sibling/sizeof sibling[0])sibling[i++]=*name++;
    if(*name)return 0;
    sibling[i]=0;return sibling;
}
static void load_settings(HANDLE module) {
    const wchar_t *ini;
    settings=defaults;
    if(!module_folder(module)||(ini=sibling_file(L"FastGPS.ini"))==0)return;
#define READ(field,key) settings.field=GetPrivateProfileIntW(L"Routing",L##key,(int)defaults.field,ini)
    READ(prefer_forward,"PreferForward");READ(adaptive,"AdaptiveDetours");
    READ(percent,"DetourPercent");READ(max_extra,"MaxExtraDistance");
    READ(adaptive_percent,"AdaptiveMaxPercent");READ(adaptive_extra,"AdaptiveMaxExtraDistance");
    READ(preserve,"PreserveForwardRoute");
#undef READ
    settings.logging=GetPrivateProfileIntW(L"Diagnostics",L"Logging",(int)defaults.logging,ini);
    normalize_settings(&settings);
}
#endif
