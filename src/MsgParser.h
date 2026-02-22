#ifndef MSGPARSER_H
#define MSGPARSER_H

#include "EmailTypes.h"

/**
 * Parser for Microsoft Outlook MSG files using Python's extract_msg library.
 * Bridges C++ with Python via the Python C API.
 */
class MsgParser {
public:
    MsgParser();
    ~MsgParser();
    
    /** Parses an MSG file and returns the email message data. */
    EmailMessage parse(const QString& filePath);
    
private:
    /** Finds the Python site-packages directory (bundled or venv). */
    QString findSitePackages();
    /** Initializes Python interpreter and loads extract_msg module. */
    bool initPython();
    /** Converts a Python object to QString. */
    QString pyObjectToString(void* obj);
    /** Converts a Python object to QByteArray (for binary data). */
    QByteArray pyObjectToBytes(void* obj);
    /** Converts a Python datetime object to QDateTime. */
    QDateTime pyObjectToDateTime(void* obj);
    
    static bool s_pythonInitialized;
    static bool s_moduleLoaded;
    static void* s_msgModule;
};

#endif
