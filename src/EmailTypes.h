#ifndef EMAILTYPES_H
#define EMAILTYPES_H

#include <QString>
#include <QByteArray>
#include <QDateTime>
#include <QList>

struct EmailAttachment {
    QString filename;
    QString mimeType;
    QByteArray data;
    qint64 size;
};

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
