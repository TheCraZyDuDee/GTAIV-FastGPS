"""Offline verification: python verify_exe.py path/to/GTAIV.exe. Read-only."""
import hashlib
import struct
import sys
from pathlib import Path


def verify(path):
    data = Path(path).read_bytes()
    u16 = lambda p: struct.unpack_from('<H', data, p)[0]
    u32 = lambda p: struct.unpack_from('<I', data, p)[0]
    pe = u32(0x3c)
    if not (data[:2] == b'MZ' and data[pe:pe+4] == b'PE\0\0'
            and u16(pe+4) == 0x14c and u32(pe+8) == 0x63d3e735
            and u16(pe+24) == 0x10b and u32(pe+80) == 0x1be6400):
        raise ValueError('Unsupported PE header')
    base = u32(pe+52)
    table = pe+24+u16(pe+20)
    sections = [struct.unpack_from('<IIII', data, table+40*i+8)
                for i in range(u16(pe+6))]

    def read(rva, n):
        for _, start, size, raw in sections:
            if start <= rva and rva+n <= start+size:
                return data[raw+rva-start:raw+rva-start+n]
        raise ValueError(f'Unmapped RVA: {rva:#x}')

    gate = bytes.fromhex('83 be 64 0d 00 00 02 74 1c f6 86 78 0d 00 00 01 75 13 8b 86 68 0d 00 00 05 b8 0b 00 00 39 05')
    timer = struct.pack('<I', base+0xd735b4)
    tail = bytes.fromhex('76 58 ff b6 70 0d 00 00 b9')
    state = bytes.fromhex('c7 83 64 0d 00 00 01 00 00 00 a1')
    stamp = bytes.fromhex('89 83 68 0d 00 00')
    if read(0x53786a, 48) != gate+timer+tail+struct.pack('<I',base+0xd77a80):
        raise ValueError('GPS delay signature mismatch')
    if read(0x537b04, 21) != state+timer+stamp:
        raise ValueError('Route deviation signature mismatch')
    detect = read(0x537ae8, 21)
    expected = bytes.fromhex('0f 2f 0d') + struct.pack('<I', base+0xbe8b68) + bytes.fromhex('77 13 85 d2 7e 24 52 8b cb e8 23 ef ff ff')
    if detect != expected or base & 0xffff:
        raise ValueError('GPS distance comparison signature mismatch')
    if (struct.unpack('<f', read(0xbe8b68, 4))[0],
            struct.unpack('<f', read(0xbe8b38, 4))[0]) != (50.0, 20.0):
        raise ValueError('Distance constant mismatch')
    for relocated_base in (base, 0x12340000, 0x70000000):
        operand = bytearray(struct.pack('<I', relocated_base+0xbe8b68))
        operand[0] = 0x38
        if struct.unpack('<I', operand)[0] != relocated_base+0xbe8b38:
            raise ValueError('Distance operand relocation mismatch')
    insn = bytearray(read(0x537882, 5))
    before = struct.unpack_from('<I', insn, 1)[0]
    insn[2] = 0
    after = struct.unpack_from('<I', insn, 1)[0]
    if (before, after) != (3000, 184):
        raise ValueError('Invalid delay patch')
    print('SHA-256:', hashlib.sha256(data).hexdigest())
    print('Verified GPS gate and deviation signatures')
    print(f'Patch RVA 0x537884: 0x0B -> 0x00 ({before} -> {after} ms)')
    print('Patch RVA 0x537AEB: 0x68 -> 0x38 (50 -> 20 units; GPS-only reference)')
    print('Verified low-byte address change at three aligned image bases')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('Usage: python verify_exe.py GTAIV.exe')
    try:
        verify(sys.argv[1])
    except (ValueError, OSError, struct.error) as exc:
        sys.exit(f'Verification failed: {exc}')
