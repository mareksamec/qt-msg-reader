#ifndef MSGPARSER_H
#define MSGPARSER_H

#include "EmailTypes.h"

/**
 * Parser for Microsoft Outlook MSG files, backed by libmsg (a pure C,
 * dependency-free reader for the MS-CFB/MS-OXMSG format).
 */
class MsgParser {
public:
    /** Parses an MSG file and returns the email message data. */
    EmailMessage parse(const QString& filePath);
};

#endif
