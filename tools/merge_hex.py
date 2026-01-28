#!/usr/bin/env python3
"""
合并 Bootloader 和 Application HEX 文件

用法:
    python3 merge_hex.py bootloader.hex app.hex -o combined.hex
"""

import sys
import argparse

def parse_hex_line(line):
    """解析 Intel HEX 行"""
    if not line.startswith(':'):
        return None
    
    line = line.strip()
    byte_count = int(line[1:3], 16)
    address = int(line[3:7], 16)
    record_type = int(line[7:9], 16)
    data = line[9:-2]
    checksum = int(line[-2:], 16)
    
    return {
        'byte_count': byte_count,
        'address': address,
        'type': record_type,
        'data': data,
        'checksum': checksum,
        'raw': line
    }

def read_hex_file(filename):
    """读取 HEX 文件"""
    records = []
    with open(filename, 'r') as f:
        for line in f:
            record = parse_hex_line(line)
            if record:
                records.append(record)
    return records

def merge_hex_files(boot_hex, app_hex, output_hex):
    """合并两个 HEX 文件"""
    boot_records = read_hex_file(boot_hex)
    app_records = read_hex_file(app_hex)
    
    with open(output_hex, 'w') as f:
        # 写入 Bootloader 记录 (排除 EOF)
        for record in boot_records:
            if record['type'] != 1:  # 不是 EOF
                f.write(record['raw'] + '\n')
        
        # 写入 Application 记录
        for record in app_records:
            f.write(record['raw'] + '\n')
    
    print(f"合并完成: {output_hex}")
    print(f"  Bootloader: {len([r for r in boot_records if r['type'] == 0])} 数据记录")
    print(f"  Application: {len([r for r in app_records if r['type'] == 0])} 数据记录")

def main():
    parser = argparse.ArgumentParser(description='合并 HEX 文件')
    parser.add_argument('bootloader', help='Bootloader HEX 文件')
    parser.add_argument('application', help='Application HEX 文件')
    parser.add_argument('-o', '--output', default='combined.hex', help='输出文件')
    
    args = parser.parse_args()
    
    merge_hex_files(args.bootloader, args.application, args.output)

if __name__ == '__main__':
    main()
