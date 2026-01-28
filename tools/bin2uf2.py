#!/usr/bin/env python3
"""
BIN to UF2 Converter for CH592/CH591
Usage: bin2uf2.py input.bin output.uf2 [family_id] [base_addr]
"""
import sys
import struct

UF2_MAGIC_START0 = 0x0A324655
UF2_MAGIC_START1 = 0x9E5D5157
UF2_MAGIC_END = 0x0AB16F30
UF2_FLAG_FAMILY = 0x00002000

def convert(infile, outfile, family=0x4b50e11a, base=0x00001000):
    with open(infile, 'rb') as f:
        data = f.read()
    
    blocks = []
    num_blocks = (len(data) + 255) // 256
    
    for i in range(num_blocks):
        ptr = i * 256
        chunk = data[ptr:ptr+256].ljust(256, b'\x00')
        block = struct.pack('<IIIIIIII',
            UF2_MAGIC_START0, UF2_MAGIC_START1, UF2_FLAG_FAMILY,
            base + ptr, 256, i, num_blocks, family)
        block += chunk + struct.pack('<I', UF2_MAGIC_END)
        blocks.append(block.ljust(512, b'\x00'))
    
    with open(outfile, 'wb') as f:
        f.write(b''.join(blocks))
    print(f"Created {outfile}: {num_blocks} blocks")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: bin2uf2.py input.bin output.uf2 [family_id] [base_addr]")
        sys.exit(1)
    family = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x4b50e11a
    base = int(sys.argv[4], 0) if len(sys.argv) > 4 else 0x00001000
    convert(sys.argv[1], sys.argv[2], family, base)
