/* GTA IV Complete Edition 1.2.0.59 x86. Runtime-only GPS changes. */
#include "direction_policy.h"
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
IMPORT U32 __stdcall GetCurrentThreadId(void);
IMPORT U32 __stdcall GetTickCount(void);
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
#include "diagnostics.h"
static RejectedDirection rejected[4];
static U32 last_sample[4], last_wait[3];
static unsigned have_wait[3];
static U16 r16(const U8 *p) { return (U16)(p[0]|((U16)p[1]<<8)); }
static U32 r32(const U8 *p) { return (U32)p[0]|((U32)p[1]<<8)|((U32)p[2]<<16)|((U32)p[3]<<24); }
static void w32(U8 *p,U32 x) { p[0]=(U8)x;p[1]=(U8)(x>>8);p[2]=(U8)(x>>16);p[3]=(U8)(x>>24); }
static int match(const U8 *p,const U8 *e,U32 n) { U32 i;for(i=0;i<n;i++)if(p[i]!=e[i])return 0;return 1; }
static Tracker *slot(void *owner) {
    unsigned i; struct Slot *s;
    for(i=0;i<4;i++)if(slots[i].owner==owner)return &slots[i].tracking;
    for(i=0;i<4;i++)if(!slots[i].owner)break;
    if(i==4)i=replacement++&3;
    s=&slots[i]; s->owner=owner; rejected[i].valid=0; last_sample[i]=0;
    /* Avoid CRT memset dependency. Other fields become initialized on first use. */
    s->tracking.valid=0;s->tracking.has_target=0;s->tracking.locked=0;
    s->tracking.bad=0;s->tracking.good=0;s->tracking.evidence=0;s->tracking.last_force=0;
    return &s->tracking;
}
/* The native pathfinder propagates costs backwards from the destination,
   then extracts the path forwards. Both passes must exclude the same edge. */
static volatile int bias_active;
static U32 bias_thread, bias_blocked;
static Direction bias_direction;
static U8 *relax_continue,*relax_skip,*extract_continue,*extract_skip;
static Point node_pos(const U8 *n) {
    Point p;p.x=(short)r16(n+0x14)*0.125f;p.y=(short)r16(n+0x16)*0.125f;
    p.z=(short)r16(n+0x18)*0.015625f;p.w=0;return p;
}
static __declspec(noinline) int __cdecl blocked_edge(const U8 *from,const U8 *to) {
    if(!bias_active||GetCurrentThreadId()!=bias_thread)return 0;
    if(!forbidden_edge(&bias_direction,node_pos(from),node_pos(to)))return 0;
    ++bias_blocked;return 1;
}
/* Exact original instruction replay on allowed edges. Preserve all GPRs,
   EFLAGS and XMM registers at both mid-function detours. The original ESP
   is 180 bytes above the save area (144 + pushad 32 + pushfd 4). */
static __declspec(naked) void relax_hook(void) {
    __asm {
        pushfd
        cmp dword ptr [bias_active],0
        je relax_allowed
        pushad
        sub esp,144
        movdqu [esp],xmm0
        movdqu [esp+16],xmm1
        movdqu [esp+32],xmm2
        movdqu [esp+48],xmm3
        movdqu [esp+64],xmm4
        movdqu [esp+80],xmm5
        movdqu [esp+96],xmm6
        movdqu [esp+112],xmm7
        stmxcsr [esp+128]
        mov eax,[esp+220] /* original [esp+0x28]: current destination-side node */
        push eax
        push esi /* predecessor: actual forward source */
        call blocked_edge
        add esp,8
        test eax,eax
        movdqu xmm0,[esp]
        movdqu xmm1,[esp+16]
        movdqu xmm2,[esp+32]
        movdqu xmm3,[esp+48]
        movdqu xmm4,[esp+64]
        movdqu xmm5,[esp+80]
        movdqu xmm6,[esp+96]
        movdqu xmm7,[esp+112]
        ldmxcsr [esp+128]
        lea esp,[esp+144]
        popad
        jnz relax_blocked
    relax_allowed:
        popfd
        movzx ecx,word ptr [esi+0x10]
        movsx eax,cx
        jmp dword ptr [relax_continue]
    relax_blocked:
        popfd
        jmp dword ptr [relax_skip]
    }
}
static __declspec(naked) void extract_hook(void) {
    __asm {
        pushfd
        cmp dword ptr [bias_active],0
        je extract_allowed
        pushad
        sub esp,144
        movdqu [esp],xmm0
        movdqu [esp+16],xmm1
        movdqu [esp+32],xmm2
        movdqu [esp+48],xmm3
        movdqu [esp+64],xmm4
        movdqu [esp+80],xmm5
        movdqu [esp+96],xmm6
        movdqu [esp+112],xmm7
        stmxcsr [esp+128]
        mov eax,[esp+200] /* original [esp+0x14]: current start-side node */
        push edx /* neighbor: actual forward destination */
        push eax
        call blocked_edge
        add esp,8
        test eax,eax
        movdqu xmm0,[esp]
        movdqu xmm1,[esp+16]
        movdqu xmm2,[esp+32]
        movdqu xmm3,[esp+48]
        movdqu xmm4,[esp+64]
        movdqu xmm5,[esp+80]
        movdqu xmm6,[esp+96]
        movdqu xmm7,[esp+112]
        ldmxcsr [esp+128]
        lea esp,[esp+144]
        popad
        jnz extract_blocked
    extract_allowed:
        popfd
        mov ecx,[esp+0x14]
        movsx ecx,word ptr [ecx+0x10]
        jmp dword ptr [extract_continue]
    extract_blocked:
        popfd
        jmp dword ptr [extract_skip]
    }
}
typedef void (__thiscall *BuildRoute)(U8 *,Point *,Point *,U8 *,int,int *,U32);
static RoadMotion motion;
static unsigned slot_index(void *owner) {
    unsigned i;slot(owner);for(i=0;i<4;i++)if(slots[i].owner==owner)return i;return 4;
}
static void copy_point(volatile Point *d,const volatile Point *s) {
    d->x=s->x;d->y=s->y;d->z=s->z;d->w=s->w;
}
static void log_bias(U32 count,U32 elapsed) {
    char b[128],*p=log_text(b,"bias_stats blocked_edges=");p=log_uint(p,count);
    p=log_text(p," wall_ms=");p=log_uint(p,elapsed);log_send(b,p);
}
static void __fastcall rebuild(U8 *graph,void *unused,Point *dest,Point *route,U8 *flags,int max,int *count,U32 mode) {
    BuildRoute native=(BuildRoute)(game+0x4EB600);
    U8 *self=(U8 *)route; U32 now=*(U32 *)(game+TIMER),start;
    int pending=*(int *)(self+STATE)==1,n,i,reverse,verdict;float progress;
    unsigned index=slot_index(self);Point p={0}; Direction d;
    Point saved[200];U8 saved_flags[200];
    (void)unused;
    if(get_player())get_pos(&p);
    if(pending)log_route("build_before",self,p);
    native(graph,dest,route,flags,max,count,mode);
    n=*count;
    if(pending&&index<4&&max>=2&&max<=200&&n>=2&&n<=max&&motion.moving&&now-motion.last<=350&&
       get_player()&&((void *(__cdecl *)(void))(game+0x53F1C0))()&&
       direction_init(&d,p,motion.dx,motion.dy)) {
        if(!start_progress(&d,route,n,&progress,&reverse)) {
            rejected[index].valid=0;log_event("baseline_direction_unknown");
        } else if(!reverse && progress>= -6) {
            rejected[index].valid=0;log_event("baseline_not_backward");
        } else if(retry_allowed(&rejected[index],&d,*dest)) {
            log_route("baseline_backward",self,p);
            for(i=0;i<n;i++){copy_point(&saved[i],&route[i]);saved_flags[i]=flags[i];}
            bias_direction=d;bias_thread=GetCurrentThreadId();bias_blocked=0;start=GetTickCount();
            bias_active=1;
            native(graph,dest,route,flags,max,count,mode);
            bias_active=0;
            log_bias(bias_blocked,GetTickCount()-start);
            log_route("biased_candidate",self,p);
            verdict=(!bias_blocked||*count>max)?ALT_INVALID:alternative_verdict(&d,saved,n,route,*count);
            if(verdict==ALT_ACCEPT) {
                rejected[index].valid=0;log_event("forward_alternative_accepted");
            } else {
                for(i=0;i<n;i++){copy_point(&route[i],&saved[i]);flags[i]=saved_flags[i];}*count=n;
                rejected[index].at=d;rejected[index].target=*dest;rejected[index].valid=1;
                log_event(!bias_blocked?"reject_no_matching_reverse_edges":verdict==ALT_REVERSE?"reject_still_backward_or_no_forward_progress":verdict==ALT_DEST?"reject_different_endpoint":verdict==ALT_DETOUR?"reject_excessive_detour":"reject_invalid_or_disconnected_path");
                log_event("forward_alternative_rejected_baseline_restored");
            }
        } else log_event("forward_retry_deferred");
    } else if(pending)log_event("bias_skipped_no_vehicle_motion_or_valid_baseline");
    if(pending)log_route("build_after",self,p);
}

/* Original call is thiscall, ECX=self and no stack arguments. Replacement
   fastcall consumes ECX, ignores EDX and uses the same return convention. */
static void __fastcall update(U8 *self,void *unused) {
    Tracker *s; Point p; int reason,count; U32 now; unsigned index;
    (void)unused;
    if(get_player()) { p.w=0;get_pos(&p);if(valid_point(p))motion_sample(&motion,p,*(U32 *)(game+TIMER));else motion.valid=motion.moving=0; }
    else motion.valid=motion.moving=0;
    native_update(self);
    s=slot(self);
    for(index=0;index<4;index++)if(slots[index].owner==self)break;
    if(index==4)return;
    if(*(int *)(self+STATE)!=3 || !get_player()) {
        tracker_pause(s);
        now=*(U32 *)(game+TIMER);
        if((*(int *)(self+STATE)==1||*(int *)(self+STATE)==2) && get_player() && now-last_sample[index]>=500) {
            p.w=0;get_pos(&p);last_sample[index]=now;log_route("pending",self,p);
        }
        /* A cleared GPS or absent player starts a genuinely new session.
           State 1/2 are pending refreshes and preserve retry history. */
        if(*(int *)(self+STATE)==0 || !get_player()) { s->has_target=0; s->locked=0; rejected[index].valid=0; }
        return;
    }
    count=*(int *)(self+COUNT);
    if(count<2 || count>200) { tracker_pause(s);return; }
    p.w=0;get_pos(&p); now=*(U32 *)(game+TIMER);
    { unsigned was_locked=s->locked;
    reason=tracker_update(s,p,*(Point *)(self+TARGET),(const Point *)self,count,
                          game[0xDA2EA3]!=0,now);
    if(reason && rejected[index].valid && motion.moving && now-motion.last<=350) {
        Direction d;
        if(direction_init(&d,p,motion.dx,motion.dy)&&!retry_allowed(&rejected[index],&d,*(Point *)(self+TARGET))) {
            reason=KEEP;s->locked=0;tracker_pause(s);
        }
    }
    if(reason) {
        log_route(reason==REVERSE?"force_reverse":"force_departure",self,p);
        /* Match vanilla's off-route state transition; no new path job or
           route buffer mutation. Native logic handles it on the next update. */
        *(U32 *)(self+STATE)=1; *(U32 *)(self+STAMP)=now;
        tracker_pause(s);
        if(was_locked)OutputDebugStringA("[FastGPS v0.7] Direction lock overridden; retrying native route.\n");
        OutputDebugStringA(reason==REVERSE?"[FastGPS v0.7] Reverse-progress reroute.\n":
                                          "[FastGPS v0.7] Predicted-departure reroute.\n");
    }
    if(now-last_sample[index]>=500) {last_sample[index]=now;log_route("active",self,p);}
    }
}
/* Observe the actual native readiness result; never bypass it. */
static U8 __fastcall ready(U8 *graph,void *unused,U32 slot_id) {
    U8 result=((U8 (__thiscall *)(U8 *,U32))(game+0x4ECE70))(graph,slot_id);
    U32 now=*(U32 *)(game+TIMER);(void)unused;
    if(slot_id<3 && (result || !have_wait[slot_id] || now-last_wait[slot_id]>=500)) {
        have_wait[slot_id]=!result;last_wait[slot_id]=now;
        log_region(result?"roads_ready":"roads_wait",graph,slot_id);
    }
    return result;
}
static int apply_patch(void *module) {
    static const U8 bounds_x[]={0xf3,0x0f,0x10,0xa6,0x50,0x0d,0,0,0xf3,0x0f,0x10,0x5c,0x24,0x14,0x8d,0xbe,0x50,0x0d,0,0,0x83,0xc4,0x04};
    static const U8 ready_call[]={0xe8,0xd1,0x55,0xfb,0xff,0x84,0xc0,0x74,0x44};
    static const U8 gate[]={0x83,0xBE,0x64,0x0D,0,0,2,0x74,0x1C,0xF6,0x86,0x78,0x0D,0,0,1,0x75,0x13,0x8B,0x86,0x68,0x0D,0,0,0x05,0xB8,0x0B,0,0,0x39,0x05};
    static const U8 tail[]={0x76,0x58,0xFF,0xB6,0x70,0x0D,0,0,0xB9};
    static const U8 caller[]={0x8B,0xCE,0xE8,0x32,0,0,0,0x80,0x7D,0x0C,0,0x74,0x18};
    static const U8 entry[]={0x55,0x8B,0xEC,0x83,0xE4,0xF0,0x83,0xEC,0x44,0x53,0x8B,0xD9,0x56,0x83,0xBB,0x64,0x0D,0,0,3,0x57,0x0F,0x85,0xDE,1,0,0};
    static const U8 cmp[]={0x0F,0x2F,0x0D};
    static const U8 branch[]={0x77,0x13,0x85,0xD2,0x7E,0x24,0x52,0x8B,0xCB,0xE8,0x23,0xEF,0xFF,0xFF};
    static const U8 off[]={0xC7,0x83,0x64,0x0D,0,0,1,0,0,0,0xA1};
    static const U8 stamp[]={0x89,0x83,0x68,0x0D,0,0};
    static const U8 position[]={0xE8,0xBB,0xFF,0xFF,0xFF,0x85,0xC0,0x75,0x19};
    static const U8 build_call[]={0x56,0x57,0xe8,0x33,0x3d,0xfb,0xff,0xff,0xb6,0x70,0x0d,0,0};
    static const U8 vehicle[]={0xe8,0x8b,0xfe,0xff,0xff,0x85,0xc0,0x74,0x10,0xf6,0x80,0x6c,2,0,0,4,0x74,7,0x8b,0x80,0x30,0x0b,0,0,0xc3};
    static const U8 relax_bytes[]={0x0f,0xb7,0x4e,0x10,0x0f,0xbf,0xc1};
    static const U8 extract_bytes[]={0x8b,0x4c,0x24,0x14,0x0f,0xbf,0x49,0x10};
    static const U8 relax_exit[]={0x8b,0x7c,0x24,0x28,0x8b,0x74,0x24,0x18};
    static const U8 extract_exit[]={0x8b,0x4c,0x24,0x14,0x47,0x0f,0xb6,0x41,0x1e};
    static const U8 relax_next[]={0x3b,0xf8,0x7d,0x4d};
    static const U8 extract_next[]={0x2b,0xc8,0x0f,0xbf,0x42,0x10};
    U8 *b=(U8 *)GetModuleHandleA(0),*pe,*g,*d,*h,*o,*q,*r,*e; U32 n,old,old2,old3,ignored,i;
    HANDLE pin;int flushed;
    (void)module;
    if(!b || ((U32)b&0xffff) || r16(b)!=0x5A4D)return 0;
    n=r32(b+0x3C);if(n<0x40||n>0x800)return 0;pe=b+n;
    if(r32(pe)!=0x4550||r16(pe+4)!=0x14C||r32(pe+8)!=0x63D3E735||
       r16(pe+24)!=0x10B||r32(pe+80)!=0x01BE6400)return 0;
    g=b+0x53786A;d=b+DETECT;h=b+HOOK;o=b+0x537B04;q=b+0x5378C8;r=b+0x4E7EE4;e=b+0x4E8078;
    if(!match(b+0x537746,bounds_x,sizeof bounds_x)||!match(b+0x53789A,ready_call,sizeof ready_call)||
       !match(g,gate,sizeof gate)||r32(g+31)!=(U32)b+TIMER||!match(g+35,tail,sizeof tail)||
       r32(g+44)!=(U32)b+0xD77A80||!match(h-2,caller,sizeof caller)||
       !match(b+0x537920,entry,sizeof entry)||!match(d-3,cmp,sizeof cmp)||
       r32(d)!=(U32)b+0xBE8B68||!match(d+4,branch,sizeof branch)||
       !match(o,off,sizeof off)||r32(o+11)!=(U32)b+TIMER||!match(o+15,stamp,sizeof stamp)||
       r32(b+0xBE8B68)!=0x42480000||r32(b+0xBE8B38)!=0x41A00000||
       !match(b+0x53F090,position,sizeof position)||b[0x53F050]!=0xA1||
       r32(b+0x53F051)!=(U32)b+0xC36F14||
       !match(q-2,build_call,sizeof build_call)||!match(b+0x53F1C0,vehicle,sizeof vehicle)||
       !match(r,relax_bytes,sizeof relax_bytes)||!match(e,extract_bytes,sizeof extract_bytes)||
       !match(b+0x4E7F3C,relax_exit,sizeof relax_exit)||!match(b+0x4E8285,extract_exit,sizeof extract_exit)||
       !match(r+sizeof relax_bytes,relax_next,sizeof relax_next)||!match(e+sizeof extract_bytes,extract_next,sizeof extract_next))return 0;
    relax_continue=r+sizeof relax_bytes;relax_skip=b+0x4E7F3C;
    extract_continue=e+sizeof extract_bytes;extract_skip=b+0x4E8285;
    game=b; native_update=(void (__thiscall *)(void *))(b+0x537920);
    get_pos=(void (__cdecl *)(Point *))(b+0x53F090);
    get_player=(void *(__cdecl *)(void))(b+0x53F050);
    /* Pin ASI for the lifetime of the process: the call target must stay mapped. */
    if(!GetModuleHandleExA(5,(const char *)&update,&pin))return 0;
    n=DETECT-DELAY+1;
    if(!VirtualProtect(b+DELAY,n,0x40,&old))return 0;
    if(!VirtualProtect(r,sizeof relax_bytes,0x40,&old2)) {VirtualProtect(b+DELAY,n,old,&ignored);return 0;}
    if(!VirtualProtect(e,sizeof extract_bytes,0x40,&old3)) {
        VirtualProtect(r,sizeof relax_bytes,old2,&ignored);VirtualProtect(b+DELAY,n,old,&ignored);return 0;
    }
    w32(b+0x53789B, (U32)&ready-(U32)(b+0x53789F));
    b[DELAY]=0;*d=0x38;w32(h+1,(U32)&update-(U32)(h+5));w32(q+1,(U32)&rebuild-(U32)(q+5));
    r[0]=0xE9;w32(r+1,(U32)&relax_hook-(U32)(r+5));for(i=5;i<sizeof relax_bytes;i++)r[i]=0x90;
    e[0]=0xE9;w32(e+1,(U32)&extract_hook-(U32)(e+5));for(i=5;i<sizeof extract_bytes;i++)e[i]=0x90;
    flushed=FlushInstructionCache(GetCurrentProcess(),b+DELAY,n)&&
            FlushInstructionCache(GetCurrentProcess(),r,sizeof relax_bytes)&&FlushInstructionCache(GetCurrentProcess(),e,sizeof extract_bytes);
    if(!flushed) {
        w32(b+0x53789B,0xfffb55d1);w32(h+1,0x32);*d=0x68;b[DELAY]=0x0B;w32(q+1,0xfffb3d33);
        for(i=0;i<sizeof relax_bytes;i++)r[i]=relax_bytes[i];for(i=0;i<sizeof extract_bytes;i++)e[i]=extract_bytes[i];
        FlushInstructionCache(GetCurrentProcess(),b+DELAY,n);FlushInstructionCache(GetCurrentProcess(),r,sizeof relax_bytes);FlushInstructionCache(GetCurrentProcess(),e,sizeof extract_bytes);
    }
    if(!VirtualProtect(e,sizeof extract_bytes,old3,&ignored))OutputDebugStringA("[FastGPS v0.7] Could not restore extraction page protection.\n");
    if(!VirtualProtect(r,sizeof relax_bytes,old2,&ignored))OutputDebugStringA("[FastGPS v0.7] Could not restore search page protection.\n");
    if(!VirtualProtect(b+DELAY,n,old,&ignored))OutputDebugStringA("[FastGPS v0.7] Could not restore GPS page protection.\n");
    if(!flushed)return 0;
    return 1;
}
int __stdcall DllMain(HANDLE module,U32 reason,void *reserved) {
    (void)reserved;
    if(reason==1) {
        int ok=apply_patch(module);log_open();
        log_event(ok?"FastGPS v0.7 active; coordinates x10":"FastGPS v0.7 rejected");
        OutputDebugStringA(ok?"[FastGPS v0.7] Active.\n":"[FastGPS v0.7] Not applied.\n");
    }
    return 1;
}
