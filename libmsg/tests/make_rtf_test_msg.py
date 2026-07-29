#!/usr/bin/env python3
"""
Builds tests/sample_rtf.msg: a minimal CFB fixture with ONLY
PR_RTF_COMPRESSED set (no PR_BODY, no PR_HTML), to exercise msg_reader.c's
RTF fallback path - decompression, plus recovering both plain text and,
since this RTF is "\\fromhtml1"-encapsulated, the original HTML - through
the real CFB/property-parsing pipeline, not just msg_rtf.c in isolation.

Reuses the CFB-building helpers from make_test_msg.py; see that module's
docstring for the general sector layout this mirrors.
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from make_test_msg import (
    SECTOR_SIZE, ENDOFCHAIN, FREESECT, FATSECT,
    MiniAllocator, RegularAllocator, DirEntry,
)

RTF_TEXT = (
    b"{\\rtf1\\ansi\\fromhtml1\\deff0"
    b"{\\*\\htmltag64 <html><body>}"
    b"Hello RTF HTML"
    b"{\\*\\htmltag92 </body></html>}"
    b"}"
)


def wrap_uncompressed_rtf(rtf: bytes) -> bytes:
    """PR_RTF_COMPRESSED header (MS-OXRTFCP) around raw, uncompressed RTF."""
    compressed_size = len(rtf) + 12
    uncompressed_size = len(rtf)
    magic = 0x414C454D  # "MELA": uncompressed
    crc = 0
    return struct.pack("<IIII", compressed_size, uncompressed_size, magic, crc) + rtf


def build(out_path: str):
    mini = MiniAllocator()

    top_props = b"\x00" * 32
    s_top_props = mini.add(top_props)
    s_rtf = mini.add(wrap_uncompressed_rtf(RTF_TEXT))

    NUM_ENTRIES = 3
    dir_sectors_needed = -(-NUM_ENTRIES // 4)
    first_dir_sector = 1
    minifat_sector = first_dir_sector + dir_sectors_needed
    first_free_regular = minifat_sector + 1

    regular = RegularAllocator(first_free_regular)
    s_root = regular.add(bytes(mini.blob))

    entries = [None] * NUM_ENTRIES
    entries[0] = DirEntry("Root Entry", 5, child=1, start_sector=s_root[0], size=s_root[1])
    entries[1] = DirEntry("__properties_version1.0", 2, right=2,
                           start_sector=s_top_props[0], size=s_top_props[1])
    entries[2] = DirEntry("__substg1.0_10090102", 2,
                           start_sector=s_rtf[0], size=s_rtf[1])

    dir_bytes = bytearray()
    for e in entries:
        dir_bytes += e.pack()
    while len(dir_bytes) % SECTOR_SIZE != 0:
        dir_bytes += b"\x00"

    total_sectors = regular.next_index

    fat = [FREESECT] * total_sectors
    fat[0] = FATSECT
    for i in range(dir_sectors_needed):
        sector = first_dir_sector + i
        fat[sector] = sector + 1 if i < dir_sectors_needed - 1 else ENDOFCHAIN
    fat[minifat_sector] = ENDOFCHAIN
    for idx, nxt in regular.fat_entries.items():
        fat[idx] = nxt

    fat_sector_bytes = b"".join(struct.pack("<I", v) for v in fat)
    fat_sector_bytes += b"\xff" * (SECTOR_SIZE - len(fat_sector_bytes))

    minifat_entries = list(mini.minifat)
    minifat_sector_bytes = b"".join(struct.pack("<I", v) for v in minifat_entries)
    minifat_sector_bytes += b"\xff" * (SECTOR_SIZE - len(minifat_sector_bytes))

    header = bytearray(512)
    header[0:8] = bytes.fromhex("D0CF11E0A1B11AE1")
    struct.pack_into("<H", header, 24, 0x003E)  # minor version
    struct.pack_into("<H", header, 26, 3)       # major version (v3)
    struct.pack_into("<H", header, 28, 0xFFFE)  # byte order
    struct.pack_into("<H", header, 30, 9)       # sector shift -> 512
    struct.pack_into("<H", header, 32, 6)       # mini sector shift -> 64
    struct.pack_into("<I", header, 40, 0)       # num directory sectors (v4 only)
    struct.pack_into("<I", header, 44, 1)       # num FAT sectors
    struct.pack_into("<I", header, 48, first_dir_sector)
    struct.pack_into("<I", header, 52, 0)       # transaction signature
    struct.pack_into("<I", header, 56, 4096)    # mini stream cutoff
    struct.pack_into("<I", header, 60, minifat_sector)
    struct.pack_into("<I", header, 64, 1)       # num mini FAT sectors
    struct.pack_into("<I", header, 68, ENDOFCHAIN)  # first DIFAT sector
    struct.pack_into("<I", header, 72, 0)       # num DIFAT sectors
    difat = [0] + [FREESECT] * 108
    for i, v in enumerate(difat):
        struct.pack_into("<I", header, 76 + i * 4, v)

    out = bytearray()
    out += header
    out += fat_sector_bytes
    out += dir_bytes
    out += minifat_sector_bytes
    for sect in regular.sectors:
        out += sect

    Path(out_path).write_bytes(bytes(out))
    print(f"wrote {out_path} ({len(out)} bytes)")


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else "sample_rtf.msg")
