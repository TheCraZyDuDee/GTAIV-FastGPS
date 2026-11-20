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

#define DELAY_RVA 0x537884UL
#define DETECT_RVA 0x537AEBUL
#define ORIGINAL_DISTANCE_RVA 0xBE8B68UL
#define NEW_DISTANCE_RVA 0xBE8B38UL
#define PATCH_LENGTH (DETECT_RVA - DELAY_RVA + 1)
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
    static const U8 detect_prefix[] = {0x0F,0x2F,0x0D};
    static const U8 detect_tail[] = {0x77,0x13,0x85,0xD2,0x7E,0x24,0x52,0x8B,0xCB,0xE8,0x23,0xEF,0xFF,0xFF};
    U8 *base = (U8 *)GetModuleHandleA(0);
    U8 *pe, *p, *target, *off, *detect;
    U32 offset, timer, old, ignored;
    if (!base || ((U32)base & 0xFFFFUL) || read16(base)!=0x5A4D) return 0;
    offset=read32(base+0x3C);
    if (offset<0x40 || offset>0x800) return 0;
    pe=base+offset;
    if (read32(pe)!=0x4550 || read16(pe+4)!=0x14C ||
        read32(pe+8)!=HEADER_TIMESTAMP || read16(pe+24)!=0x10B ||
        read32(pe+24+56)!=IMAGE_SIZE) return 0;
    p=base+0x53786A;
    target=base+DELAY_RVA;
    detect=base+DETECT_RVA;
    off=base+0x537B04;
    timer=(U32)base+0xD735B4;
    if (!match(p,gate,sizeof(gate)) || read32(p+31)!=timer ||
        !match(p+35,tail,sizeof(tail)) ||
        read32(p+44)!=(U32)base+0xD77A80 ||
        !match(off,offroute,sizeof(offroute)) || read32(off+11)!=timer ||
        !match(off+15,stamp,sizeof(stamp)) ||
        !match(detect-3,detect_prefix,sizeof(detect_prefix)) ||
        read32(detect)!=(U32)base+ORIGINAL_DISTANCE_RVA ||
        !match(detect+4,detect_tail,sizeof(detect_tail)) ||
        read32(base+ORIGINAL_DISTANCE_RVA)!=0x42480000UL ||
        read32(base+NEW_DISTANCE_RVA)!=0x41A00000UL) return 0;
    /* Both edits lie in the same 4 KiB code page. Each is one byte:
       3000 -> 184 ms, and redirect the GPS-only 50.0f operand to the
       existing 20.0f constant. Do not modify the shared float constants.
       The PE image is 64 KiB aligned, so the low-byte edit survives ASLR. */
    if (!VirtualProtect(target,PATCH_LENGTH,PAGE_EXECUTE_READWRITE,&old)) return 0;
    *(volatile U8 *)target=0;
    *(volatile U8 *)detect=0x38;
    if (!FlushInstructionCache(GetCurrentProcess(),target,PATCH_LENGTH) ||
        !VirtualProtect(target,PATCH_LENGTH,old,&ignored)) {
        *(volatile U8 *)detect=0x68;
        *(volatile U8 *)target=0x0B;
        FlushInstructionCache(GetCurrentProcess(),target,PATCH_LENGTH);
        VirtualProtect(target,PATCH_LENGTH,old,&ignored);
        return 0;
    }
    return 1;
}

int STDCALL DllMain(HANDLE module, U32 reason, void *reserved) {
    (void)module; (void)reserved;
    if (reason==1) {
        if (apply_patch()) OutputDebugStringA("[FastGPS v0.2] Applied: 3000 -> 184 ms; off-route 50 -> 20 units.\n");
        else OutputDebugStringA("[FastGPS v0.2] Not applied: unsupported executable, conflicting patch, or protection failure.\n");
    }
    return 1;
}
