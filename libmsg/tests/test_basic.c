/* Parses tests/sample.msg (built by make_test_msg.py) and checks every
 * field libmsg extracts against the values hardcoded in that generator. */
#include <libmsg/msg.h>

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, fmt, ...) \
    do { \
        if (!(cond)) { \
            failures++; \
            fprintf(stderr, "FAIL: " fmt "\n", ##__VA_ARGS__); \
        } \
    } while (0)

#define CHECK_STR(actual, expected) \
    CHECK((actual) && strcmp((actual), (expected)) == 0, \
          "%s: got %s, want \"%s\"", #actual, (actual) ? (actual) : "(null)", (expected))

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <sample.msg>\n", argv[0]);
        return 1;
    }

    msg_error_t err;
    msg_file_t *msg = msg_open(argv[1], &err);
    CHECK(msg != NULL, "msg_open failed: %s", msg ? "" : msg_error_string(err));
    if (!msg) return 1;

    CHECK_STR(msg_get_subject(msg), "Café ☕ Test Subject");
    CHECK_STR(msg_get_body(msg), "Hello,\r\nThis is a test message body.\r\n\r\nRegards.");
    CHECK_STR(msg_get_sender_name(msg), "Alice Example");
    CHECK_STR(msg_get_sender_email(msg), "alice@example.com");

    const char *html = msg_get_html_body(msg);
    CHECK(html && strstr(html, "Hello HTML") != NULL, "html body missing expected text, got: %s",
          html ? html : "(null)");

    time_t date;
    CHECK(msg_get_date(msg, &date) == 0, "msg_get_date failed");
    CHECK(date == (time_t)1710505800, "date mismatch: got %lld, want 1710505800",
          (long long)date);

    CHECK(msg_get_recipient_count(msg) == 1, "expected 1 recipient, got %zu",
          msg_get_recipient_count(msg));
    if (msg_get_recipient_count(msg) == 1) {
        const msg_recipient_t *r = msg_get_recipient(msg, 0);
        CHECK_STR(r->name, "Bob Recipient");
        CHECK_STR(r->email, "bob@example.com");
        CHECK(r->type == MSG_RECIPIENT_TO, "expected TO recipient, got %d", (int)r->type);
    }

    CHECK(msg_get_attachment_count(msg) == 1, "expected 1 attachment, got %zu",
          msg_get_attachment_count(msg));
    if (msg_get_attachment_count(msg) == 1) {
        const msg_attachment_t *a = msg_get_attachment(msg, 0);
        CHECK_STR(a->filename, "attachment.bin");
        CHECK_STR(a->mimetype, "application/octet-stream");
        CHECK_STR(a->content_id, "test-image@cid");
        CHECK(a->size == 5000, "expected 5000 bytes of attachment data, got %zu", a->size);
        int data_ok = 1;
        if (a->data) {
            for (size_t i = 0; i < a->size; i++) {
                if (a->data[i] != (unsigned char)(i % 256)) { data_ok = 0; break; }
            }
        } else {
            data_ok = 0;
        }
        CHECK(data_ok, "attachment data content mismatch");
    }

    msg_close(msg);

    if (failures == 0) {
        printf("all checks passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
