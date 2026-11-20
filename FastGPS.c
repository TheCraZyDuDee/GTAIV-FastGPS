/* GTA IV Complete Edition 1.2.0.59 x86. Runtime-only GPS changes. */
#include "tracker.h"
#if !defined(_M_IX86)
#error Build with an x86 Windows MSVC-compatible compiler.
#endif
typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned long U32;
typedef void *HANDLE;
int _fltused=0; /* No CRT; floating-point operations are emitted inline. */
#define IMPORT __declspec(dllimport)
IMPORT HANDLE __stdcall GetModuleHandleA(const char *);
IMPORT int __stdcall GetModuleHandleExA(U32,const char *,HANDLE *);
IMPORT int __stdcall VirtualProtect(void *,U32,U32,U32 *);
IMPORT int __stdcall FlushInstructionCache(HANDLE,const void *,U32);
IMPORT HANDLE __stdcall GetCurrentProcess(void);
IMPORT void __stdcall OutputDebugStringA(const char *);
#define DELAY 0x537884UL
#define HOOK 0x5378E9UL
#define DETECT 0x537AEBUL
#define TIMER 0xD735B4UL
#define STATE 0xD64
#define STAMP 0xD68
#define COUNT 0xD60
#define TARGET 0xD50
static U8 *game;
static void (__thiscall *native_update)(void *);
static void (__cdecl *get_pos)(Point *);
static void *(__cdecl *get_player)(void);
static struct Slot { void *owner; Tracker tracking; } slots[4];
static unsigned replacement;
static U16 r16(const U8 *p) { return (U16)(p[0]|((U16)p[1]<<8)); }
static U32 r32(const U8 *p) { return (U32)p[0]|((U32)p[1]<<8)|((U32)p[2]<<16)|((U32)p[3]<<24); }
static void w32(U8 *p,U32 x) { p[0]=(U8)x;p[1]=(U8)(x>>8);p[2]=(U8)(x>>16);p[3]=(U8)(x>>24); }
static int match(const U8 *p,const U8 *e,U32 n) { U32 i;for(i=0;i<n;i++)if(p[i]!=e[i])return 0;return 1; }
static Tracker *slot(void *owner) {
    unsigned i; struct Slot *s;
    for(i=0;i<4;i++)if(slots[i].owner==owner)return &slots[i].tracking;
    for(i=0;i<4;i++)if(!slots[i].owner)break;
    if(i==4)i=replacement++&3;
    s=&slots[i]; s->owner=owner;
    /* Avoid CRT memset dependency. Other fields become initialized on first use. */
    s->tracking.valid=0;s->tracking.has_target=0;s->tracking.locked=0;
    s->tracking.bad=0;s->tracking.good=0;s->tracking.evidence=0;s->tracking.last_force=0;
    return &s->tracking;
}
/* Original call is thiscall, ECX=self and no stack arguments. Replacement
   fastcall consumes ECX, ignores EDX and uses the same return convention. */
static void __fastcall update(U8 *self,void *unused) {
    Tracker *s; Point p; int reason,count; U32 now;
    (void)unused;
    native_update(self);
    s=slot(self);
    if(*(int *)(self+STATE)!=3 || !get_player()) { tracker_pause(s); return; }
    count=*(int *)(self+COUNT);
    if(count<2 || count>200) { tracker_pause(s);return; }
    p.w=0;get_pos(&p); now=*(U32 *)(game+TIMER);
    reason=tracker_update(s,p,*(Point *)(self+TARGET),(const Point *)self,count,
                          game[0xDA2EA3]!=0,now);
    if(reason) {
        /* Match vanilla's off-route state transition; no new path job or
           route buffer mutation. Native logic handles it on the next update. */
        *(U32 *)(self+STATE)=1; *(U32 *)(self+STAMP)=now;
        tracker_pause(s);
        OutputDebugStringA(reason==REVERSE?"[FastGPS v0.3] Reverse-progress reroute.\n":
                                          "[FastGPS v0.3] Predicted-departure reroute.\n");
    }
}
static int apply_patch(void *module) {
    static const U8 gate[]={0x83,0xBE,0x64,0x0D,0,0,2,0x74,0x1C,0xF6,0x86,0x78,0x0D,0,0,1,0x75,0x13,0x8B,0x86,0x68,0x0D,0,0,0x05,0xB8,0x0B,0,0,0x39,0x05};
    static const U8 tail[]={0x76,0x58,0xFF,0xB6,0x70,0x0D,0,0,0xB9};
    static const U8 caller[]={0x8B,0xCE,0xE8,0x32,0,0,0,0x80,0x7D,0x0C,0,0x74,0x18};
    static const U8 entry[]={0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x83,0xEC,0x44,0x53,0x8B,0xD9,0x56,0x83,0xBB,0x64,0x0D,0,0,3,0x57,0x0F,0x85,0xDE,1,0,0};
    static const U8 cmp[]={0x0F,0x2F,0x0D};
    static const U8 branch[]={0x77,0x13,0x85,0xD2,0x7E,0x24,0x52,0x8B,0xCB,0xE8,0x23,0xEF,0xFF,0xFF};
    static const U8 off[]={0xC7,0x83,0x64,0x0D,0,0,1,0,0,0,0xA1};
    static const U8 stamp[]={0x89,0x83,0x68,0x0D,0,0};
    static const U8 position[]={0xE8,0xBB,0xFF,0xFF,0xFF,0x85,0xC0,0x75,0x19};
    U8 *b=(U8 *)GetModuleHandleA(0),*pe,*g,*d,*h,*o; U32 n,old,ignored; HANDLE pin;
    (void)module;
    if(!b || ((U32)b&0xffff) || r16(b)!=0x5A4D)return 0;
    n=r32(b+0x3C);if(n<0x40||n>0x800)return 0;pe=b+n;
    if(r32(pe)!=0x4550||r16(pe+4)!=0x14C||r32(pe+8)!=0x63D3E735||
       r16(pe+24)!=0x10B||r32(pe+80)!=0x01BE6400)return 0;
    g=b+0x53786A;d=b+DETECT;h=b+HOOK;o=b+0x537B04;
    if(!match(g,gate,sizeof gate)||r32(g+31)!=(U32)b+TIMER||!match(g+35,tail,sizeof tail)||
       r32(g+44)!=(U32)b+0xD77A80||!match(h-2,caller,sizeof caller)||
       !match(b+0x537920,entry,sizeof entry)||!match(d-3,cmp,sizeof cmp)||
       r32(d)!=(U32)b+0xBE8B68||!match(d+4,branch,sizeof branch)||
       !match(o,off,sizeof off)||r32(o+11)!=(U32)b+TIMER||!match(o+15,stamp,sizeof stamp)||
       r32(b+0xBE8B68)!=0x42480000||r32(b+0xBE8B38)!=0x41A00000||
       !match(b+0x53F090,position,sizeof position)||b[0x53F050]!=0xA1||
       r32(b+0x53F051)!=(U32)b+0xC36F14)return 0;
    game=b; native_update=(void (__thiscall *)(void *))(b+0x537920);
    get_pos=(void (__cdecl *)(Point *))(b+0x53F090);
    get_player=(void *(__cdecl *)(void))(b+0x53F050);
    /* Pin ASI for the lifetime of the process: the call target must stay mapped. */
    if(!GetModuleHandleExA(5,(const char *)&update,&pin))return 0;
    n=DETECT-DELAY+1;
    if(!VirtualProtect(b+DELAY,n,0x40,&old))return 0;
    b[DELAY]=0; *d=0x38; w32(h+1,(U32)&update-(U32)(h+5));
    if(!FlushInstructionCache(GetCurrentProcess(),b+DELAY,n)||
       !VirtualProtect(b+DELAY,n,old,&ignored)) {
        w32(h+1,0x32);*d=0x68;b[DELAY]=0x0B;
        FlushInstructionCache(GetCurrentProcess(),b+DELAY,n);
        VirtualProtect(b+DELAY,n,old,&ignored);return 0;
    }
    return 1;
}
int __stdcall DllMain(HANDLE module,U32 reason,void *reserved) {
    (void)reserved;
    if(reason==1)OutputDebugStringA(apply_patch(module)?"[FastGPS v0.3] Motion-aware GPS active.\n":
        "[FastGPS v0.3] Not applied: unsupported build, conflicting patch or protection failure.\n");
    return 1;
}
