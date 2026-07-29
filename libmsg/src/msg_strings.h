#ifndef LIBMSG_STRINGS_H
#define LIBMSG_STRINGS_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* Encodes a single Unicode code point as UTF-8 into `out` (which must have
 * room for at least 4 bytes) and returns the number of bytes written. */
size_t msg_utf8_encode(uint32_t cp, uint8_t *out);

/* Converts a UTF-16LE byte buffer (as stored in "...001F" streams) to a
 * malloc'd, NUL-terminated UTF-8 string. Handles surrogate pairs. */
char *msg_utf16le_to_utf8(const uint8_t *data, size_t len);

/* Converts a Windows codepage-encoded byte buffer (as stored in "...001E"
 * streams) to a malloc'd, NUL-terminated UTF-8 string. Only Windows-1252
 * is implemented (by far the common case for legacy ANSI .msg files);
 * anything else falls back to a byte-for-byte Latin-1 style pass so we
 * never crash or drop data, we just may not map exotic codepages exactly. */
char *msg_codepage_to_utf8(const uint8_t *data, size_t len, uint32_t codepage);

/* Converts a Windows FILETIME (100ns intervals since 1601-01-01 UTC) to a
 * time_t. Returns 0 on success, -1 if the value can't be represented. */
int msg_filetime_to_unix(uint64_t filetime, time_t *out);

#endif /* LIBMSG_STRINGS_H */
