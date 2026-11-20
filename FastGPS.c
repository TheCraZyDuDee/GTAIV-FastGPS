/* GTAIV-FastGPS: verified against GTA IV 1.2.0.59, x86.
 * No hooks, threads, dependencies or writes to GTAIV.exe on disk. */
#if defined(_MSC_VER)
#define STDCALL __stdcall
#define IMPORT __declspec(dllimport)
#else
#error Build this source using an x86 Windows MSVC-compatible compiler.
#endif
#if !defined(_M_IX86)
#error GTA IV requires a 32-bit ASI.
#endif

typedef unsigned char U8;
typedef unsigned short U16;
typedef unsigned long U32;
typedef void *HANDLE;
IMPORT HANDLE STDCALL GetModuleHandleA(const char *name);
IMPORT int STDCALL VirtualProtect(void *address, U32 length, U32 protection, U32 *old);
IMPORT int STDCALL FlushInstructionCache(HANDLE process, const void *address, U32 length);
IMPORT HANDLE STDCALL GetCurrentProcess(void);
IMPORT void STDCALL OutputDebugStringA(const char *message);

#define PATCH_RVA 0x537884UL
#define HEADER_TIMESTAMP 0x63D3E735UL
#define IMAGE_SIZE 0x01BE6400UL
#define PAGE_EXECUTE_READWRITE 0x40UL

static U16 read16(const U8 *p) {
    return (U16)(p[0] | ((U16)p[1] << 8));
}
static U32 read32(const U8 *p) {
    return (U32)p[0] | ((U32)p[1]<<8) | ((U32)p[2]<<16) | ((U32)p[3]<<24);
}
static int match(const U8 *p, const U8 *expected, U32 length) {
    U32 i;
    for (i=0;i<length;++i) if(p[i]!=expected[i]) return 0;
    return 1;
}

static int apply_patch(void) {
    /* Compare the complete time-gate prefix and branch, checking relocated
       absolute timer/state references separately. */
    static const U8 gate[] = {
        0x83,0xBE,0x64,0x0D,0x00,0x00,0x02,0x74,0x1C,
        0xF6,0x86,0x78,0x0D,0x00,0x00,0x01,0x75,0x13,
        0x8B,0x86,0x68,0x0D,0x00,0x00,0x05,0xB8,0x0B,0x00,0x00,0x39,0x05
    };
    static const U8 offroute[] = {0xC7,0x83,0x64,0x0D,0,0,1,0,0,0,0xA1};
    static const U8 stamp[] = {0x89,0x83,0x68,0x0D,0,0};
    static const U8 tail[] = {0x76,0x58,0xFF,0xB6,0x70,0x0D,0,0,0xB9};
    U8 *base = (U8 *)GetModuleHandleA(0);
    U8 *pe, *p, *target, *off;
    U32 offset, timer, old, ignored;
    if (!base || read16(base)!=0x5A4D) return 0;
    offset=read32(base+0x3C);
    if (offset<0x40 || offset>0x800) return 0;
    pe=base+offset;
    if (read32(pe)!=0x4550 || read16(pe+4)!=0x14C ||
        read32(pe+8)!=HEADER_TIMESTAMP || read16(pe+24)!=0x10B ||
        read32(pe+24+56)!=IMAGE_SIZE) return 0;
    p=base+0x53786A;
    target=base+PATCH_RVA;
    off=base+0x537B04;
    timer=(U32)base+0xD735B4;
    if (!match(p,gate,sizeof(gate)) || read32(p+31)!=timer ||
        !match(p+35,tail,sizeof(tail)) ||
        read32(p+44)!=(U32)base+0xD77A80 ||
        !match(off,offroute,sizeof(offroute)) || read32(off+11)!=timer ||
        !match(off+15,stamp,sizeof(stamp))) return 0;
    /* 3000 (0x0BB8) -> 184 (0x00B8). A single byte write avoids a torn
       multi-byte immediate. Never bypass the vanilla readiness check. */
    if (!VirtualProtect(target,1,PAGE_EXECUTE_READWRITE,&old)) return 0;
    *(volatile U8 *)target=0;
    if (!FlushInstructionCache(GetCurrentProcess(),target,1)) {
        *(volatile U8 *)target=0x0B;
        FlushInstructionCache(GetCurrentProcess(),target,1);
        VirtualProtect(target,1,old,&ignored);
        return 0;
    }
    if (!VirtualProtect(target,1,old,&ignored)) {
        *(volatile U8 *)target=0x0B;
        FlushInstructionCache(GetCurrentProcess(),target,1);
        VirtualProtect(target,1,old,&ignored);
        return 0;
    }
    return 1;
}

int STDCALL DllMain(HANDLE module, U32 reason, void *reserved) {
    (void)module; (void)reserved;
    if (reason==1) {
        if (apply_patch()) OutputDebugStringA("[FastGPS] Applied: 3000 -> 184 ms.\n");
        else OutputDebugStringA("[FastGPS] Not applied: unsupported executable, conflicting patch, or protection failure.\n");
    }
    return 1;
}
