/*
 * Minimal reader for the Compound File Binary format (MS-CFB), a.k.a. OLE2
 * structured storage. This is the container format .msg files are stored
 * in. Only reading is implemented, and only what's needed to walk the
 * directory tree and pull out stream contents.
 */
#ifndef LIBMSG_CFB_H
#define LIBMSG_CFB_H

#include <stddef.h>
#include <stdint.h>

#define CFB_ROOT_ENTRY_ID 0u
#define CFB_NO_ENTRY 0xFFFFFFFFu

typedef enum {
    CFB_ENTRY_UNKNOWN = 0,
    CFB_ENTRY_STORAGE = 1,
    CFB_ENTRY_STREAM = 2,
    CFB_ENTRY_ROOT = 5
} cfb_entry_type_t;

typedef struct {
    cfb_entry_type_t type;
    uint32_t left_sibling;
    uint32_t right_sibling;
    uint32_t child;
    uint32_t start_sector;
    uint64_t size;
    char name[128]; /* decoded to UTF-8; CFB names are <=32 UTF-16 chars */
} cfb_dir_entry_t;

typedef struct {
    uint8_t *data;      /* whole file contents */
    size_t size;

    uint32_t sector_size;      /* 512 or 4096 */
    uint32_t mini_sector_size; /* 64 */
    uint32_t mini_stream_cutoff;

    uint32_t *fat;
    size_t fat_count;

    uint32_t *minifat;
    size_t minifat_count;

    cfb_dir_entry_t *entries;
    size_t entry_count;

    uint8_t *mini_stream; /* root entry's stream contents, cached */
    size_t mini_stream_size;

    size_t total_sectors;
} cfb_t;

/* Returns 0 (CFB_OK) on success. Negative values are errors; see cfb.c. */
#define CFB_OK 0
#define CFB_ERR_IO (-1)
#define CFB_ERR_NOT_CFB (-2)
#define CFB_ERR_CORRUPT (-3)
#define CFB_ERR_MEMORY (-4)

int cfb_open(const char *path, cfb_t **out);
void cfb_close(cfb_t *cfb);

/* Finds a direct child of parent_id whose name matches exactly
 * (case-insensitive, ASCII). Returns CFB_NO_ENTRY if not found. */
uint32_t cfb_find_child(const cfb_t *cfb, uint32_t parent_id, const char *name);

/* Returns a malloc'd, name-sorted array of direct children of parent_id
 * whose name starts with `prefix` (case-insensitive). Caller frees the
 * array (not the entries, which live in cfb->entries). *out_count is set
 * even on empty results (to 0). Returns NULL only on allocation failure
 * with a non-zero match count. */
uint32_t *cfb_find_children_prefix(const cfb_t *cfb, uint32_t parent_id,
                                    const char *prefix, size_t *out_count);

/* Reads the full contents of a stream entry. Returns a malloc'd buffer
 * (caller frees) and sets *out_size. For a zero-length stream, returns a
 * non-NULL 0-byte allocation. Returns NULL on error. */
uint8_t *cfb_read_stream(const cfb_t *cfb, uint32_t entry_id, size_t *out_size);

#endif /* LIBMSG_CFB_H */
