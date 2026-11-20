"""Verify the PE32 imports and exact clang-cl x86 detour encodings/relocations.
Usage: python verify_build.py FastGPS.asi FastGPS.obj
This is structural verification, not execution or native integration testing.
"""
import struct
import sys
from pathlib import Path


def u16(data, p):
    return struct.unpack_from('<H', data, p)[0]


def u32(data, p):
    return struct.unpack_from('<I', data, p)[0]


def verify_pe(path):
    data = Path(path).read_bytes()
    pe = u32(data, 0x3c)
    assert data[:2] == b'MZ' and data[pe:pe+4] == b'PE\0\0'
    assert u16(data, pe+4) == 0x14c and u16(data, pe+24) == 0x10b
    assert u16(data, pe+22) & 0x2000  # DLL
    opt = pe+24
    table = opt+u16(data, pe+20)
    sections = [struct.unpack_from('<IIII', data, table+40*i+8) for i in range(u16(data, pe+6))]

    def offset(rva):
        for _, start, size, raw in sections:
            if start <= rva < start+size:
                return raw+rva-start
        raise ValueError(f'Unmapped RVA {rva:x}')

    def text(rva):
        return data[offset(rva):].split(b'\0', 1)[0].decode('ascii')

    i = offset(u32(data, opt+104))
    libs = {}
    while any(data[i:i+20]):
        original, _, _, name, first = struct.unpack_from('<IIIII', data, i)
        thunk = offset(original or first)
        names = []
        while u32(data, thunk):
            names.append(text(u32(data, thunk)+2))
            thunk += 4
        libs[text(name).lower()] = set(names)
        i += 20
    assert libs == {'kernel32.dll': {'CreateFileW', 'FlushInstructionCache', 'GetCurrentProcess',
        'GetCurrentThreadId', 'GetModuleHandleA', 'GetModuleHandleExA', 'GetModuleFileNameW', 'GetPrivateProfileIntW',
        'GetTickCount', 'OutputDebugStringA', 'VirtualProtect', 'WriteFile'}}, libs
    print('PASS PE32 DLL and 12 undecorated kernel32 imports; no CRT')


def verify_hooks(path):
    data = Path(path).read_bytes()
    machine, sections, _, symtab, count, opt, _ = struct.unpack_from('<HHIIIHH', data)
    assert machine == 0x14c and opt == 0
    strings = symtab+18*count
    symbols = {}
    i = 0
    while i < count:
        off = symtab+i*18
        name = data[off:off+8]
        if name[:4] == b'\0'*4:
            name = data[strings+u32(name, 4):].split(b'\0', 1)[0]
        else:
            name = name.rstrip(b'\0')
        value, section, _, _, aux = struct.unpack_from('<IhHBB', data, off+8)
        symbols[i] = (name.decode('ascii'), value, section)
        i += 1+aux
    common = ('9c833d00000000000f84920000006081ec90000000'
        'f30f7f0424f30f7f4c2410f30f7f542420f30f7f5c2430f30f7f642440f30f7f6c2450'
        'f30f7f742460f30f7f7c24700fae9c2480000000')
    restore = ('e80000000083c40885c0f30f6f0424f30f6f4c2410f30f6f542420f30f6f5c2430'
        'f30f6f642440f30f6f6c2450f30f6f742460f30f6f7c24700fae9424800000008da4249000000061')
    cases = {
        'relax': ('8b8424dc0000005056', '750e9d0fb74e100fbfc1ff25000000009dff2500000000', 170, 177),
        'extract': ('8b8424c80000005250', '750f9d8b4c24140fbf4910ff25000000009dff2500000000', 171, 178),
    }
    for kind, (args, tail, cont, skip) in cases.items():
        _, value, section = next(s for s in symbols.values() if s[0] == f'_{kind}_hook')
        assert 1 <= section <= sections
        off = 20+40*(section-1)
        size, raw, reloc = struct.unpack_from('<III', data, off+16)
        expected = bytes.fromhex(common+args+restore+tail)
        assert data[raw+value:raw+value+len(expected)] == expected, f'{kind} hook encoding differs'
        relocations = []
        for i in range(u16(data, off+32)):
            address, symbol, typ = struct.unpack_from('<IIH', data, reloc+i*10)
            if value <= address < value+len(expected):
                relocations.append((address-value, symbols[symbol][0], typ))
        assert relocations == [(3, '_bias_active', 6), (86, '_blocked_edge', 20),
            (cont, f'_{kind}_continue', 6), (skip, f'_{kind}_skip', 6)], relocations
        print(f'PASS {kind} hook: stack offsets, argument order, branch targets, register/MXCSR restoration and relocations')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit('Usage: python verify_build.py FastGPS.asi FastGPS.obj')
    verify_pe(sys.argv[1])
    verify_hooks(sys.argv[2])
