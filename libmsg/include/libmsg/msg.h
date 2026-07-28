/*
 * libmsg - a small, dependency-free C library for reading the fields most
 * applications need out of Microsoft Outlook .msg files (MS-CFB container +
 * MS-OXMSG property layout).
 *
 * This is not a full reimplementation of the format (no RTF decompression,
 * no named properties, no embedded-message attachments, no signed/S-MIME
 * messages, no write support). It covers the common case: subject, plain
 * and HTML body, sender, date, recipients and attachments.
 */
#ifndef LIBMSG_H
#define LIBMSG_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msg_file msg_file_t;

typedef enum {
    MSG_OK = 0,
    MSG_ERR_OPEN,        /* could not open/read the file */
    MSG_ERR_NOT_CFB,     /* not a Compound File Binary (OLE2) container */
    MSG_ERR_CORRUPT,     /* container or property structures are malformed */
    MSG_ERR_NO_PROPERTIES, /* file has no top-level property stream */
    MSG_ERR_MEMORY       /* allocation failure */
} msg_error_t;

typedef enum {
    MSG_RECIPIENT_UNKNOWN = 0,
    MSG_RECIPIENT_TO = 1,
    MSG_RECIPIENT_CC = 2,
    MSG_RECIPIENT_BCC = 3
} msg_recipient_type_t;

typedef struct {
    char *name;                    /* display name, UTF-8; may be NULL */
    char *email;                   /* email/SMTP address, UTF-8; may be NULL */
    msg_recipient_type_t type;
} msg_recipient_t;

typedef struct {
    char *filename;                /* best available filename, UTF-8; may be NULL */
    char *mimetype;                /* UTF-8; may be NULL if not stored */
    unsigned char *data;           /* attachment bytes; may be NULL */
    size_t size;                   /* length of data in bytes */
} msg_attachment_t;

/* Opens a .msg file and parses its core fields. Returns NULL on failure and,
 * if err_out is non-NULL, stores the reason in *err_out. */
msg_file_t *msg_open(const char *path, msg_error_t *err_out);

/* Frees everything returned by the msg_get_* accessors below. */
void msg_close(msg_file_t *msg);

const char *msg_get_subject(const msg_file_t *msg);
const char *msg_get_body(const msg_file_t *msg);

/* HTML body. If the file has no stored HTML part, one is synthesized from
 * the plain text body (mirrors extract_msg's behavior), so this only
 * returns NULL when there is no body at all. */
const char *msg_get_html_body(const msg_file_t *msg);

const char *msg_get_sender_name(const msg_file_t *msg);
const char *msg_get_sender_email(const msg_file_t *msg);

/* Send/delivery date. Returns 0 and fills *out on success, -1 if the
 * message has no usable date property. */
int msg_get_date(const msg_file_t *msg, time_t *out);

size_t msg_get_recipient_count(const msg_file_t *msg);
const msg_recipient_t *msg_get_recipient(const msg_file_t *msg, size_t index);

size_t msg_get_attachment_count(const msg_file_t *msg);
const msg_attachment_t *msg_get_attachment(const msg_file_t *msg, size_t index);

/* Human-readable description of an msg_error_t value. */
const char *msg_error_string(msg_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* LIBMSG_H */
