#ifndef MSGPARSER_H
#define MSGPARSER_H

#include "EmailTypes.h"

class MsgParser {
public:
    MsgParser();
    ~MsgParser();
    
    EmailMessage parse(const QString& filePath);
    
private:
    bool initPython();
    QString pyObjectToString(void* obj);
    QByteArray pyObjectToBytes(void* obj);
    QDateTime pyObjectToDateTime(void* obj);
    
    static bool s_pythonInitialized;
    static bool s_moduleLoaded;
    static void* s_msgModule;
};

#endif
