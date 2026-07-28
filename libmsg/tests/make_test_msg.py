#!/usr/bin/env python3
"""
Hand-rolled Compound File Binary (MS-CFB) writer used ONLY to build a small,
deterministic .msg fixture for testing libmsg. This is not a general CFB
writer: it hardcodes the exact directory layout for one test message so the
expected values in tests/test_basic.c can be asserted against byte-for-byte.

Layout (see MS-CFB / MS-OXMSG for the on-disk formats being reproduced):
  sector 0        FAT sector
  sectors 1-4     directory sectors (15 entries, 4 per 512-byte sector)
  sector 5        mini-FAT sector
  sectors 6..     mini-stream container, then the one "large" stream
                  (the attachment payload, forced over the 4096-byte
                  mini-stream cutoff so the normal-FAT read path is
                  exercised too).
"""
import struct
import sys
from pathlib import Path

SECTOR_SIZE = 512
MINI_SECTOR_SIZE = 64
MINI_STREAM_CUTOFF = 4096
ENDOFCHAIN = 0xFFFFFFFE
FREESECT = 0xFFFFFFFF
FATSECT = 0xFFFFFFFD
NOSTREAM = 0xFFFFFFFF

SUBJECT = "Café ☕ Test Subject"
BODY = "Hello,\r\nThis is a test message body.\r\n\r\nRegards."
HTML = b"<html><body><p>Hello HTML</p></body></html>"
SENDER_NAME = "Alice Example"
SENDER_EMAIL = "alice@example.com"
RECIPIENT_NAME = "Bob Recipient"
RECIPIENT_EMAIL = "bob@example.com"
ATTACHMENT_FILENAME = "attachment.bin"
ATTACHMENT_MIMETYPE = "application/octet-stream"
ATTACHMENT_CONTENT_ID = "test-image@cid"
ATTACHMENT_DATA = bytes((i % 256) for i in range(5000))
SUBMIT_TIME_UNIX = 1710505800  # 2024-03-15T12:30:00Z


def utf16le(s: str) -> bytes:
    return s.encode("utf-16-le")


def filetime(unix_ts: int) -> int:
    return (unix_ts + 11644473600) * 10_000_000


class MiniAllocator:
    """Packs small stream payloads into the mini-stream, 64-byte-aligned."""

    def __init__(self):
        self.blob = bytearray()
        self.minifat = []

    def add(self, data: bytes):
        start = len(self.blob) // MINI_SECTOR_SIZE
        n = max(1, -(-len(data) // MINI_SECTOR_SIZE))
        padded = data + b"\x00" * (n * MINI_SECTOR_SIZE - len(data))
        self.blob.extend(padded)
        for i in range(n):
            self.minifat.append(start + i + 1 if i < n - 1 else ENDOFCHAIN)
        return start, len(data)


class RegularAllocator:
    """Packs payloads into normal 512-byte sectors, chained via the FAT."""

    def __init__(self, first_free_sector: int):
        self.sectors = []
        self.fat_entries = {}
        self.next_index = first_free_sector

    def add(self, data: bytes):
        start = self.next_index
        n = max(1, -(-len(data) // SECTOR_SIZE))
        padded = data + b"\x00" * (n * SECTOR_SIZE - len(data))
        for i in range(n):
            self.sectors.append(padded[i * SECTOR_SIZE:(i + 1) * SECTOR_SIZE])
        for i in range(n):
            self.fat_entries[self.next_index + i] = (
                self.next_index + i + 1 if i < n - 1 else ENDOFCHAIN
            )
        self.next_index += n
        return start, len(data)


def dir_name_field(name: str) -> bytes:
    raw = utf16le(name) + b"\x00\x00"
    assert len(raw) <= 64
    return raw + b"\x00" * (64 - len(raw))


class DirEntry:
    def __init__(self, name, obj_type, left=NOSTREAM, right=NOSTREAM,
                 child=NOSTREAM, start_sector=0, size=0):
        self.name = name
        self.obj_type = obj_type
        self.left = left
        self.right = right
        self.child = child
        self.start_sector = start_sector
        self.size = size

    def pack(self) -> bytes:
        name_bytes = utf16le(self.name) + b"\x00\x00"
        out = dir_name_field(self.name)
        out += struct.pack("<H", len(name_bytes))
        out += struct.pack("<B", self.obj_type)
        out += struct.pack("<B", 1)  # color: irrelevant for reading
        out += struct.pack("<III", self.left, self.right, self.child)
        out += b"\x00" * 16  # CLSID
        out += struct.pack("<I", 0)  # state bits
        out += b"\x00" * 8  # creation time
        out += b"\x00" * 8  # modified time
        out += struct.pack("<I", self.start_sector)
        out += struct.pack("<Q", self.size)
        assert len(out) == 128
        return out


def prop_entry(prop_id: int, prop_type: int, value8: bytes) -> bytes:
    assert len(value8) == 8
    return struct.pack("<HHI", prop_type, prop_id, 0) + value8


def build(out_path: str):
    mini = MiniAllocator()

    # --- Top-level properties stream (header: 32 bytes for the root Message
    # object) with just the one fixed-length property libmsg reads directly
    # from the store: PR_CLIENT_SUBMIT_TIME (0x0039, PtypTime/0x0040). ---
    top_props = b"\x00" * 32
    top_props += prop_entry(0x0039, 0x0040, struct.pack("<Q", filetime(SUBMIT_TIME_UNIX)))
    s_top_props = mini.add(top_props)

    s_subject = mini.add(utf16le(SUBJECT))
    s_body = mini.add(utf16le(BODY))
    s_html = mini.add(HTML)
    s_sender_name = mini.add(utf16le(SENDER_NAME))
    s_sender_email = mini.add(utf16le(SENDER_EMAIL))

    # --- Recipient properties stream (header: 8 bytes) with
    # PR_RECIPIENT_TYPE (0x0C15, PtypInteger32) = 1 (TO). ---
    recip_props = b"\x00" * 8
    recip_props += prop_entry(0x0C15, 0x0003, struct.pack("<i", 1) + b"\x00" * 4)
    s_recip_props = mini.add(recip_props)
    s_recip_name = mini.add(utf16le(RECIPIENT_NAME))
    s_recip_email = mini.add(utf16le(RECIPIENT_EMAIL))

    s_att_filename = mini.add(utf16le(ATTACHMENT_FILENAME))
    s_att_mimetype = mini.add(utf16le(ATTACHMENT_MIMETYPE))
    s_att_content_id = mini.add(utf16le(ATTACHMENT_CONTENT_ID))

    assert len(ATTACHMENT_DATA) >= MINI_STREAM_CUTOFF, "attachment must exercise the normal-FAT path"

    # Directory sectors: 16 entries at 4 per sector -> sectors 1..4.
    NUM_ENTRIES = 16
    dir_sectors_needed = -(-NUM_ENTRIES // 4)
    first_dir_sector = 1
    minifat_sector = first_dir_sector + dir_sectors_needed  # 5
    first_free_regular = minifat_sector + 1  # 6

    regular = RegularAllocator(first_free_regular)
    s_root = regular.add(bytes(mini.blob))
    s_att_data = regular.add(ATTACHMENT_DATA)

    # --- Directory entries (ids fixed to match the layout in the module
    # docstring). ---
    entries = [None] * NUM_ENTRIES
    entries[0] = DirEntry("Root Entry", 5, child=1,
                           start_sector=s_root[0], size=s_root[1])
    entries[1] = DirEntry("__properties_version1.0", 2, right=2,
                           start_sector=s_top_props[0], size=s_top_props[1])
    entries[2] = DirEntry("__substg1.0_0037001F", 2, right=3,
                           start_sector=s_subject[0], size=s_subject[1])
    entries[3] = DirEntry("__substg1.0_1000001F", 2, right=4,
                           start_sector=s_body[0], size=s_body[1])
    entries[4] = DirEntry("__substg1.0_10130102", 2, right=5,
                           start_sector=s_html[0], size=s_html[1])
    entries[5] = DirEntry("__substg1.0_0C1A001F", 2, right=6,
                           start_sector=s_sender_name[0], size=s_sender_name[1])
    entries[6] = DirEntry("__substg1.0_5D01001F", 2, right=7,
                           start_sector=s_sender_email[0], size=s_sender_email[1])
    entries[7] = DirEntry("__recip_version1.0_#00000000", 1, right=8, child=9)
    entries[8] = DirEntry("__attach_version1.0_#00000000", 1, child=12)
    entries[9] = DirEntry("__properties_version1.0", 2, right=10,
                           start_sector=s_recip_props[0], size=s_recip_props[1])
    entries[10] = DirEntry("__substg1.0_3001001F", 2, right=11,
                            start_sector=s_recip_name[0], size=s_recip_name[1])
    entries[11] = DirEntry("__substg1.0_39FE001F", 2,
                            start_sector=s_recip_email[0], size=s_recip_email[1])
    entries[12] = DirEntry("__substg1.0_3707001F", 2, right=13,
                            start_sector=s_att_filename[0], size=s_att_filename[1])
    entries[13] = DirEntry("__substg1.0_370E001F", 2, right=15,
                            start_sector=s_att_mimetype[0], size=s_att_mimetype[1])
    entries[14] = DirEntry("__substg1.0_37010102", 2,
                            start_sector=s_att_data[0], size=s_att_data[1])
    entries[15] = DirEntry("__substg1.0_3712001F", 2, right=14,
                            start_sector=s_att_content_id[0], size=s_att_content_id[1])

    dir_bytes = bytearray()
    for e in entries:
        dir_bytes += e.pack()
    while len(dir_bytes) % SECTOR_SIZE != 0:
        dir_bytes += b"\x00"

    total_sectors = first_free_regular + regular.next_index - first_free_regular
    total_sectors = regular.next_index  # sector count is just the high-water mark

    fat = [FREESECT] * total_sectors
    fat[0] = FATSECT
    for i in range(dir_sectors_needed):
        sector = first_dir_sector + i
        fat[sector] = sector + 1 if i < dir_sectors_needed - 1 else ENDOFCHAIN
    fat[minifat_sector] = ENDOFCHAIN
    for idx, nxt in regular.fat_entries.items():
        fat[idx] = nxt

    assert total_sectors <= 128, "test fixture grew past a single FAT sector"

    fat_sector_bytes = b"".join(struct.pack("<I", v) for v in fat)
    fat_sector_bytes += b"\xff" * (SECTOR_SIZE - len(fat_sector_bytes))

    minifat_entries = list(mini.minifat)
    minifat_sector_bytes = b"".join(struct.pack("<I", v) for v in minifat_entries)
    minifat_sector_bytes += b"\xff" * (SECTOR_SIZE - len(minifat_sector_bytes))
    assert len(minifat_entries) * MINI_SECTOR_SIZE <= SECTOR_SIZE * (SECTOR_SIZE // 4), \
        "test fixture grew past a single mini-FAT sector"

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
    struct.pack_into("<I", header, 56, MINI_STREAM_CUTOFF)
    struct.pack_into("<I", header, 60, minifat_sector)
    struct.pack_into("<I", header, 64, 1)       # num mini FAT sectors
    struct.pack_into("<I", header, 68, ENDOFCHAIN)  # first DIFAT sector
    struct.pack_into("<I", header, 72, 0)       # num DIFAT sectors
    difat = [0] + [FREESECT] * 108
    for i, v in enumerate(difat):
        struct.pack_into("<I", header, 76 + i * 4, v)
    assert len(header) == 512

    out = bytearray()
    out += header
    out += fat_sector_bytes           # sector 0
    out += dir_bytes                  # sectors 1..dir_sectors_needed
    out += minifat_sector_bytes       # sector minifat_sector
    for sect in regular.sectors:
        out += sect

    Path(out_path).write_bytes(bytes(out))
    print(f"wrote {out_path} ({len(out)} bytes, {total_sectors} sectors)")


if __name__ == "__main__":
    build(sys.argv[1] if len(sys.argv) > 1 else "sample.msg")
