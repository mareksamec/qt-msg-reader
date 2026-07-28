/* Small demo/debug CLI for libmsg: dumps the fields the library extracts
 * from a .msg file to stdout. */
#include <libmsg/msg.h>

#include <stdio.h>
#include <time.h>

static const char *recipient_type_name(msg_recipient_type_t type) {
    switch (type) {
        case MSG_RECIPIENT_TO: return "To";
        case MSG_RECIPIENT_CC: return "Cc";
        case MSG_RECIPIENT_BCC: return "Bcc";
        default: return "?";
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <file.msg>\n", argv[0]);
        return 1;
    }

    msg_error_t err;
    msg_file_t *msg = msg_open(argv[1], &err);
    if (!msg) {
        fprintf(stderr, "error: %s\n", msg_error_string(err));
        return 1;
    }

    printf("Subject: %s\n", msg_get_subject(msg) ? msg_get_subject(msg) : "(none)");
    printf("From:    %s <%s>\n",
           msg_get_sender_name(msg) ? msg_get_sender_name(msg) : "(unknown)",
           msg_get_sender_email(msg) ? msg_get_sender_email(msg) : "");

    time_t date;
    if (msg_get_date(msg, &date) == 0) {
        char buf[64];
        struct tm tm_buf;
#if defined(_WIN32)
        gmtime_s(&tm_buf, &date);
#else
        gmtime_r(&date, &tm_buf);
#endif
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S UTC", &tm_buf);
        printf("Date:    %s\n", buf);
    } else {
        printf("Date:    (none)\n");
    }

    size_t rc = msg_get_recipient_count(msg);
    printf("Recipients (%zu):\n", rc);
    for (size_t i = 0; i < rc; i++) {
        const msg_recipient_t *r = msg_get_recipient(msg, i);
        printf("  [%s] %s <%s>\n", recipient_type_name(r->type),
               r->name ? r->name : "(unknown)", r->email ? r->email : "");
    }

    size_t ac = msg_get_attachment_count(msg);
    printf("Attachments (%zu):\n", ac);
    for (size_t i = 0; i < ac; i++) {
        const msg_attachment_t *a = msg_get_attachment(msg, i);
        printf("  %s (%s, %zu bytes)\n", a->filename ? a->filename : "(unnamed)",
               a->mimetype ? a->mimetype : "unknown type", a->size);
    }

    const char *body = msg_get_body(msg);
    printf("\n--- Body ---\n%s\n", body ? body : "(none)");

    msg_close(msg);
    return 0;
}
