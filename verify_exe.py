"""Read-only executable verification against FastGPS.c signatures."""
import hashlib
import re
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
    if base & 0xffff:
        raise ValueError('Unaligned image base')
    table = pe+24+u16(pe+20)
    sections = [struct.unpack_from('<IIII', data, table+40*i+8)
                for i in range(u16(pe+6))]

    def read(rva, n):
        for _, start, size, raw in sections:
            if start <= rva and rva+n <= start+size:
                return data[raw+rva-start:raw+rva-start+n]
        raise ValueError(f'Unmapped RVA: {rva:#x}')

    source = Path(__file__).with_name('FastGPS.c').read_text()
    sites = {'gate': 0x53786a, 'tail': 0x53788d, 'caller': 0x5378e7,
             'entry': 0x537920, 'cmp': 0x537ae8, 'branch': 0x537aef,
             'off': 0x537b04, 'stamp': 0x537b13, 'position': 0x53f090}
    for name, rva in sites.items():
        pattern = re.search(r'\b'+name+r'\[\]=\{([^}]+)\}', source)
        expected = bytes(int(part, 0) for part in pattern.group(1).split(','))
        if read(rva, len(expected)) != expected:
            raise ValueError(f'{name} signature mismatch at {rva:#x}')
    operands = {0x537889: base+0xd735b4, 0x537896: base+0xd77a80,
                0x537aeb: base+0xbe8b68, 0x537b0f: base+0xd735b4,
                0xbe8b68: 0x42480000, 0xbe8b38: 0x41a00000,
                0x53f051: base+0xc36f14}
    for rva, expected in operands.items():
        if struct.unpack('<I', read(rva, 4))[0] != expected:
            raise ValueError(f'Operand mismatch at {rva:#x}')
    if read(0x53f050, 1) != b'\xa1':
        raise ValueError('Player accessor mismatch')
    if struct.unpack('<I', read(0x537883, 4))[0] != 3000:
        raise ValueError('Unexpected timer')
    for relocated_base in (base, 0x12340000, 0x70000000):
        operand = bytearray(struct.pack('<I', relocated_base+0xbe8b68))
        operand[0] = 0x38
        if struct.unpack('<I', operand)[0] != relocated_base+0xbe8b38:
            raise ValueError('Distance relocation mismatch')
        call_site = relocated_base+0x5378e9
        target = 0x10002000
        rel = (target-call_site-5) & 0xffffffff
        if (call_site+5+rel) & 0xffffffff != target:
            raise ValueError('Hook relocation mismatch')
    print('SHA-256:', hashlib.sha256(data).hexdigest())
    print('PASS PE header, nine code signatures, timer and distance operands, hook relocation')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('Usage: python verify_exe.py GTAIV.exe')
    try:
        verify(sys.argv[1])
    except (ValueError, OSError, struct.error) as exc:
        sys.exit(f'Verification failed: {exc}')
