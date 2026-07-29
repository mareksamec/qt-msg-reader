#include "msg_rtf.h"
#include "msg_strings.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================
 * MS-OXRTFCP: PR_RTF_COMPRESSED decompression
 * ============================================================ */

#define RTF_MAGIC_COMPRESSED   0x75465A4Cu /* "LZFu" */
#define RTF_MAGIC_UNCOMPRESSED 0x414C454Du /* "MELA" */
#define RTF_DICT_SIZE 4096u

/* MS-OXRTFCP 2.3.1.1: the LZ ring buffer is preseeded with this fixed
 * 207-byte prelude so early back-references can hit common RTF boilerplate
 * without it ever having been written to the (initially empty) stream. */
static const char RTF_INIT_DICT[] =
    "{\\rtf1\\ansi\\mac\\deff0\\deftab720{\\fonttbl;}"
    "{\\f0\\fnil \\froman \\fswiss \\fmodern \\fscript "
    "\\fdecor MS Sans SerifSymbolArialTimes New RomanCourier"
    "{\\colortbl\\red0\\green0\\blue0\n\r\\par "
    "\\pard\\plain\\f0\\fs20\\b\\i\\u\\tab\\tx";
#define RTF_INIT_DICT_LEN 207u

static uint32_t rd_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint8_t *msg_rtf_decompress(const uint8_t *data, size_t size, size_t *out_size) {
    *out_size = 0;
    if (!data || size < 16) return NULL;

    uint32_t compressed_size = rd_u32le(data);
    uint32_t uncompressed_size = rd_u32le(data + 4);
    uint32_t magic = rd_u32le(data + 8);
    /* data + 12: CRC32 of the payload; not verified. */

    if ((uint64_t)compressed_size + 4u > (uint64_t)size) return NULL;
    size_t payload_len = compressed_size >= 12u ? (size_t)(compressed_size - 12u) : 0;
    const uint8_t *payload = data + 16;
    if (16 + payload_len > size) payload_len = size - 16; /* tolerate truncated input */

    uint8_t *out = (uint8_t *)malloc((size_t)uncompressed_size + 1);
    if (!out) return NULL;

    if (magic == RTF_MAGIC_UNCOMPRESSED) {
        size_t n = payload_len < uncompressed_size ? payload_len : uncompressed_size;
        memcpy(out, payload, n);
        out[n] = '\0';
        *out_size = n;
        return out;
    }

    if (magic != RTF_MAGIC_COMPRESSED) {
        free(out);
        return NULL;
    }

    uint8_t dict[RTF_DICT_SIZE];
    memcpy(dict, RTF_INIT_DICT, RTF_INIT_DICT_LEN);
    size_t write_pos = RTF_INIT_DICT_LEN;

    size_t in_pos = 0, out_len = 0;
    while (out_len < uncompressed_size && in_pos < payload_len) {
        uint8_t control = payload[in_pos++];
        for (int bit = 0; bit < 8 && out_len < uncompressed_size && in_pos < payload_len; bit++) {
            if (((control >> bit) & 1u) == 0) {
                uint8_t byte = payload[in_pos++];
                out[out_len++] = byte;
                dict[write_pos] = byte;
                write_pos = (write_pos + 1) % RTF_DICT_SIZE;
            } else {
                if (in_pos + 2 > payload_len) { in_pos = payload_len; break; }
                uint8_t b1 = payload[in_pos];
                uint8_t b2 = payload[in_pos + 1];
                in_pos += 2;
                uint32_t offset = ((uint32_t)b1 << 4) | (uint32_t)(b2 >> 4);
                uint32_t length = (uint32_t)(b2 & 0x0Fu) + 2u;
                for (uint32_t k = 0; k < length && out_len < uncompressed_size; k++) {
                    uint8_t c = dict[(offset + k) % RTF_DICT_SIZE];
                    out[out_len++] = c;
                    dict[write_pos] = c;
                    write_pos = (write_pos + 1) % RTF_DICT_SIZE;
                }
            }
        }
    }

    out[out_len] = '\0';
    *out_size = out_len;
    return out;
}

/* ============================================================
 * RTF -> plain text / HTML extraction
 * ============================================================ */

typedef struct {
    uint8_t *buf;
    size_t len, cap;
} bytebuf_t;

static void bb_push(bytebuf_t *b, const uint8_t *data, size_t n) {
    if (b->len + n > b->cap) {
        size_t newcap = b->cap ? b->cap * 2 : 256;
        while (newcap < b->len + n) newcap *= 2;
        uint8_t *grown = (uint8_t *)realloc(b->buf, newcap);
        if (!grown) return; /* keep prior content; better than crashing */
        b->buf = grown;
        b->cap = newcap;
    }
    if (b->len + n > b->cap) n = b->cap - b->len; /* the realloc above failed */
    memcpy(b->buf + b->len, data, n);
    b->len += n;
}

static void bb_push_byte(bytebuf_t *b, uint8_t c) { bb_push(b, &c, 1); }
static void bb_push_str(bytebuf_t *b, const char *s) { bb_push(b, (const uint8_t *)s, strlen(s)); }

typedef enum { RTF_MODE_TEXT, RTF_MODE_HTML } rtf_mode_t;

typedef struct {
    int skip;           /* inside a destination whose text isn't emitted */
    int uc;             /* current \ucN: number of fallback chars \uN is followed by */
    int html_suppress;  /* inside \htmlrtf ... \htmlrtf0 (HTML mode only) */
} rtf_group_t;

#define RTF_MAX_DEPTH 512

typedef struct {
    const uint8_t *p;
    const uint8_t *end;
    rtf_group_t stack[RTF_MAX_DEPTH];
    int depth;
    rtf_group_t cur;
    int uc_skip_remaining;
    bytebuf_t out;
    uint32_t codepage;
    rtf_mode_t mode;
} rtf_parser_t;

static int rtf_is_alpha(uint8_t c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int rtf_is_digit(uint8_t c) { return c >= '0' && c <= '9'; }

/* Known non-content destinations, matched by control word name (without a
 * leading "\*"). Anything else marked ignorable ("\*\whatever") that isn't
 * one of these and isn't "htmltag" in HTML mode is skipped too, per the
 * RTF spec's "ignorable destination" convention - see rtf_handle_group_start. */
static const char *const RTF_SKIP_DESTINATIONS[] = {
    "fonttbl", "colortbl", "stylesheet", "info", "generator",
    "header", "headerf", "headerl", "headerr",
    "footer", "footerf", "footerl", "footerr", "footnote",
    "pict", "object", "filetbl", "template", "doccomm",
    "listtable", "listoverridetable", "revtbl", "rsidtbl",
    "xmlnstbl", "fldinst", "wgrffmtfilter", "themedata",
    "colorschememapping", "latentstyles", "datastore",
    "bkmkstart", "bkmkend", "shppict", "nonshppict",
    NULL
};

static int rtf_is_skip_destination(const char *name, rtf_mode_t mode) {
    /* Raw markup text; useful for HTML reconstruction, noise for plain text. */
    if (mode == RTF_MODE_TEXT && strcmp(name, "htmltag") == 0) return 1;
    for (int i = 0; RTF_SKIP_DESTINATIONS[i]; i++) {
        if (strcmp(name, RTF_SKIP_DESTINATIONS[i]) == 0) return 1;
    }
    return 0;
}

static int rtf_emitting(const rtf_parser_t *rp) {
    if (rp->cur.skip) return 0;
    if (rp->mode == RTF_MODE_HTML && rp->cur.html_suppress) return 0;
    return 1;
}

static void rtf_emit_byte(rtf_parser_t *rp, uint8_t byte) {
    if (!rtf_emitting(rp)) return;
    if (byte < 0x80) {
        bb_push_byte(&rp->out, byte);
    } else {
        char *decoded = msg_codepage_to_utf8(&byte, 1, rp->codepage);
        if (decoded) {
            bb_push_str(&rp->out, decoded);
            free(decoded);
        }
    }
}

static void rtf_emit_cp(rtf_parser_t *rp, uint32_t cp) {
    if (!rtf_emitting(rp)) return;
    uint8_t buf[4];
    size_t n = msg_utf8_encode(cp, buf);
    bb_push(&rp->out, buf, n);
}

/* Reads a control word/symbol name (letters only) without advancing `p`,
 * for the group-start destination lookahead below. */
static size_t rtf_peek_name(const uint8_t *p, const uint8_t *end, char *out, size_t out_cap) {
    const uint8_t *start = p;
    while (p < end && rtf_is_alpha(*p)) p++;
    size_t len = (size_t)(p - start);
    if (len >= out_cap) len = out_cap - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return len;
}

/* Called right after consuming the "{" that opens a new group. Peeks ahead
 * (without moving rp->p) to see whether this group is a destination we
 * should skip, per MS-RTF's "\*\name" ignorable-destination convention plus
 * a hardcoded list of older destinations that were never marked ignorable
 * (fonttbl, colortbl, info, ...). The control word(s) themselves are left
 * for the main loop to consume normally right after this returns. */
static void rtf_handle_group_start(rtf_parser_t *rp) {
    if (rp->depth < RTF_MAX_DEPTH) {
        rp->stack[rp->depth++] = rp->cur;
    }
    rp->uc_skip_remaining = 0;

    const uint8_t *q = rp->p;
    int ignorable = 0;

    if (q < rp->end && *q == '\\') {
        q++;
        if (q < rp->end && *q == '*') {
            ignorable = 1;
            q++;
            if (q < rp->end && *q == '\\') q++;
        }
        char name[32];
        size_t name_len = rtf_peek_name(q, rp->end, name, sizeof(name));

        if (name_len > 0) {
            if (ignorable) {
                if (!(strcmp(name, "htmltag") == 0 && rp->mode == RTF_MODE_HTML)) {
                    rp->cur.skip = 1;
                }
            } else if (rtf_is_skip_destination(name, rp->mode)) {
                rp->cur.skip = 1;
            }
        } else if (ignorable) {
            rp->cur.skip = 1;
        }
    }
}

static void rtf_handle_group_end(rtf_parser_t *rp) {
    if (rp->depth > 0) {
        rp->cur = rp->stack[--rp->depth];
    }
    rp->uc_skip_remaining = 0;
}

static void rtf_handle_control(rtf_parser_t *rp) {
    if (rp->p >= rp->end) return;

    uint8_t c = *rp->p;

    if (!rtf_is_alpha(c)) {
        /* Control symbol: exactly one character, no parameter, no eaten space. */
        rp->p++;
        switch (c) {
            case '{': rtf_emit_byte(rp, '{'); break;
            case '}': rtf_emit_byte(rp, '}'); break;
            case '\\': rtf_emit_byte(rp, '\\'); break;
            case '~': rtf_emit_cp(rp, 0x00A0); break; /* non-breaking space */
            case '_': rtf_emit_byte(rp, '-'); break;  /* non-breaking hyphen */
            case '-': break;                          /* optional hyphen: invisible */
            case '*': break;                          /* ignorable-destination marker: handled at group start */
            case '\'': {
                if (rp->p + 1 < rp->end) {
                    char hex[3] = { (char)rp->p[0], (char)rp->p[1], 0 };
                    uint8_t byte = (uint8_t)strtoul(hex, NULL, 16);
                    rp->p += 2;
                    if (rp->uc_skip_remaining > 0) {
                        rp->uc_skip_remaining--;
                    } else {
                        rtf_emit_byte(rp, byte);
                    }
                }
                break;
            }
            default: break; /* unknown symbol: no effect */
        }
        return;
    }

    const uint8_t *name_start = rp->p;
    while (rp->p < rp->end && rtf_is_alpha(*rp->p)) rp->p++;
    size_t name_len = (size_t)(rp->p - name_start);
    char name[32];
    size_t clipped = name_len >= sizeof(name) ? sizeof(name) - 1 : name_len;
    memcpy(name, name_start, clipped);
    name[clipped] = '\0';

    int has_param = 0;
    long param = 0;
    int neg = 0;
    if (rp->p < rp->end && *rp->p == '-') { neg = 1; rp->p++; }
    while (rp->p < rp->end && rtf_is_digit(*rp->p)) {
        has_param = 1;
        param = param * 10 + (*rp->p - '0');
        rp->p++;
    }
    if (neg) param = -param;

    if (rp->p < rp->end && *rp->p == ' ') rp->p++; /* the one delimiting space */

    if (strcmp(name, "par") == 0 || strcmp(name, "line") == 0) {
        rtf_emit_byte(rp, '\n');
    } else if (strcmp(name, "tab") == 0) {
        rtf_emit_byte(rp, '\t');
    } else if (strcmp(name, "uc") == 0) {
        if (has_param) rp->cur.uc = (int)param;
    } else if (strcmp(name, "u") == 0) {
        uint16_t u16 = (uint16_t)(param < 0 ? param + 65536 : param);
        rtf_emit_cp(rp, u16);
        rp->uc_skip_remaining = rp->cur.uc;
    } else if (rp->mode == RTF_MODE_HTML && strcmp(name, "htmlrtf") == 0) {
        rp->cur.html_suppress = has_param ? (param != 0) : 1;
    }
    /* Everything else is formatting we don't need for extraction (\b, \i,
     * \fs20, \highlight, ...): consumed above, no output. */
}

static void rtf_parse(rtf_parser_t *rp) {
    while (rp->p < rp->end) {
        uint8_t c = *rp->p;
        if (c == '{') {
            rp->p++;
            rtf_handle_group_start(rp);
        } else if (c == '}') {
            rp->p++;
            rtf_handle_group_end(rp);
        } else if (c == '\\') {
            rp->p++;
            rtf_handle_control(rp);
        } else if (c == '\r' || c == '\n') {
            rp->p++; /* insignificant RTF source formatting */
        } else {
            rp->p++;
            if (rp->uc_skip_remaining > 0) {
                rp->uc_skip_remaining--;
            } else {
                rtf_emit_byte(rp, c);
            }
        }
    }
}

static char *rtf_run(const uint8_t *rtf, size_t size, uint32_t codepage, rtf_mode_t mode) {
    rtf_parser_t rp;
    memset(&rp, 0, sizeof(rp));
    rp.p = rtf;
    rp.end = rtf + size;
    rp.codepage = codepage;
    rp.mode = mode;
    rp.cur.uc = 1;

    rtf_parse(&rp);

    bb_push_byte(&rp.out, 0);
    if (!rp.out.buf) {
        uint8_t *empty = (uint8_t *)malloc(1);
        if (empty) empty[0] = '\0';
        return (char *)empty;
    }
    return (char *)rp.out.buf;
}

static int rtf_contains(const uint8_t *hay, size_t hay_len, const char *needle) {
    size_t needle_len = strlen(needle);
    if (needle_len == 0 || needle_len > hay_len) return 0;
    for (size_t i = 0; i + needle_len <= hay_len; i++) {
        if (memcmp(hay + i, needle, needle_len) == 0) return 1;
    }
    return 0;
}

char *msg_rtf_to_text(const uint8_t *rtf, size_t size, uint32_t codepage) {
    if (!rtf || size == 0) {
        uint8_t *empty = (uint8_t *)malloc(1);
        if (empty) empty[0] = '\0';
        return (char *)empty;
    }
    return rtf_run(rtf, size, codepage, RTF_MODE_TEXT);
}

char *msg_rtf_to_html(const uint8_t *rtf, size_t size, uint32_t codepage) {
    if (!rtf || size == 0) return NULL;
    if (!rtf_contains(rtf, size, "\\fromhtml1")) return NULL;

    char *html = rtf_run(rtf, size, codepage, RTF_MODE_HTML);
    if (html && html[0] == '\0') {
        free(html);
        return NULL; /* nothing usable was reconstructed */
    }
    return html;
}
