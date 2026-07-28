#include "MsgParser.h"

#include <libmsg/msg.h>

EmailMessage MsgParser::parse(const QString& filePath) {
    EmailMessage msg;

    msg_error_t err;
    msg_file_t* file = msg_open(filePath.toUtf8().constData(), &err);
    if (!file) {
        msg.errorMessage = QString::fromUtf8(msg_error_string(err));
        return msg;
    }

    msg.isValid = true;

    if (const char* subject = msg_get_subject(file)) {
        msg.subject = QString::fromUtf8(subject);
    }
    if (const char* body = msg_get_body(file)) {
        msg.bodyPlainText = QString::fromUtf8(body);
    }
    if (const char* html = msg_get_html_body(file)) {
        msg.bodyHtml = QString::fromUtf8(html);
    }
    if (const char* senderName = msg_get_sender_name(file)) {
        msg.senderName = QString::fromUtf8(senderName);
    }
    if (const char* senderEmail = msg_get_sender_email(file)) {
        msg.senderEmail = QString::fromUtf8(senderEmail);
    }

    time_t date;
    if (msg_get_date(file, &date) == 0) {
        msg.date = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(date), Qt::UTC);
    }

    size_t recipientCount = msg_get_recipient_count(file);
    for (size_t i = 0; i < recipientCount; i++) {
        const msg_recipient_t* r = msg_get_recipient(file, i);
        if (!r || !r->email) continue;

        QString email = QString::fromUtf8(r->email);
        switch (r->type) {
            case MSG_RECIPIENT_TO:
                if (!msg.toRecipients.isEmpty()) msg.toRecipients += ", ";
                msg.toRecipients += email;
                break;
            case MSG_RECIPIENT_CC:
                if (!msg.ccRecipients.isEmpty()) msg.ccRecipients += ", ";
                msg.ccRecipients += email;
                break;
            case MSG_RECIPIENT_BCC:
                if (!msg.bccRecipients.isEmpty()) msg.bccRecipients += ", ";
                msg.bccRecipients += email;
                break;
            default:
                break;
        }
    }

    size_t attachmentCount = msg_get_attachment_count(file);
    for (size_t i = 0; i < attachmentCount; i++) {
        const msg_attachment_t* a = msg_get_attachment(file, i);
        if (!a) continue;

        EmailAttachment att;
        att.filename = a->filename ? QString::fromUtf8(a->filename)
                                    : QString("attachment_%1").arg(msg.attachments.size() + 1);
        att.mimeType = a->mimetype ? QString::fromUtf8(a->mimetype) : QString();
        if (a->data && a->size > 0) {
            att.data = QByteArray(reinterpret_cast<const char*>(a->data), static_cast<int>(a->size));
        }
        att.size = static_cast<qint64>(a->size);
        msg.attachments.append(att);
    }

    msg_close(file);
    return msg;
}
