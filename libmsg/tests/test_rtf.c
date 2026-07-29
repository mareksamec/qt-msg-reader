/* Direct unit tests for msg_rtf.c: MS-OXRTFCP decompression and RTF -> text
 * / HTML extraction. These exercise the tokenizer in isolation (no CFB
 * container involved) so the trickier parsing rules - destination
 * skipping, \uN/\uc fallback-skip counting, \htmlrtf/\htmltag HTML
 * encapsulation, and the LZ77 self-overlapping back-reference copy - each
 * get a small, independently-verifiable case. */
#include "../src/msg_rtf.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;

#define CHECK(cond, fmt, ...) \
    do { \
        if (!(cond)) { \
            failures++; \
            fprintf(stderr, "FAIL: " fmt "\n", ##__VA_ARGS__); \
        } \
    } while (0)

/* Wraps `rtf` in an uncompressed ("MELA") PR_RTF_COMPRESSED payload, which
 * exercises the container/header parsing and the whole tokenizer without
 * depending on the LZ77 back-reference path. */
static uint8_t *wrap_uncompressed(const char *rtf, size_t *out_size) {
    size_t rtf_len = strlen(rtf);
    size_t total = 16 + rtf_len;
    uint8_t *buf = (uint8_t *)malloc(total);
    uint32_t compressed_size = (uint32_t)(rtf_len + 12);
    uint32_t uncompressed_size = (uint32_t)rtf_len;
    uint32_t magic = 0x414C454Du; /* "MELA" */
    uint32_t crc = 0;
    memcpy(buf + 0, &compressed_size, 4);
    memcpy(buf + 4, &uncompressed_size, 4);
    memcpy(buf + 8, &magic, 4);
    memcpy(buf + 12, &crc, 4);
    memcpy(buf + 16, rtf, rtf_len);
    *out_size = total;
    return buf;
}

static void test_uncompressed_roundtrip(void) {
    const char *rtf = "{\\rtf1 hello world}";
    size_t size;
    uint8_t *wrapped = wrap_uncompressed(rtf, &size);

    size_t out_size;
    uint8_t *decompressed = msg_rtf_decompress(wrapped, size, &out_size);
    CHECK(decompressed != NULL, "decompress returned NULL");
    if (decompressed) {
        CHECK(out_size == strlen(rtf), "size mismatch: got %zu want %zu", out_size, strlen(rtf));
        CHECK(memcmp(decompressed, rtf, out_size) == 0, "content mismatch: got %.*s",
              (int)out_size, decompressed);
    }
    free(decompressed);
    free(wrapped);
}

static void test_malformed_header(void) {
    uint8_t tiny[8] = {0};
    size_t out_size;
    uint8_t *r = msg_rtf_decompress(tiny, sizeof(tiny), &out_size);
    CHECK(r == NULL, "expected NULL for too-short header");
    CHECK(out_size == 0, "expected out_size 0, got %zu", out_size);
}

static void test_bad_magic(void) {
    const char *rtf = "{\\rtf1 x}";
    size_t size;
    uint8_t *wrapped = wrap_uncompressed(rtf, &size);
    wrapped[8] = 0xFF; wrapped[9] = 0xFF; wrapped[10] = 0xFF; wrapped[11] = 0xFF;
    size_t out_size;
    uint8_t *r = msg_rtf_decompress(wrapped, size, &out_size);
    CHECK(r == NULL, "expected NULL for bad magic");
    free(wrapped);
    free(r);
}

/* Hand-verified LZFu compressed sample encoding "aaaa" as one literal 'a'
 * followed by a length-3 back-reference to the offset that literal was just
 * written to (207, since the ring buffer's write cursor starts right after
 * the 207-byte preset dictionary). This specifically exercises the
 * self-overlapping copy case, where the reference reads bytes that were
 * written earlier in the very same copy loop:
 *   control byte 0x02: bit0=0 (literal 'a'), bit1=1 (2-byte reference token)
 *   reference token bytes for offset=207, length=3 (length field is length-2):
 *     b1 = offset >> 4        = 207 >> 4        = 0x0C
 *     b2 = (offset & 0xF)<<4 | (length-2) = (0xF << 4) | 0x1 = 0xF1 */
static void test_compressed_backreference(void) {
    uint8_t data[] = {
        0x02,       /* control byte */
        'a',        /* literal token (bit0=0) */
        0x0C, 0xF1  /* reference token (bit1=1): offset=207, length=3 */
    };

    uint32_t compressed_size = (uint32_t)(sizeof(data) + 12);
    uint32_t uncompressed_size = 4; /* "aaaa" */
    uint32_t magic = 0x75465A4Cu;   /* "LZFu" */
    uint32_t crc = 0;
    uint8_t buf[16 + sizeof(data)];
    memcpy(buf + 0, &compressed_size, 4);
    memcpy(buf + 4, &uncompressed_size, 4);
    memcpy(buf + 8, &magic, 4);
    memcpy(buf + 12, &crc, 4);
    memcpy(buf + 16, data, sizeof(data));

    size_t out_size;
    uint8_t *out = msg_rtf_decompress(buf, sizeof(buf), &out_size);
    CHECK(out != NULL, "compressed decompress returned NULL");
    if (out) {
        CHECK(out_size == 4, "expected 4 bytes, got %zu", out_size);
        CHECK(memcmp(out, "aaaa", 4) == 0, "expected 'aaaa', got %.*s", (int)out_size, out);
    }
    free(out);
}

static void test_plain_text_extraction(void) {
    const char *rtf =
        "{\\rtf1\\ansi\\deff0"
        "{\\fonttbl{\\f0 Arial;}}"
        "{\\colortbl;\\red0\\green0\\blue0;}"
        "{\\*\\generator Microsoft Word;}"
        "\\pard\\plain\\f0 Hello, \\'e9world\\par "
        "Second line\\tab tabbed\\par}";
    char *text = msg_rtf_to_text((const uint8_t *)rtf, strlen(rtf), 0);
    CHECK(text != NULL, "msg_rtf_to_text returned NULL");
    if (text) {
        CHECK(strstr(text, "Hello, ") != NULL, "missing greeting, got: %s", text);
        CHECK(strstr(text, "\xc3\xa9world") != NULL,
              "missing accented char (utf8 for cp1252 0xE9 + world), got: %s", text);
        CHECK(strstr(text, "Second line") != NULL, "missing second line, got: %s", text);
        CHECK(strstr(text, "\ttabbed") != NULL, "missing tab, got: %s", text);
        CHECK(strstr(text, "Arial") == NULL, "font table leaked into text: %s", text);
        CHECK(strstr(text, "Microsoft Word") == NULL, "generator leaked into text: %s", text);
        CHECK(strchr(text, '\n') != NULL, "expected a newline from \\par, got: %s", text);
    }
    free(text);
}

static void test_unicode_with_fallback(void) {
    /* U+2019 (right single quotation mark) with a 1-byte cp1252 fallback
     * ('92) that must be skipped once, not duplicated into the output. */
    const char *rtf = "{\\rtf1 It\\u8217\\'92s fine}";
    char *text = msg_rtf_to_text((const uint8_t *)rtf, strlen(rtf), 0);
    CHECK(text != NULL, "msg_rtf_to_text returned NULL");
    if (text) {
        CHECK(strstr(text, "It\xe2\x80\x99s fine") != NULL,
              "expected exactly one right-quote char, got: %s", text);
    }
    free(text);
}

static void test_non_html_returns_null(void) {
    const char *rtf = "{\\rtf1\\ansi\\deff0 Just plain rich text, no html.}";
    char *html = msg_rtf_to_html((const uint8_t *)rtf, strlen(rtf), 0);
    CHECK(html == NULL, "expected NULL for non-html-encapsulated RTF, got: %s", html ? html : "(null)");
    free(html);
}

static void test_html_reconstruction(void) {
    const char *rtf =
        "{\\rtf1\\ansi\\fromhtml1\\deff0"
        "{\\fonttbl{\\f0 Arial;}}"
        "{\\*\\htmltag64 <html><body>}"
        "{\\*\\htmltag72 <b>}"
        "\\htmlrtf \\b \\htmlrtf0 Bold text\\htmlrtf \\b0 \\htmlrtf0 "
        "{\\*\\htmltag76 </b>}"
        "{\\*\\htmltag92 </body></html>}"
        "}";
    char *html = msg_rtf_to_html((const uint8_t *)rtf, strlen(rtf), 0);
    CHECK(html != NULL, "msg_rtf_to_html returned NULL for html-encapsulated RTF");
    if (html) {
        CHECK(strstr(html, "<html><body>") != NULL, "missing <html><body>, got: %s", html);
        CHECK(strstr(html, "<b>Bold text</b>") != NULL, "missing formatted bold text, got: %s", html);
        CHECK(strstr(html, "Arial") == NULL, "font table leaked into html, got: %s", html);
    }
    free(html);
}

int main(void) {
    test_uncompressed_roundtrip();
    test_malformed_header();
    test_bad_magic();
    test_compressed_backreference();
    test_plain_text_extraction();
    test_unicode_with_fallback();
    test_non_html_returns_null();
    test_html_reconstruction();

    if (failures == 0) {
        printf("all rtf checks passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
