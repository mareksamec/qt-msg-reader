/* Opens tests/sample_rtf.msg (built by make_rtf_test_msg.py) - a message
 * with ONLY PR_RTF_COMPRESSED set, no PR_BODY or PR_HTML - and checks that
 * msg_open()'s RTF fallback recovers both a plain text body and, since the
 * RTF is "\fromhtml1"-encapsulated, the original HTML. This is the
 * end-to-end counterpart to test_rtf.c's direct msg_rtf.c unit tests: it
 * exercises the real CFB/property-parsing pipeline, including the
 * PR_RTF_COMPRESSED (0x1009) property lookup wired up in msg_reader.c. */
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

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <sample_rtf.msg>\n", argv[0]);
        return 1;
    }

    msg_error_t err;
    msg_file_t *msg = msg_open(argv[1], &err);
    CHECK(msg != NULL, "msg_open failed: %s", msg ? "" : msg_error_string(err));
    if (!msg) return 1;

    const char *body = msg_get_body(msg);
    CHECK(body && strcmp(body, "Hello RTF HTML") == 0,
          "expected body \"Hello RTF HTML\", got: %s", body ? body : "(null)");

    const char *html = msg_get_html_body(msg);
    CHECK(html && strcmp(html, "<html><body>Hello RTF HTML</body></html>") == 0,
          "expected reconstructed html body, got: %s", html ? html : "(null)");

    msg_close(msg);

    if (failures == 0) {
        printf("all rtf msg checks passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", failures);
    return 1;
}
