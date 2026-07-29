#ifndef LIBMSG_RTF_H
#define LIBMSG_RTF_H

#include <stddef.h>
#include <stdint.h>

/* Decompresses a PR_RTF_COMPRESSED (0x1009) stream per MS-OXRTFCP: a 16-byte
 * header (CompressedSize, UncompressedSize, Magic, CRC, all little-endian
 * uint32) followed by either raw RTF ("MELA" magic) or an LZ77 stream over a
 * 4096-byte ring buffer preseeded with a fixed 207-byte dictionary ("LZFu"
 * magic). Returns a malloc'd, NUL-terminated buffer of *out_size bytes of
 * raw RTF markup, or NULL if the header is malformed. Tolerant of truncated/
 * corrupt input: decompression just stops early rather than reading or
 * writing out of bounds. */
uint8_t *msg_rtf_decompress(const uint8_t *data, size_t size, size_t *out_size);

/* Strips RTF control words/groups down to the human-readable text: known
 * non-content destinations (fonttbl, colortbl, stylesheet, info, pict,
 * object, generator, ...) are skipped, \par/\line become newlines, \tab
 * becomes a tab, and \'hh / \uN escapes are decoded through `codepage`.
 * Always returns a malloc'd, NUL-terminated UTF-8 string (empty if the RTF
 * has no visible text), never NULL, so it is always safe to use as a plain
 * text body fallback. */
char *msg_rtf_to_text(const uint8_t *rtf, size_t size, uint32_t codepage);

/* Reconstructs the original HTML from RTF that encapsulates it (MS-OXRTFEX:
 * a "\fromhtml1" marked stream where the real markup lives in "\htmltag"
 * destinations and the plain-RTF fallback formatting is wrapped in
 * "\htmlrtf"/"\htmlrtf0" and dropped). Returns a malloc'd UTF-8 string, or
 * NULL if the RTF isn't HTML-encapsulated (plain "Rich Text" formatted
 * messages have no HTML to recover; callers should fall back to
 * msg_rtf_to_text() in that case). */
char *msg_rtf_to_html(const uint8_t *rtf, size_t size, uint32_t codepage);

#endif /* LIBMSG_RTF_H */
