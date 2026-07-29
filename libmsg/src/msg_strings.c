#include "msg_strings.h"

#include <stdlib.h>
#include <string.h>

size_t msg_utf8_encode(uint32_t cp, uint8_t *out) {
    if (cp <= 0x7F) {
        out[0] = (uint8_t)cp;
        return 1;
    } else if (cp <= 0x7FF) {
        out[0] = (uint8_t)(0xC0 | (cp >> 6));
        out[1] = (uint8_t)(0x80 | (cp & 0x3F));
        return 2;
    } else if (cp <= 0xFFFF) {
        out[0] = (uint8_t)(0xE0 | (cp >> 12));
        out[1] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (uint8_t)(0x80 | (cp & 0x3F));
        return 3;
    } else {
        out[0] = (uint8_t)(0xF0 | (cp >> 18));
        out[1] = (uint8_t)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (uint8_t)(0x80 | (cp & 0x3F));
        return 4;
    }
}

char *msg_utf16le_to_utf8(const uint8_t *data, size_t len) {
    size_t units = len / 2;
    /* Worst case: every unit is a 3-byte UTF-8 sequence, plus NUL. */
    uint8_t *out = (uint8_t *)malloc(units * 3 + 1);
    if (!out) return NULL;

    size_t out_pos = 0;
    for (size_t i = 0; i < units; i++) {
        uint16_t unit = (uint16_t)(data[i * 2] | (data[i * 2 + 1] << 8));
        uint32_t cp;

        if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < units) {
            uint16_t low = (uint16_t)(data[(i + 1) * 2] | (data[(i + 1) * 2 + 1] << 8));
            if (low >= 0xDC00 && low <= 0xDFFF) {
                cp = 0x10000 + (((uint32_t)(unit - 0xD800) << 10) | (low - 0xDC00));
                i++;
            } else {
                cp = 0xFFFD;
            }
        } else if (unit >= 0xD800 && unit <= 0xDFFF) {
            cp = 0xFFFD; /* lone surrogate */
        } else {
            cp = unit;
        }

        out_pos += msg_utf8_encode(cp, out + out_pos);
    }
    out[out_pos] = '\0';
    return (char *)out;
}

/* Windows-1252 code points for the 0x80-0x9F range, where it diverges from
 * Latin-1 (0 marks the handful of slots the codepage leaves undefined). */
static const uint16_t CP1252_HIGH[32] = {
    0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
    0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178
};

char *msg_codepage_to_utf8(const uint8_t *data, size_t len, uint32_t codepage) {
    (void)codepage; /* Only Windows-1252/Latin-1 is implemented; see header. */
    uint8_t *out = (uint8_t *)malloc(len * 3 + 1);
    if (!out) return NULL;

    size_t out_pos = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        uint32_t cp;
        if (b < 0x80 || b >= 0xA0) {
            cp = b;
        } else {
            cp = CP1252_HIGH[b - 0x80];
            if (cp == 0) cp = 0xFFFD;
        }
        out_pos += msg_utf8_encode(cp, out + out_pos);
    }
    out[out_pos] = '\0';
    return (char *)out;
}

int msg_filetime_to_unix(uint64_t filetime, time_t *out) {
    /* FILETIME epoch is 1601-01-01; Unix epoch is 1970-01-01. The gap is
     * 11644473600 seconds, in 100ns units: 116444736000000000. */
    const uint64_t EPOCH_DIFF = 116444736000000000ULL;
    if (filetime < EPOCH_DIFF) return -1;
    uint64_t secs = (filetime - EPOCH_DIFF) / 10000000ULL;
    *out = (time_t)secs;
    return 0;
}
