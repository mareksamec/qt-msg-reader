#ifndef EMAILTYPES_H
#define EMAILTYPES_H

#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QList>

/**
 * Represents a single email attachment with its metadata and binary content.
 */
struct EmailAttachment {
    QString filename;
    QString mimeType;
    QByteArray data;
    qint64 size;
};

/**
 * Represents a parsed email message with all its properties.
 * isValid indicates whether parsing was successful; errorMessage contains error details if not.
 */
struct EmailMessage {
    QString subject;
    QString bodyPlainText;
    QString bodyHtml;
    QString senderName;
    QString senderEmail;
    QString toRecipients;
    QString ccRecipients;
    QString bccRecipients;
    QDateTime date;
    QList<EmailAttachment> attachments;
    bool isValid = false;
    QString errorMessage;
};

#endif
