import os
import struct
from itertools import chain

SECTOR_SIZE = 2352
DATA_OFFSET = 24
DATA_SIZE = 2048

DIRS_NTSC = [
    "1ST", "ANIM", "BG", "CHARA", "ITEM", "MISC", "SND", "TEST", 
    "TIM", "VIN", "XA"
]

FILE_TYPES = [
    "TIM", "VAB", "BIN", "DMS", "ANM", "PLM", "IPD", "ILM", 
    "TMD", "DAT", "KDT", "CMP", "TXT", "UU1", "UU2", ""
]

TOC_OFFSET = 0xB91C
FILE_COUNT = 2074

def extract_assets(bin_path, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    
    with open(bin_path, 'rb') as f:
        # Step 1: Find SLUS_007.07
        f.seek(16 * SECTOR_SIZE + DATA_OFFSET)
        pvd = f.read(DATA_SIZE)
        
        root_dr = pvd[156:156+34]
        root_extent = struct.unpack('<I', root_dr[2:6])[0]
        root_size = struct.unpack('<I', root_dr[10:14])[0]
        
        f.seek(root_extent * SECTOR_SIZE + DATA_OFFSET)
        root_data = f.read(root_size)
        
        idx = 0
        slus_extent = None
        slus_size = None
        while idx < root_size:
            dr_len = root_data[idx]
            if dr_len == 0:
                idx += 1
                continue
            extent = struct.unpack('<I', root_data[idx+2:idx+6])[0]
            size = struct.unpack('<I', root_data[idx+10:idx+14])[0]
            name_len = root_data[idx+32]
            name = root_data[idx+33:idx+33+name_len].decode('ascii', errors='ignore').split(';')[0]
            if name == 'SLUS_007.07':
                slus_extent = extent
                slus_size = size
                break
            idx += dr_len
            
        print(f"Found SLUS_007.07 at sector {slus_extent}, size {slus_size}")
        
        # Step 2: Read SLUS_007.07 into memory
        slus_data = bytearray()
        f.seek(slus_extent * SECTOR_SIZE)
        sectors_to_read = (slus_size + 2047) // 2048
        for _ in range(sectors_to_read):
            sector = f.read(SECTOR_SIZE)
            slus_data.extend(sector[DATA_OFFSET : DATA_OFFSET + 2048])
            
        slus_data = slus_data[:slus_size]
        
        # Parse file table
        entries = []
        for i in range(FILE_COUNT):
            entry_data = slus_data[TOC_OFFSET + i * 12 : TOC_OFFSET + (i + 1) * 12]
            meta, file1, file2 = struct.unpack('<3I', entry_data)
            
            name = "".join(chain(
                (chr(32 + ((file1 >> shift) & 63)) for shift in range(4, 28, 6)),
                (chr(32 + ((file2 >> shift) & 63)) for shift in range(0, 24, 6))
            )).strip()
            
            # Remove invalid chars from name if any
            name = "".join(c for c in name if c.isalnum() or c in '_-')
            
            size_blocks = meta >> 19
            lba = meta & 0x7FFFF
            
            path_idx = file1 & 15
            type_idx = (file2 >> 24) & 15
            
            dir_name = DIRS_NTSC[path_idx] if path_idx < len(DIRS_NTSC) else "UNKNOWN"
            ext_name = FILE_TYPES[type_idx] if type_idx < len(FILE_TYPES) else ""
                
            entries.append({
                'lba': lba,
                'size_blocks': size_blocks,
                'name': name,
                'dir': dir_name,
                'ext': ext_name
            })
            
        print(f"Parsed {len(entries)} file entries. Beginning extraction...")
        
        # Step 3: Extract
        for i in range(len(entries)):
            entry = entries[i]
            
            if entry['dir'] == 'XA':
                continue
                
            next_lba = None
            for j in range(i + 1, len(entries)):
                if entries[j]['dir'] != 'XA' and entries[j]['lba'] > entry['lba']:
                    next_lba = entries[j]['lba']
                    break
                    
            if next_lba:
                sectors_to_read = next_lba - entry['lba']
            else:
                sectors_to_read = (entry['size_blocks'] * 256 + 2047) // 2048
                
            full_dir = os.path.join(out_dir, entry['dir'])
            os.makedirs(full_dir, exist_ok=True)
            
            ext_suffix = f".{entry['ext']}" if entry['ext'] else ""
            filename = f"{entry['name']}{ext_suffix}"
            filepath = os.path.join(full_dir, filename)
            
            with open(filepath, 'wb') as out_f:
                f.seek(entry['lba'] * SECTOR_SIZE)
                for s in range(sectors_to_read):
                    sector_data = f.read(SECTOR_SIZE)
                    payload = sector_data[DATA_OFFSET : DATA_OFFSET + DATA_SIZE]
                    out_f.write(payload)
                    
            if i % 100 == 0:
                print(f"Extracted {i}/{len(entries)} files...")
                
        print("Done!")

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description="Extract assets from SH1 BIN dump.")
    parser.add_argument("bin_path", help="Path to the .BIN file")
    parser.add_argument("out_dir", help="Output directory for extracted files")
    args = parser.parse_args()
    
    extract_assets(args.bin_path, args.out_dir)