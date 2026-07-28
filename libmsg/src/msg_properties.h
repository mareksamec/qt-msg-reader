#ifndef LIBMSG_PROPERTIES_H
#define LIBMSG_PROPERTIES_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "cfb.h"

/* Header sizes preceding the array of 16-byte property entries in a
 * "__properties_version1.0" stream, per MS-OXMSG 2.4.2. */
#define MSG_PROPSTORE_SKIP_TOPLEVEL 32u /* the root Message object */
#define MSG_PROPSTORE_SKIP_OTHER 8u     /* attachments, recipients */

typedef struct {
    uint16_t type;     /* MAPI property type, e.g. 0x001F, 0x0003, 0x0040 */
    uint16_t prop_id;
    uint32_t flags;
    uint8_t value[8];  /* fixed-length value, or {size, reserved} for variable-length */
} msg_prop_t;

typedef struct {
    msg_prop_t *props;
    size_t count;
} msg_prop_store_t;

/* Parses a raw "__properties_version1.0" stream. Returns 0 on success. */
int msg_prop_store_parse(const uint8_t *data, size_t size, uint32_t header_skip,
                          msg_prop_store_t *out);
void msg_prop_store_free(msg_prop_store_t *store);

const msg_prop_t *msg_prop_store_find(const msg_prop_store_t *store,
                                       uint16_t prop_id, uint16_t type);

int msg_prop_get_i32(const msg_prop_store_t *store, uint16_t prop_id, int32_t *out);
int msg_prop_get_time(const msg_prop_store_t *store, uint16_t prop_id, time_t *out);

/* Looks up prop_id as a string stream directly by name: tries the Unicode
 * form ("...001F") first, then the ANSI form ("...001E") decoded using
 * `codepage` (0 = assume Windows-1252). Returns a malloc'd UTF-8 string,
 * or NULL if neither stream is present. */
char *msg_get_string_prop(const cfb_t *cfb, uint32_t parent_id,
                           uint16_t prop_id, uint32_t codepage);

/* Reads a binary ("...0102") stream by prop_id. Returns a malloc'd buffer
 * (size in *out_size) or NULL if not present. */
uint8_t *msg_get_binary_prop(const cfb_t *cfb, uint32_t parent_id,
                              uint16_t prop_id, size_t *out_size);

#endif /* LIBMSG_PROPERTIES_H */
