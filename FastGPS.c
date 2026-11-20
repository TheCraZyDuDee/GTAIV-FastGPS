/* GTA IV Complete Edition 1.2.0.59 x86. Runtime-only GPS changes. */
#include "road_start.h"
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
/* Native graph layout is read-only; region/count checks match the loader. */
static U8 *node_at(U8 *graph,U32 id) {
    U32 region=id&0xffff,index=id>>16,count; U8 *nodes;
    if(region>=64)return 0;
    count=r32(graph+0xB04+4*region);
    nodes=*(U8 **)(graph+0x804+4*region);
    if(!nodes||index>=count||count>65536)return 0;
    nodes+=32*index;
    return r32(nodes+8)==id?nodes:0;
}
static U8 *links_at(U8 *graph,U8 *node,unsigned *count) {
    U32 region=r16(node+8),size; int start=(short)r16(node+0x12); U8 *links;
    if(region>=64||start<0)return 0;
    *count=node[0x1e]&15;
    size=r32(graph+0xD04+4*region);
    links=*(U8 **)(graph+0x904+4*region);
    if(!links||!*count||size>1048576||(U32)start>size||*count>size-(U32)start)return 0;
    return links+8*start;
}
static int reciprocal_lane(U8 *graph,U8 *node,U32 id) {
    unsigned n,i;U8 *links=links_at(graph,node,&n);
    if(!links)return 0;
    for(i=0;i<n;i++)if(r32(links+8*i)==id && (links[8*i+5]&0x38))return 1;
    return 0;
}
static Point node_pos(U8 *n) {
    Point p;
    p.x=(short)r16(n+0x14)*0.125f;p.y=(short)r16(n+0x16)*0.125f;
    p.z=(short)r16(n+0x18)*0.015625f;p.w=0;return p;
}
static U32 forward_node(U8 *graph,U32 original,Point p,float dx,float dy) {
    U8 *a=node_at(graph,original),*links,*b; unsigned n,i;
    U32 best=original; float score=1e10f,second=1e10f;
    Point ap;
    if(!a||a[0x1f]&0xa0)return original;
    links=links_at(graph,a,&n);if(!links)return original;
    ap=node_pos(a);
    for(i=0;i<n;i++) {
        U8 *link=links+8*i;U32 id=r32(link);float v;
        /* Low three bits are forward lanes in native route extraction. */
        if(!(link[5]&7)||!((b=node_at(graph,id))))continue;
        if((b[0x1f]&0xa0)||((a[0x1f]^b[0x1f])&2)||((a[0x1b]^b[0x1b])&31)||
           ((a[0x1c]&0xf0)==0xb0)!=((b[0x1c]&0xf0)==0xb0)||
           (b[0x1c]&0xf0)==0x70||(b[0x1c]&0xf0)==0x80||!reciprocal_lane(graph,b,original))continue;
        v=forward_edge_score(p,ap,node_pos(b),dx,dy);
        if(v<0)continue;
        if(v<score) { second=score;score=v;best=id; }
        else if(v<second)second=v;
    }
    /* Ambiguous fork: keep the native choice. */
    return second-score<2.0f?original:best;
}
typedef U32 *(__thiscall *Nearest)(U8 *,U32 *,const Point *,float,U32,U32,U32,U32,U32,U32,float,float,float,int,U32);
typedef void (__thiscall *BuildRoute)(U8 *,Point *,Point *,U8 *,int,int *,U32);
static RoadMotion motion;
static int choosing,redirected;
static Point actual;
static float travel_x,travel_y;
static U32 * __fastcall start_node(U8 *graph,void *unused,U32 *out,const Point *p,float limit,
        U32 a,U32 b,U32 c,U32 d,U32 e,U32 f,float hx,float hy,float zw,int region,U32 flags) {
    Nearest nearest=(Nearest)(game+0x4E87D0);
    U32 *result=nearest(graph,out,p,limit,a,b,c,d,e,f,hx,hy,zw,region,flags);
    (void)unused;
    /* Only the player-start lookup inside a scoped GPS rebuild is patched. */
    if(choosing && result==out && (out[0]&0xffff)!=0xffff &&
       norm2(p->x-actual.x,p->y-actual.y)<1 && absf(p->z-actual.z)<1) {
        U32 candidate=forward_node(graph,*out,actual,travel_x,travel_y);
        if(candidate!=*out) { *out=candidate;redirected=1;OutputDebugStringA("[FastGPS v0.5] Forward road endpoint selected.\n"); }
    }
    return result;
}
static void __fastcall rebuild(U8 *graph,void *unused,Point *dest,Point *route,U8 *flags,int max,int *count,U32 mode) {
    BuildRoute native=(BuildRoute)(game+0x4EB600);
    U8 *self=(U8 *)route; U32 now=*(U32 *)(game+TIMER);
    int pending=*(int *)(self+STATE)==1;
    (void)unused;
    choosing=redirected=0;
    if(pending)OutputDebugStringA("[FastGPS v0.5] Native reroute started.\n");
    if(pending && motion.moving && now-motion.last<=350 && get_player() &&
       ((void *(__cdecl *)(void))(game+0x53F1C0))()) {
        actual.w=0;get_pos(&actual);
        if(valid_point(actual)) { travel_x=motion.dx;travel_y=motion.dy;choosing=1; }
    }
    native(graph,dest,route,flags,max,count,mode);
    choosing=0;
    if(redirected && (*count<2||*count>max||
       !valid_point(route[0])||!valid_point(route[1])||doubles_back(actual,route[0],route[1],travel_x,travel_y))) {
        OutputDebugStringA("[FastGPS v0.5] Forward route unsuitable; native fallback.\n");
        native(graph,dest,route,flags,max,count,mode);
    }
    if(pending)OutputDebugStringA(*count>0?"[FastGPS v0.5] Native reroute returned a path.\n":"[FastGPS v0.5] Native reroute returned no path.\n");
    redirected=0;
}

/* Original call is thiscall, ECX=self and no stack arguments. Replacement
   fastcall consumes ECX, ignores EDX and uses the same return convention. */
static void __fastcall update(U8 *self,void *unused) {
    Tracker *s; Point p; int reason,count; U32 now;
    (void)unused;
    if(get_player()) { p.w=0;get_pos(&p);if(valid_point(p))motion_sample(&motion,p,*(U32 *)(game+TIMER));else motion.valid=motion.moving=0; }
    else motion.valid=motion.moving=0;
    native_update(self);
    s=slot(self);
    if(*(int *)(self+STATE)!=3 || !get_player()) {
        tracker_pause(s);
        /* A cleared GPS or absent player starts a genuinely new session.
           State 1/2 are pending refreshes and preserve retry history. */
        if(*(int *)(self+STATE)==0 || !get_player()) { s->has_target=0; s->locked=0; }
        return;
    }
    count=*(int *)(self+COUNT);
    if(count<2 || count>200) { tracker_pause(s);return; }
    p.w=0;get_pos(&p); now=*(U32 *)(game+TIMER);
    { unsigned was_locked=s->locked;
    reason=tracker_update(s,p,*(Point *)(self+TARGET),(const Point *)self,count,
                          game[0xDA2EA3]!=0,now);
    if(reason) {
        /* Match vanilla's off-route state transition; no new path job or
           route buffer mutation. Native logic handles it on the next update. */
        *(U32 *)(self+STATE)=1; *(U32 *)(self+STAMP)=now;
        tracker_pause(s);
        if(was_locked)OutputDebugStringA("[FastGPS v0.5] Direction lock overridden; retrying native route.\n");
        OutputDebugStringA(reason==REVERSE?"[FastGPS v0.5] Reverse-progress reroute.\n":
                                          "[FastGPS v0.5] Predicted-departure reroute.\n");
    }
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
    static const U8 build_call[]={0x56,0x57,0xe8,0x33,0x3d,0xfb,0xff,0xff,0xb6,0x70,0x0d,0,0};
    static const U8 start_call[]={0x8d,0x44,0x24,0x60,0x50,0x8d,0x44,0x24,0x78,0x50,0xe8,0x8f,0xd0,0xff,0xff,0x8b,0x4d,0x14};
    static const U8 vehicle[]={0xe8,0x8b,0xfe,0xff,0xff,0x85,0xc0,0x74,0x10,0xf6,0x80,0x6c,2,0,0,4,0x74,7,0x8b,0x80,0x30,0x0b,0,0,0xc3};
    static const U8 nearest_entry[]={0x83,0xec,0x1c,0xf3,0x0f,0x10,0x44,0x24,0x28,0x53,0x55,0x56};
    static const U8 graph_link[]={0x0f,0xb7,0x41,8,0x0f,0xbf,0x49,0x12,0x8b,0x84,0x86,4,9,0,0};
    static const U8 graph_lane[]={0xf6,0x44,0xce,5,7};
    U8 *b=(U8 *)GetModuleHandleA(0),*pe,*g,*d,*h,*o,*q,*k; U32 n,old,old2,ignored; HANDLE pin;
    int flushed;
    (void)module;
    if(!b || ((U32)b&0xffff) || r16(b)!=0x5A4D)return 0;
    n=r32(b+0x3C);if(n<0x40||n>0x800)return 0;pe=b+n;
    if(r32(pe)!=0x4550||r16(pe+4)!=0x14C||r32(pe+8)!=0x63D3E735||
       r16(pe+24)!=0x10B||r32(pe+80)!=0x01BE6400)return 0;
    g=b+0x53786A;d=b+DETECT;h=b+HOOK;o=b+0x537B04;q=b+0x5378C8;k=b+0x4EB73C;
    if(!match(g,gate,sizeof gate)||r32(g+31)!=(U32)b+TIMER||!match(g+35,tail,sizeof tail)||
       r32(g+44)!=(U32)b+0xD77A80||!match(h-2,caller,sizeof caller)||
       !match(b+0x537920,entry,sizeof entry)||!match(d-3,cmp,sizeof cmp)||
       r32(d)!=(U32)b+0xBE8B68||!match(d+4,branch,sizeof branch)||
       !match(o,off,sizeof off)||r32(o+11)!=(U32)b+TIMER||!match(o+15,stamp,sizeof stamp)||
       r32(b+0xBE8B68)!=0x42480000||r32(b+0xBE8B38)!=0x41A00000||
       !match(b+0x53F090,position,sizeof position)||b[0x53F050]!=0xA1||
       r32(b+0x53F051)!=(U32)b+0xC36F14||
       !match(q-2,build_call,sizeof build_call)||!match(k-10,start_call,sizeof start_call)||
       !match(b+0x53F1C0,vehicle,sizeof vehicle)||!match(b+0x4E87D0,nearest_entry,sizeof nearest_entry)||
       !match(b+0x4E8020,graph_link,sizeof graph_link)||!match(b+0x4E805A,graph_lane,sizeof graph_lane))return 0;
    game=b; native_update=(void (__thiscall *)(void *))(b+0x537920);
    get_pos=(void (__cdecl *)(Point *))(b+0x53F090);
    get_player=(void *(__cdecl *)(void))(b+0x53F050);
    /* Pin ASI for the lifetime of the process: the call target must stay mapped. */
    if(!GetModuleHandleExA(5,(const char *)&update,&pin))return 0;
    n=DETECT-DELAY+1;
    if(!VirtualProtect(b+DELAY,n,0x40,&old))return 0;
    if(!VirtualProtect(k,5,0x40,&old2)) { VirtualProtect(b+DELAY,n,old,&ignored);return 0; }
    b[DELAY]=0; *d=0x38; w32(h+1,(U32)&update-(U32)(h+5));
    w32(q+1,(U32)&rebuild-(U32)(q+5));w32(k+1,(U32)&start_node-(U32)(k+5));
    flushed=FlushInstructionCache(GetCurrentProcess(),b+DELAY,n)&&FlushInstructionCache(GetCurrentProcess(),k,5);
    if(!flushed) {
        w32(h+1,0x32);*d=0x68;b[DELAY]=0x0B;w32(q+1,0xfffb3d33);w32(k+1,0xffffd08f);
        FlushInstructionCache(GetCurrentProcess(),b+DELAY,n);FlushInstructionCache(GetCurrentProcess(),k,5);
    }
    if(!VirtualProtect(k,5,old2,&ignored))OutputDebugStringA("[FastGPS v0.5] Could not restore node-call page protection.\n");
    if(!VirtualProtect(b+DELAY,n,old,&ignored))OutputDebugStringA("[FastGPS v0.5] Could not restore GPS page protection.\n");
    if(!flushed)return 0;
    return 1;
}
int __stdcall DllMain(HANDLE module,U32 reason,void *reserved) {
    (void)reserved;
    if(reason==1)OutputDebugStringA(apply_patch(module)?"[FastGPS v0.5] Motion-aware GPS and forward road selection active.\n":
        "[FastGPS v0.5] Not applied: unsupported build, conflicting patch or protection failure.\n");
    return 1;
}
