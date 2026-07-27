#include "libmsg/msg.h"

#include <stdlib.h>
#include <string.h>

#include "cfb.h"
#include "msg_properties.h"
#include "msg_strings.h"

/* MAPI property tags used below (see MS-OXPROPS / MS-OXMSG). Only the
 * "main" ones extract_msg exposes for a plain Message object are covered;
 * calendar/contact/task/signed-message specific properties, named
 * properties, and RTF (de)compression are intentionally out of scope. */
#define PR_SUBJECT              0x0037
#define PR_BODY                 0x1000
#define PR_HTML                 0x1013
#define PR_SENDER_NAME          0x0C1A
#define PR_SENDER_EMAIL_ADDRESS 0x0C1F
#define PR_SENDER_SMTP_ADDRESS  0x5D01
#define PR_CLIENT_SUBMIT_TIME   0x0039
#define PR_MESSAGE_DELIVERY_TIME 0x0E06
#define PR_MESSAGE_CODEPAGE     0x3FFD
#define PR_RECIPIENT_TYPE       0x0C15
#define PR_DISPLAY_NAME         0x3001
#define PR_EMAIL_ADDRESS        0x3003
#define PR_SMTP_ADDRESS         0x39FE
#define PR_ATTACH_LONG_FILENAME 0x3707
#define PR_ATTACH_FILENAME      0x3704
#define PR_ATTACH_MIME_TAG      0x370E
#define PR_ATTACH_DATA_BIN      0x3701

#define PROPERTIES_STREAM_NAME "__properties_version1.0"

struct msg_file {
    char *subject;
    char *body;
    char *html_body;

    char *sender_name;
    char *sender_email;

    int has_date;
    time_t date;

    msg_recipient_t *recipients;
    size_t recipient_count;

    msg_attachment_t *attachments;
    size_t attachment_count;
};

static char *synthesize_html_from_body(const char *body) {
    /* Same fallback extract_msg uses when there's no stored HTML part:
     * escape the plain text and wrap it. */
    size_t len = strlen(body);
    char *out = (char *)malloc(len * 6 + 64); /* worst case: every char becomes "&amp;" */
    if (!out) return NULL;

    size_t pos = 0;
    memcpy(out + pos, "<html><body>", 12);
    pos += 12;

    for (size_t i = 0; i < len; i++) {
        char c = body[i];
        if (c == '&') { memcpy(out + pos, "&amp;", 5); pos += 5; }
        else if (c == '<') { memcpy(out + pos, "&lt;", 4); pos += 4; }
        else if (c == '>') { memcpy(out + pos, "&gt;", 4); pos += 4; }
        else if (c == '\r') { /* skip; \n below emits the line break */ }
        else if (c == '\n') { memcpy(out + pos, "<br />", 6); pos += 6; }
        else out[pos++] = c;
    }

    memcpy(out + pos, "</body></html>", 15);
    pos += 15;
    out[pos] = '\0';
    return out;
}

static int read_prop_store(const cfb_t *cfb, uint32_t parent_id, uint32_t header_skip,
                            msg_prop_store_t *out) {
    uint32_t stream_id = cfb_find_child(cfb, parent_id, PROPERTIES_STREAM_NAME);
    if (stream_id == CFB_NO_ENTRY) {
        out->props = NULL;
        out->count = 0;
        return -1;
    }
    size_t size = 0;
    uint8_t *data = cfb_read_stream(cfb, stream_id, &size);
    if (!data) {
        out->props = NULL;
        out->count = 0;
        return -1;
    }
    int rc = msg_prop_store_parse(data, size, header_skip, out);
    free(data);
    return rc;
}

static char *get_string_with_fallback(const cfb_t *cfb, uint32_t parent_id,
                                       uint16_t primary, uint16_t fallback,
                                       uint32_t codepage) {
    char *value = msg_get_string_prop(cfb, parent_id, primary, codepage);
    if (value) return value;
    if (fallback) return msg_get_string_prop(cfb, parent_id, fallback, codepage);
    return NULL;
}

static void free_recipients(msg_recipient_t *recipients, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(recipients[i].name);
        free(recipients[i].email);
    }
    free(recipients);
}

static void free_attachments(msg_attachment_t *attachments, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(attachments[i].filename);
        free(attachments[i].mimetype);
        free(attachments[i].data);
    }
    free(attachments);
}

static int load_recipients(msg_file_t *msg, const cfb_t *cfb, uint32_t codepage) {
    size_t count = 0;
    uint32_t *ids = cfb_find_children_prefix(cfb, CFB_ROOT_ENTRY_ID, "__recip", &count);
    if (count == 0) {
        free(ids);
        return 0;
    }

    msg_recipient_t *recipients = (msg_recipient_t *)calloc(count, sizeof(msg_recipient_t));
    if (!recipients) { free(ids); return -1; }

    for (size_t i = 0; i < count; i++) {
        uint32_t recip_id = ids[i];

        msg_prop_store_t props;
        read_prop_store(cfb, recip_id, MSG_PROPSTORE_SKIP_OTHER, &props);

        int32_t type_flags = 0;
        msg_prop_get_i32(&props, PR_RECIPIENT_TYPE, &type_flags);
        msg_prop_store_free(&props);

        int type = type_flags & 0xF;
        recipients[i].type = (type >= MSG_RECIPIENT_TO && type <= MSG_RECIPIENT_BCC)
                                  ? (msg_recipient_type_t)type
                                  : MSG_RECIPIENT_UNKNOWN;
        recipients[i].name = msg_get_string_prop(cfb, recip_id, PR_DISPLAY_NAME, codepage);
        recipients[i].email = get_string_with_fallback(cfb, recip_id, PR_SMTP_ADDRESS,
                                                         PR_EMAIL_ADDRESS, codepage);
    }

    free(ids);
    msg->recipients = recipients;
    msg->recipient_count = count;
    return 0;
}

static int load_attachments(msg_file_t *msg, const cfb_t *cfb, uint32_t codepage) {
    size_t count = 0;
    uint32_t *ids = cfb_find_children_prefix(cfb, CFB_ROOT_ENTRY_ID, "__attach", &count);
    if (count == 0) {
        free(ids);
        return 0;
    }

    msg_attachment_t *attachments = (msg_attachment_t *)calloc(count, sizeof(msg_attachment_t));
    if (!attachments) { free(ids); return -1; }

    for (size_t i = 0; i < count; i++) {
        uint32_t att_id = ids[i];
        attachments[i].filename = get_string_with_fallback(cfb, att_id, PR_ATTACH_LONG_FILENAME,
                                                             PR_ATTACH_FILENAME, codepage);
        attachments[i].mimetype = msg_get_string_prop(cfb, att_id, PR_ATTACH_MIME_TAG, codepage);
        attachments[i].data = msg_get_binary_prop(cfb, att_id, PR_ATTACH_DATA_BIN, &attachments[i].size);
    }

    free(ids);
    msg->attachments = attachments;
    msg->attachment_count = count;
    return 0;
}

msg_file_t *msg_open(const char *path, msg_error_t *err_out) {
    msg_error_t err = MSG_OK;
    cfb_t *cfb = NULL;

    int rc = cfb_open(path, &cfb);
    if (rc != CFB_OK) {
        switch (rc) {
            case CFB_ERR_NOT_CFB: err = MSG_ERR_NOT_CFB; break;
            case CFB_ERR_MEMORY:  err = MSG_ERR_MEMORY; break;
            case CFB_ERR_CORRUPT: err = MSG_ERR_CORRUPT; break;
            default:              err = MSG_ERR_OPEN; break;
        }
        if (err_out) *err_out = err;
        return NULL;
    }

    msg_prop_store_t top_props;
    if (read_prop_store(cfb, CFB_ROOT_ENTRY_ID, MSG_PROPSTORE_SKIP_TOPLEVEL, &top_props) != 0) {
        cfb_close(cfb);
        if (err_out) *err_out = MSG_ERR_NO_PROPERTIES;
        return NULL;
    }

    msg_file_t *msg = (msg_file_t *)calloc(1, sizeof(msg_file_t));
    if (!msg) {
        msg_prop_store_free(&top_props);
        cfb_close(cfb);
        if (err_out) *err_out = MSG_ERR_MEMORY;
        return NULL;
    }

    int32_t codepage_signed = 0;
    uint32_t codepage = msg_prop_get_i32(&top_props, PR_MESSAGE_CODEPAGE, &codepage_signed) == 0
                             ? (uint32_t)codepage_signed
                             : 0;

    msg->subject = msg_get_string_prop(cfb, CFB_ROOT_ENTRY_ID, PR_SUBJECT, codepage);
    msg->body = msg_get_string_prop(cfb, CFB_ROOT_ENTRY_ID, PR_BODY, codepage);

    size_t html_size = 0;
    uint8_t *html_raw = msg_get_binary_prop(cfb, CFB_ROOT_ENTRY_ID, PR_HTML, &html_size);
    if (html_raw) {
        char *html = (char *)malloc(html_size + 1);
        if (html) {
            memcpy(html, html_raw, html_size);
            html[html_size] = '\0';
        }
        free(html_raw);
        msg->html_body = html;
    } else if (msg->body) {
        msg->html_body = synthesize_html_from_body(msg->body);
    }

    msg->sender_name = msg_get_string_prop(cfb, CFB_ROOT_ENTRY_ID, PR_SENDER_NAME, codepage);
    msg->sender_email = get_string_with_fallback(cfb, CFB_ROOT_ENTRY_ID, PR_SENDER_SMTP_ADDRESS,
                                                  PR_SENDER_EMAIL_ADDRESS, codepage);

    if (msg_prop_get_time(&top_props, PR_CLIENT_SUBMIT_TIME, &msg->date) == 0) {
        msg->has_date = 1;
    } else if (msg_prop_get_time(&top_props, PR_MESSAGE_DELIVERY_TIME, &msg->date) == 0) {
        msg->has_date = 1;
    }

    msg_prop_store_free(&top_props);

    load_recipients(msg, cfb, codepage);
    load_attachments(msg, cfb, codepage);

    cfb_close(cfb);

    if (err_out) *err_out = MSG_OK;
    return msg;
}

void msg_close(msg_file_t *msg) {
    if (!msg) return;
    free(msg->subject);
    free(msg->body);
    free(msg->html_body);
    free(msg->sender_name);
    free(msg->sender_email);
    free_recipients(msg->recipients, msg->recipient_count);
    free_attachments(msg->attachments, msg->attachment_count);
    free(msg);
}

const char *msg_get_subject(const msg_file_t *msg) { return msg->subject; }
const char *msg_get_body(const msg_file_t *msg) { return msg->body; }
const char *msg_get_html_body(const msg_file_t *msg) { return msg->html_body; }
const char *msg_get_sender_name(const msg_file_t *msg) { return msg->sender_name; }
const char *msg_get_sender_email(const msg_file_t *msg) { return msg->sender_email; }

int msg_get_date(const msg_file_t *msg, time_t *out) {
    if (!msg->has_date) return -1;
    *out = msg->date;
    return 0;
}

size_t msg_get_recipient_count(const msg_file_t *msg) { return msg->recipient_count; }

const msg_recipient_t *msg_get_recipient(const msg_file_t *msg, size_t index) {
    if (index >= msg->recipient_count) return NULL;
    return &msg->recipients[index];
}

size_t msg_get_attachment_count(const msg_file_t *msg) { return msg->attachment_count; }

const msg_attachment_t *msg_get_attachment(const msg_file_t *msg, size_t index) {
    if (index >= msg->attachment_count) return NULL;
    return &msg->attachments[index];
}

const char *msg_error_string(msg_error_t err) {
    switch (err) {
        case MSG_OK: return "no error";
        case MSG_ERR_OPEN: return "could not open or read file";
        case MSG_ERR_NOT_CFB: return "not a Compound File Binary (.msg) container";
        case MSG_ERR_CORRUPT: return "container structures are malformed";
        case MSG_ERR_NO_PROPERTIES: return "missing top-level property stream";
        case MSG_ERR_MEMORY: return "memory allocation failure";
        default: return "unknown error";
    }
}
