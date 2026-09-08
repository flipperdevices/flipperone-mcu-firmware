#!/usr/bin/env python3
import json
import re
import sys

def parse_size(size_str):
    size_str = str(size_str).strip().upper()
    
    if size_str.startswith('0X'):
        return int(size_str, 16)
    
    match = re.match(r'^(\d+(?:\.\d+)?)([KMG]?)B?$', size_str)
    if not match:
        raise ValueError()
    
    number = float(match.group(1))
    unit = match.group(2)
    
    multiplier = {
        '': 1,
        'K': 1024,
        'M': 1024 * 1024,
        'G': 1024 * 1024 * 1024
    }
    
    return int(number * multiplier[unit])

def main():
    try:
        with open(sys.argv[1], 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        fw_a_size_str = None
        for partition in data.get('partitions', []):
            name = str(partition.get('name', '')).strip()
            if name == 'FW A':
                fw_a_size_str = partition.get('size')
                break
        
        if fw_a_size_str is None:
            sys.exit(1)
        
        size_bytes = parse_size(fw_a_size_str)
        print(size_bytes)
        
    except FileNotFoundError:
        sys.exit(1)
    except Exception as e:
        sys.exit(1)

if __name__ == '__main__':
    main()