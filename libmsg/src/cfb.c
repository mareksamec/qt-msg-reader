#include "cfb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CFB_HEADER_SIZE 512u
#define CFB_FREESECT 0xFFFFFFFFu
#define CFB_ENDOFCHAIN 0xFFFFFFFEu
#define CFB_FATSECT 0xFFFFFFFDu
#define CFB_DIFSECT 0xFFFFFFFCu
#define CFB_DIR_ENTRY_SIZE 128u

static const uint8_t CFB_SIGNATURE[8] = {
    0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1
};

static uint16_t rd_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t rd_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t rd_u64(const uint8_t *p) {
    return (uint64_t)rd_u32(p) | ((uint64_t)rd_u32(p + 4) << 32);
}

/* Follows a FAT chain, copying data chunk by chunk out of `base` (which is
 * either the raw file's sector area or the cached mini-stream). Stops
 * early (without failing) if the file is truncated/corrupt; the caller
 * gets however many bytes could actually be recovered. */
static uint8_t *read_chain(const uint32_t *fat, size_t fat_count,
                            const uint8_t *base, size_t base_size,
                            uint32_t chunk_size, uint32_t start,
                            uint64_t total_size, size_t *out_size) {
    /* A stream's declared size comes straight from the (possibly hostile)
     * directory entry and can claim up to 2^64 bytes. It can never
     * legitimately exceed the sector arena backing it, so clamp here
     * rather than let a corrupt/malicious file trigger a multi-gigabyte
     * allocation and zero-fill for a file that's actually a few KB. */
    if (total_size > (uint64_t)base_size) total_size = (uint64_t)base_size;

    if (total_size == 0) {
        uint8_t *empty = (uint8_t *)malloc(1);
        if (!empty) return NULL;
        *out_size = 0;
        return empty;
    }

    uint8_t *out = (uint8_t *)malloc((size_t)total_size);
    if (!out) return NULL;

    size_t written = 0;
    uint32_t chain = start;
    size_t max_iter = fat_count + 1;
    size_t iter = 0;

    while (chain != CFB_ENDOFCHAIN && chain != CFB_FREESECT &&
           chain != CFB_FATSECT && chain != CFB_DIFSECT &&
           written < total_size && iter < max_iter) {
        size_t offset = (size_t)chain * chunk_size;
        if (offset >= base_size) break;

        size_t avail_in_chunk = base_size - offset;
        size_t want = (size_t)total_size - written;
        size_t copy = chunk_size < avail_in_chunk ? chunk_size : avail_in_chunk;
        if (copy > want) copy = want;

        memcpy(out + written, base + offset, copy);
        written += copy;

        if (copy < chunk_size) break; /* ran off the end of `base` */
        if (chain >= fat_count) break;
        chain = fat[chain];
        iter++;
    }

    if (written < total_size) memset(out + written, 0, total_size - written);
    *out_size = (size_t)total_size;
    return out;
}

static int build_fat(cfb_t *cfb, const uint8_t *header) {
    uint32_t num_fat_sectors = rd_u32(header + 44);
    uint32_t first_difat_sector = rd_u32(header + 68);
    uint32_t num_difat_sectors = rd_u32(header + 72);

    /* These counts are attacker-controlled and otherwise unbounded; a
     * corrupt/hostile file could claim billions of FAT or DIFAT sectors,
     * which would balloon the allocation below and the DIFAT walk's
     * iteration budget even though the file itself only has a handful of
     * real sectors. Neither count can legitimately exceed the number of
     * sectors that actually fit in the file. */
    size_t sector_cap = cfb->total_sectors + 1;
    if (num_fat_sectors > sector_cap) num_fat_sectors = (uint32_t)sector_cap;
    if (num_difat_sectors > sector_cap) num_difat_sectors = (uint32_t)sector_cap;

    /* Gather the list of sectors that hold FAT entries. */
    uint32_t *fat_sectors = (uint32_t *)malloc(sizeof(uint32_t) * (num_fat_sectors + 1));
    if (!fat_sectors) return CFB_ERR_MEMORY;
    size_t fat_sector_count = 0;

    for (int i = 0; i < 109 && fat_sector_count < num_fat_sectors; i++) {
        uint32_t sect = rd_u32(header + 76 + (size_t)i * 4);
        if (sect == CFB_FREESECT) break;
        fat_sectors[fat_sector_count++] = sect;
    }

    uint32_t difat_sector = first_difat_sector;
    size_t entries_per_difat = cfb->sector_size / 4 - 1;
    size_t difat_iter = 0;
    while (difat_sector != CFB_ENDOFCHAIN && difat_sector != CFB_FREESECT &&
           difat_iter < num_difat_sectors + 1 && fat_sector_count < num_fat_sectors) {
        size_t offset = CFB_HEADER_SIZE + (size_t)difat_sector * cfb->sector_size;
        if (offset + cfb->sector_size > cfb->size) break;
        const uint8_t *sect_data = cfb->data + offset;

        for (size_t i = 0; i < entries_per_difat && fat_sector_count < num_fat_sectors; i++) {
            uint32_t sect = rd_u32(sect_data + i * 4);
            if (sect == CFB_FREESECT) break;
            uint32_t *grown = (uint32_t *)realloc(fat_sectors, sizeof(uint32_t) * (fat_sector_count + 1));
            if (!grown) { free(fat_sectors); return CFB_ERR_MEMORY; }
            fat_sectors = grown;
            fat_sectors[fat_sector_count++] = sect;
        }
        difat_sector = rd_u32(sect_data + entries_per_difat * 4);
        difat_iter++;
    }

    /* Now read the actual FAT sectors into one contiguous array. */
    size_t entries_per_sector = cfb->sector_size / 4;
    uint32_t *fat = (uint32_t *)malloc(sizeof(uint32_t) * entries_per_sector * fat_sector_count);
    if (!fat) { free(fat_sectors); return CFB_ERR_MEMORY; }

    size_t fat_count = 0;
    for (size_t i = 0; i < fat_sector_count; i++) {
        size_t offset = CFB_HEADER_SIZE + (size_t)fat_sectors[i] * cfb->sector_size;
        if (offset + cfb->sector_size > cfb->size) continue;
        const uint8_t *sect_data = cfb->data + offset;
        for (size_t j = 0; j < entries_per_sector; j++) {
            fat[fat_count++] = rd_u32(sect_data + j * 4);
        }
    }

    free(fat_sectors);
    cfb->fat = fat;
    cfb->fat_count = fat_count;
    return CFB_OK;
}

static int build_minifat(cfb_t *cfb, const uint8_t *header) {
    uint32_t first_minifat_sector = rd_u32(header + 60);
    uint32_t num_minifat_sectors = rd_u32(header + 64);

    /* Same reasoning as the FAT/DIFAT counts in build_fat(): don't let a
     * hostile count balloon the allocation below. */
    size_t sector_cap = cfb->total_sectors + 1;
    if (num_minifat_sectors > sector_cap) num_minifat_sectors = (uint32_t)sector_cap;

    if (num_minifat_sectors == 0) {
        cfb->minifat = NULL;
        cfb->minifat_count = 0;
        return CFB_OK;
    }

    size_t entries_per_sector = cfb->sector_size / 4;
    uint32_t *minifat = (uint32_t *)malloc(sizeof(uint32_t) * entries_per_sector * num_minifat_sectors);
    if (!minifat) return CFB_ERR_MEMORY;

    size_t count = 0;
    uint32_t chain = first_minifat_sector;
    size_t max_iter = cfb->fat_count + 1;
    size_t iter = 0;
    while (chain != CFB_ENDOFCHAIN && chain != CFB_FREESECT && iter < max_iter) {
        size_t offset = CFB_HEADER_SIZE + (size_t)chain * cfb->sector_size;
        if (offset + cfb->sector_size > cfb->size) break;
        const uint8_t *sect_data = cfb->data + offset;
        for (size_t j = 0; j < entries_per_sector; j++) {
            minifat[count++] = rd_u32(sect_data + j * 4);
        }
        if (chain >= cfb->fat_count) break;
        chain = cfb->fat[chain];
        iter++;
    }

    cfb->minifat = minifat;
    cfb->minifat_count = count;
    return CFB_OK;
}

static void utf16le_name_to_utf8(const uint8_t *utf16, size_t len_bytes, char *out, size_t out_cap) {
    /* CFB directory names are at most 31 UTF-16 code units + a NUL. We only
     * need enough fidelity to recognize the fixed ASCII names used inside
     * .msg files (__properties_version1.0, __substg1.0_XXXXYYYY, etc.), so
     * this handles the BMP well enough and falls back to '?' for anything
     * outside ASCII (none of the names we care about use non-ASCII). */
    size_t out_pos = 0;
    size_t chars = len_bytes / 2;
    for (size_t i = 0; i < chars && out_pos + 1 < out_cap; i++) {
        uint16_t code = rd_u16(utf16 + i * 2);
        if (code == 0) break;
        if (code < 0x80) {
            out[out_pos++] = (char)code;
        } else {
            out[out_pos++] = '?';
        }
    }
    out[out_pos] = '\0';
}

static int build_directory(cfb_t *cfb, const uint8_t *header) {
    uint32_t first_dir_sector = rd_u32(header + 48);

    /* Collect the directory stream's sector chain, then parse fixed-size
     * 128-byte entries out of it directly (avoids a dependency on
     * cfb_read_stream, which needs the directory to already exist). */
    size_t cap = 16;
    cfb_dir_entry_t *entries = (cfb_dir_entry_t *)malloc(sizeof(cfb_dir_entry_t) * cap);
    if (!entries) return CFB_ERR_MEMORY;
    size_t count = 0;

    uint32_t chain = first_dir_sector;
    size_t max_iter = cfb->fat_count + 1;
    size_t iter = 0;
    while (chain != CFB_ENDOFCHAIN && chain != CFB_FREESECT && iter < max_iter) {
        size_t offset = CFB_HEADER_SIZE + (size_t)chain * cfb->sector_size;
        if (offset + cfb->sector_size > cfb->size) break;
        const uint8_t *sect_data = cfb->data + offset;

        size_t entries_this_sector = cfb->sector_size / CFB_DIR_ENTRY_SIZE;
        for (size_t i = 0; i < entries_this_sector; i++) {
            const uint8_t *e = sect_data + i * CFB_DIR_ENTRY_SIZE;
            if (count == cap) {
                cap *= 2;
                cfb_dir_entry_t *grown = (cfb_dir_entry_t *)realloc(entries, sizeof(cfb_dir_entry_t) * cap);
                if (!grown) { free(entries); return CFB_ERR_MEMORY; }
                entries = grown;
            }
            cfb_dir_entry_t *d = &entries[count];
            uint16_t name_len = rd_u16(e + 64);
            if (name_len > 64) name_len = 64;
            utf16le_name_to_utf8(e + 0, name_len, d->name, sizeof(d->name));
            d->type = (cfb_entry_type_t)e[66];
            d->left_sibling = rd_u32(e + 68);
            d->right_sibling = rd_u32(e + 72);
            d->child = rd_u32(e + 76);
            d->start_sector = rd_u32(e + 116);
            d->size = rd_u64(e + 120);
            count++;
        }

        if (chain >= cfb->fat_count) break;
        chain = cfb->fat[chain];
        iter++;
    }

    cfb->entries = entries;
    cfb->entry_count = count;
    return CFB_OK;
}

int cfb_open(const char *path, cfb_t **out) {
    *out = NULL;
    FILE *f = fopen(path, "rb");
    if (!f) return CFB_ERR_IO;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return CFB_ERR_IO; }
    long fsize = ftell(f);
    if (fsize < (long)CFB_HEADER_SIZE) { fclose(f); return CFB_ERR_NOT_CFB; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return CFB_ERR_IO; }

    uint8_t *data = (uint8_t *)malloc((size_t)fsize);
    if (!data) { fclose(f); return CFB_ERR_MEMORY; }
    size_t read = fread(data, 1, (size_t)fsize, f);
    fclose(f);
    if (read != (size_t)fsize) { free(data); return CFB_ERR_IO; }

    if (memcmp(data, CFB_SIGNATURE, 8) != 0) { free(data); return CFB_ERR_NOT_CFB; }

    uint16_t sector_shift = rd_u16(data + 30);
    uint16_t mini_sector_shift = rd_u16(data + 32);
    if (sector_shift < 6 || sector_shift > 20 || mini_sector_shift > sector_shift) {
        free(data);
        return CFB_ERR_CORRUPT;
    }

    cfb_t *cfb = (cfb_t *)calloc(1, sizeof(cfb_t));
    if (!cfb) { free(data); return CFB_ERR_MEMORY; }

    cfb->data = data;
    cfb->size = (size_t)fsize;
    cfb->sector_size = 1u << sector_shift;
    cfb->mini_sector_size = 1u << mini_sector_shift;
    cfb->mini_stream_cutoff = rd_u32(data + 56);
    cfb->total_sectors = (cfb->size - CFB_HEADER_SIZE) / cfb->sector_size;

    int rc = build_fat(cfb, data);
    if (rc != CFB_OK) { cfb_close(cfb); return rc; }

    rc = build_directory(cfb, data);
    if (rc != CFB_OK) { cfb_close(cfb); return rc; }

    if (cfb->entry_count == 0) { cfb_close(cfb); return CFB_ERR_CORRUPT; }

    rc = build_minifat(cfb, data);
    if (rc != CFB_OK) { cfb_close(cfb); return rc; }

    /* Cache the root entry's stream: this is the mini-stream container that
     * small streams' data actually lives in. */
    cfb_dir_entry_t *root = &cfb->entries[CFB_ROOT_ENTRY_ID];
    size_t mini_size = 0;
    uint8_t *mini_data = read_chain(cfb->fat, cfb->fat_count,
                                     cfb->data + CFB_HEADER_SIZE,
                                     cfb->total_sectors * (size_t)cfb->sector_size,
                                     cfb->sector_size, root->start_sector,
                                     root->size, &mini_size);
    if (!mini_data) { cfb_close(cfb); return CFB_ERR_MEMORY; }
    cfb->mini_stream = mini_data;
    cfb->mini_stream_size = mini_size;

    *out = cfb;
    return CFB_OK;
}

void cfb_close(cfb_t *cfb) {
    if (!cfb) return;
    free(cfb->data);
    free(cfb->fat);
    free(cfb->minifat);
    free(cfb->entries);
    free(cfb->mini_stream);
    free(cfb);
}

static int name_starts_with_ci(const char *name, const char *prefix) {
    while (*prefix) {
        char a = *name++, b = *prefix++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return 1;
}

static int name_equals_ci(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a++, cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
    }
    return *a == '\0' && *b == '\0';
}

/* Collects every entry reachable from a storage's "child" red-black tree
 * root (left/right siblings), appending their entry ids to the ids array
 * and bumping count. */
static void collect_children(const cfb_t *cfb, uint32_t node, uint32_t **ids,
                              size_t *count, size_t *cap, int depth) {
    if (node == CFB_NO_ENTRY || node >= cfb->entry_count || depth > 10000) return;
    const cfb_dir_entry_t *e = &cfb->entries[node];

    collect_children(cfb, e->left_sibling, ids, count, cap, depth + 1);

    if (*count == *cap) {
        size_t new_cap = *cap ? *cap * 2 : 8;
        uint32_t *grown = (uint32_t *)realloc(*ids, sizeof(uint32_t) * new_cap);
        if (grown) { *ids = grown; *cap = new_cap; }
    }
    if (*count < *cap) (*ids)[(*count)++] = node;

    collect_children(cfb, e->right_sibling, ids, count, cap, depth + 1);
}

uint32_t cfb_find_child(const cfb_t *cfb, uint32_t parent_id, const char *name) {
    if (parent_id >= cfb->entry_count) return CFB_NO_ENTRY;
    uint32_t *ids = NULL;
    size_t count = 0, cap = 0;
    collect_children(cfb, cfb->entries[parent_id].child, &ids, &count, &cap, 0);

    uint32_t found = CFB_NO_ENTRY;
    for (size_t i = 0; i < count; i++) {
        if (name_equals_ci(cfb->entries[ids[i]].name, name)) {
            found = ids[i];
            break;
        }
    }
    free(ids);
    return found;
}

static int cmp_name(const void *a, const void *b, void *ctx) {
    const cfb_t *cfb = (const cfb_t *)ctx;
    uint32_t ia = *(const uint32_t *)a, ib = *(const uint32_t *)b;
    return strcmp(cfb->entries[ia].name, cfb->entries[ib].name);
}

/* Simple insertion sort: the number of recipients/attachments in a .msg is
 * always small, so we avoid pulling in qsort_r/qsort_s portability issues. */
static void sort_by_name(const cfb_t *cfb, uint32_t *ids, size_t count) {
    for (size_t i = 1; i < count; i++) {
        uint32_t key = ids[i];
        size_t j = i;
        while (j > 0 && cmp_name(&key, &ids[j - 1], (void *)cfb) < 0) {
            ids[j] = ids[j - 1];
            j--;
        }
        ids[j] = key;
    }
}

uint32_t *cfb_find_children_prefix(const cfb_t *cfb, uint32_t parent_id,
                                    const char *prefix, size_t *out_count) {
    *out_count = 0;
    if (parent_id >= cfb->entry_count) return NULL;

    uint32_t *ids = NULL;
    size_t count = 0, cap = 0;
    collect_children(cfb, cfb->entries[parent_id].child, &ids, &count, &cap, 0);

    uint32_t *matches = NULL;
    size_t match_count = 0;
    if (count > 0) {
        matches = (uint32_t *)malloc(sizeof(uint32_t) * count);
        if (!matches) { free(ids); return NULL; }
        for (size_t i = 0; i < count; i++) {
            if (name_starts_with_ci(cfb->entries[ids[i]].name, prefix)) {
                matches[match_count++] = ids[i];
            }
        }
    }
    free(ids);

    sort_by_name(cfb, matches, match_count);
    *out_count = match_count;
    return matches;
}

uint8_t *cfb_read_stream(const cfb_t *cfb, uint32_t entry_id, size_t *out_size) {
    *out_size = 0;
    if (entry_id >= cfb->entry_count) return NULL;
    const cfb_dir_entry_t *e = &cfb->entries[entry_id];

    if (entry_id == CFB_ROOT_ENTRY_ID) {
        uint8_t *copy = (uint8_t *)malloc(cfb->mini_stream_size ? cfb->mini_stream_size : 1);
        if (!copy) return NULL;
        if (cfb->mini_stream_size) memcpy(copy, cfb->mini_stream, cfb->mini_stream_size);
        *out_size = cfb->mini_stream_size;
        return copy;
    }

    if (e->size < cfb->mini_stream_cutoff) {
        return read_chain(cfb->minifat, cfb->minifat_count,
                           cfb->mini_stream, cfb->mini_stream_size,
                           cfb->mini_sector_size, e->start_sector,
                           e->size, out_size);
    }

    return read_chain(cfb->fat, cfb->fat_count,
                       cfb->data + CFB_HEADER_SIZE,
                       cfb->total_sectors * (size_t)cfb->sector_size,
                       cfb->sector_size, e->start_sector,
                       e->size, out_size);
}
