#include "msg_properties.h"
#include "msg_strings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rd_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t rd_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int msg_prop_store_parse(const uint8_t *data, size_t size, uint32_t header_skip,
                          msg_prop_store_t *out) {
    out->props = NULL;
    out->count = 0;
    if (!data || size <= header_skip) return 0; /* empty store is not an error */

    size_t remaining = size - header_skip;
    size_t count = remaining / 16;
    if (count == 0) return 0;

    msg_prop_t *props = (msg_prop_t *)malloc(sizeof(msg_prop_t) * count);
    if (!props) return -1;

    const uint8_t *p = data + header_skip;
    for (size_t i = 0; i < count; i++) {
        const uint8_t *e = p + i * 16;
        props[i].type = rd_u16(e);
        props[i].prop_id = rd_u16(e + 2);
        props[i].flags = rd_u32(e + 4);
        memcpy(props[i].value, e + 8, 8);
    }

    out->props = props;
    out->count = count;
    return 0;
}

void msg_prop_store_free(msg_prop_store_t *store) {
    if (!store) return;
    free(store->props);
    store->props = NULL;
    store->count = 0;
}

const msg_prop_t *msg_prop_store_find(const msg_prop_store_t *store,
                                       uint16_t prop_id, uint16_t type) {
    if (!store) return NULL;
    for (size_t i = 0; i < store->count; i++) {
        if (store->props[i].prop_id == prop_id && store->props[i].type == type) {
            return &store->props[i];
        }
    }
    return NULL;
}

int msg_prop_get_i32(const msg_prop_store_t *store, uint16_t prop_id, int32_t *out) {
    const msg_prop_t *p = msg_prop_store_find(store, prop_id, 0x0003);
    if (!p) return -1;
    *out = (int32_t)rd_u32(p->value);
    return 0;
}

int msg_prop_get_time(const msg_prop_store_t *store, uint16_t prop_id, time_t *out) {
    const msg_prop_t *p = msg_prop_store_find(store, prop_id, 0x0040);
    if (!p) return -1;
    uint64_t filetime = (uint64_t)rd_u32(p->value) | ((uint64_t)rd_u32(p->value + 4) << 32);
    return msg_filetime_to_unix(filetime, out);
}

static void build_stream_name(char *buf, size_t buf_size, uint16_t prop_id, const char *type_suffix) {
    snprintf(buf, buf_size, "__substg1.0_%04X%s", prop_id, type_suffix);
}

char *msg_get_string_prop(const cfb_t *cfb, uint32_t parent_id,
                           uint16_t prop_id, uint32_t codepage) {
    char name[32];

    build_stream_name(name, sizeof(name), prop_id, "001F");
    uint32_t entry = cfb_find_child(cfb, parent_id, name);
    if (entry != CFB_NO_ENTRY) {
        size_t size = 0;
        uint8_t *data = cfb_read_stream(cfb, entry, &size);
        if (!data) return NULL;
        char *result = msg_utf16le_to_utf8(data, size);
        free(data);
        return result;
    }

    build_stream_name(name, sizeof(name), prop_id, "001E");
    entry = cfb_find_child(cfb, parent_id, name);
    if (entry != CFB_NO_ENTRY) {
        size_t size = 0;
        uint8_t *data = cfb_read_stream(cfb, entry, &size);
        if (!data) return NULL;
        char *result = msg_codepage_to_utf8(data, size, codepage);
        free(data);
        return result;
    }

    return NULL;
}

uint8_t *msg_get_binary_prop(const cfb_t *cfb, uint32_t parent_id,
                              uint16_t prop_id, size_t *out_size) {
    char name[32];
    build_stream_name(name, sizeof(name), prop_id, "0102");
    uint32_t entry = cfb_find_child(cfb, parent_id, name);
    if (entry == CFB_NO_ENTRY) {
        *out_size = 0;
        return NULL;
    }
    return cfb_read_stream(cfb, entry, out_size);
}
