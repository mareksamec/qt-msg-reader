#define QT_NO_KEYWORDS
#include <Python.h>
#undef QT_NO_KEYWORDS

#include "MsgParser.h"
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <QRegularExpression>

bool MsgParser::s_pythonInitialized = false;
bool MsgParser::s_moduleLoaded = false;
void* MsgParser::s_msgModule = nullptr;

MsgParser::MsgParser() {
    initPython();
}

MsgParser::~MsgParser() {
}

bool MsgParser::initPython() {
    if (s_pythonInitialized) {
        return s_moduleLoaded;
    }
    
    QString venvPath = QString(PYTHON_VENV_PATH);
    QString sitePackages = QString("%1/lib/python3.14/site-packages").arg(venvPath);
    
    if (!Py_IsInitialized()) {
        Py_InitializeEx(0);
        if (!Py_IsInitialized()) {
            qWarning() << "Failed to initialize Python";
            return false;
        }
        s_pythonInitialized = true;
    }
    
    PyObject* sysModule = PyImport_ImportModule("sys");
    if (sysModule) {
        PyObject* pathObj = PyObject_GetAttrString(sysModule, "path");
        if (pathObj && PyList_Check(pathObj)) {
            PyObject* sitePath = PyUnicode_FromString(sitePackages.toUtf8().constData());
            PyList_Insert(pathObj, 0, sitePath);
            Py_DECREF(sitePath);
        }
        Py_XDECREF(pathObj);
        Py_DECREF(sysModule);
    }
    
    PyObject* msgModule = PyImport_ImportModule("extract_msg");
    if (!msgModule) {
        if (PyErr_Occurred()) {
            PyErr_Print();
        }
        qWarning() << "Failed to import extract_msg module from path:" << sitePackages;
        return false;
    }
    
    s_msgModule = msgModule;
    s_moduleLoaded = true;
    return true;
}

QString MsgParser::pyObjectToString(void* obj) {
    PyObject* pyObj = static_cast<PyObject*>(obj);
    if (!pyObj || pyObj == Py_None) return QString();
    
    PyObject* strObj = PyObject_Str(pyObj);
    if (!strObj) {
        PyErr_Clear();
        return QString();
    }
    
    Py_ssize_t size;
    const char* str = PyUnicode_AsUTF8AndSize(strObj, &size);
    QString result = QString::fromUtf8(str, size);
    Py_DECREF(strObj);
    
    return result;
}

QByteArray MsgParser::pyObjectToBytes(void* obj) {
    PyObject* pyObj = static_cast<PyObject*>(obj);
    if (!pyObj || pyObj == Py_None) return QByteArray();
    
    if (PyBytes_Check(pyObj)) {
        char* buffer;
        Py_ssize_t size;
        PyBytes_AsStringAndSize(pyObj, &buffer, &size);
        return QByteArray(buffer, size);
    }
    
    PyObject* bytesObj = PyObject_Bytes(pyObj);
    if (!bytesObj) {
        PyErr_Clear();
        return QByteArray();
    }
    
    char* buffer;
    Py_ssize_t size;
    PyBytes_AsStringAndSize(bytesObj, &buffer, &size);
    QByteArray result(buffer, size);
    Py_DECREF(bytesObj);
    
    return result;
}

QDateTime MsgParser::pyObjectToDateTime(void* obj) {
    PyObject* pyObj = static_cast<PyObject*>(obj);
    if (!pyObj || pyObj == Py_None) return QDateTime();
    
    PyObject* timestampMethod = PyObject_GetAttrString(pyObj, "timestamp");
    if (timestampMethod) {
        PyObject* timestampObj = PyObject_CallObject(timestampMethod, nullptr);
        Py_DECREF(timestampMethod);
        
        if (timestampObj && PyFloat_Check(timestampObj)) {
            double timestamp = PyFloat_AsDouble(timestampObj);
            Py_DECREF(timestampObj);
            return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(timestamp * 1000));
        }
        Py_XDECREF(timestampObj);
    }
    PyErr_Clear();
    
    QString dateStr = pyObjectToString(obj);
    if (!dateStr.isEmpty()) {
        QDateTime dt = QDateTime::fromString(dateStr, Qt::ISODate);
        if (dt.isValid()) return dt;
    }
    
    return QDateTime();
}

EmailMessage MsgParser::parse(const QString& filePath) {
    EmailMessage msg;
    
    if (!s_moduleLoaded) {
        msg.errorMessage = "Python extract_msg module not loaded";
        return msg;
    }
    
    PyGILState_STATE gstate = PyGILState_Ensure();
    
    PyObject* openFunc = PyObject_GetAttrString(static_cast<PyObject*>(s_msgModule), "Message");
    if (!openFunc) {
        PyErr_Print();
        msg.errorMessage = "Failed to get Message class from extract_msg";
        PyGILState_Release(gstate);
        return msg;
    }
    
    PyObject* filePathPy = PyUnicode_FromString(filePath.toUtf8().constData());
    PyObject* args = PyTuple_Pack(1, filePathPy);
    
    PyObject* msgObj = PyObject_CallObject(openFunc, args);
    Py_DECREF(args);
    Py_DECREF(filePathPy);
    Py_DECREF(openFunc);
    
    if (!msgObj) {
        PyErr_Print();
        msg.errorMessage = "Failed to open MSG file: " + filePath;
        PyGILState_Release(gstate);
        return msg;
    }
    
    msg.isValid = true;
    
    PyObject* subjectObj = PyObject_GetAttrString(msgObj, "subject");
    msg.subject = pyObjectToString(subjectObj);
    Py_XDECREF(subjectObj);
    
    PyObject* bodyObj = PyObject_GetAttrString(msgObj, "body");
    msg.bodyPlainText = pyObjectToString(bodyObj);
    Py_XDECREF(bodyObj);
    
    PyObject* htmlBodyObj = PyObject_GetAttrString(msgObj, "htmlBody");
    if (htmlBodyObj && htmlBodyObj != Py_None) {
        QByteArray htmlBytes = pyObjectToBytes(htmlBodyObj);
        if (!htmlBytes.isEmpty()) {
            msg.bodyHtml = QString::fromUtf8(htmlBytes);
        }
    }
    Py_XDECREF(htmlBodyObj);
    PyErr_Clear();
    
    PyObject* senderObj = PyObject_GetAttrString(msgObj, "sender");
    msg.senderName = pyObjectToString(senderObj);
    Py_XDECREF(senderObj);
    PyErr_Clear();
    
    if (!msg.senderName.isEmpty()) {
        QRegularExpression emailRe(R"((?:<|^)([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})(?:>|$))");
        QRegularExpressionMatch match = emailRe.match(msg.senderName);
        if (match.hasMatch()) {
            msg.senderEmail = match.captured(1);
            QString name = msg.senderName;
            name.remove(QRegularExpression(R"(\s*<[^>]+>\s*)"));
            name = name.trimmed();
            if (!name.isEmpty()) {
                msg.senderName = name;
            }
        }
    }
    
    PyObject* dateObj = PyObject_GetAttrString(msgObj, "date");
    msg.date = pyObjectToDateTime(dateObj);
    Py_XDECREF(dateObj);
    
    PyObject* recipientsObj = PyObject_GetAttrString(msgObj, "recipients");
    if (recipientsObj && PyList_Check(recipientsObj)) {
        Py_ssize_t len = PyList_Size(recipientsObj);
        for (Py_ssize_t i = 0; i < len; ++i) {
            PyObject* recip = PyList_GetItem(recipientsObj, i);
            
            PyObject* typeObj = PyObject_GetAttrString(recip, "type");
            int recipType = 0;
            if (typeObj && PyLong_Check(typeObj)) {
                recipType = PyLong_AsLong(typeObj);
            }
            Py_XDECREF(typeObj);
            PyErr_Clear();
            
            PyObject* emailObj = PyObject_GetAttrString(recip, "email");
            QString email = pyObjectToString(emailObj);
            Py_XDECREF(emailObj);
            PyErr_Clear();
            
            if (!email.isEmpty()) {
                if (recipType == 1) {
                    if (!msg.toRecipients.isEmpty()) msg.toRecipients += ", ";
                    msg.toRecipients += email;
                } else if (recipType == 2) {
                    if (!msg.ccRecipients.isEmpty()) msg.ccRecipients += ", ";
                    msg.ccRecipients += email;
                }
            }
        }
    }
    Py_XDECREF(recipientsObj);
    PyErr_Clear();
    
    if (msg.toRecipients.isEmpty()) {
        PyObject* toObj = PyObject_GetAttrString(msgObj, "to");
        if (toObj && toObj != Py_None) {
            QString toStr = pyObjectToString(toObj);
            toStr.remove(QRegularExpression(R"(<|>)"));
            msg.toRecipients = toStr;
        }
        Py_XDECREF(toObj);
        PyErr_Clear();
    }
    
    if (msg.ccRecipients.isEmpty()) {
        PyObject* ccObj = PyObject_GetAttrString(msgObj, "cc");
        if (ccObj && ccObj != Py_None) {
            QString ccStr = pyObjectToString(ccObj);
            ccStr.remove(QRegularExpression(R"(<|>)"));
            msg.ccRecipients = ccStr;
        }
        Py_XDECREF(ccObj);
        PyErr_Clear();
    }
    
    PyObject* attachmentsObj = PyObject_GetAttrString(msgObj, "attachments");
    if (attachmentsObj && PyList_Check(attachmentsObj)) {
        Py_ssize_t len = PyList_Size(attachmentsObj);
        for (Py_ssize_t i = 0; i < len; ++i) {
            PyObject* value = PyList_GetItem(attachmentsObj, i);
            EmailAttachment att;
            
            PyObject* filenameObj = PyObject_GetAttrString(value, "longFilename");
            if (!filenameObj || filenameObj == Py_None) {
                Py_XDECREF(filenameObj);
                filenameObj = PyObject_GetAttrString(value, "shortFilename");
            }
            if (!filenameObj || filenameObj == Py_None) {
                Py_XDECREF(filenameObj);
                filenameObj = PyObject_GetAttrString(value, "name");
            }
            att.filename = pyObjectToString(filenameObj);
            Py_XDECREF(filenameObj);
            PyErr_Clear();
            
            if (att.filename.isEmpty()) {
                att.filename = QString("attachment_%1").arg(msg.attachments.size() + 1);
            }
            
            PyObject* mimeObj = PyObject_GetAttrString(value, "mimetype");
            att.mimeType = pyObjectToString(mimeObj);
            Py_XDECREF(mimeObj);
            PyErr_Clear();
            
            PyObject* dataMethod = PyObject_GetAttrString(value, "data");
            if (dataMethod && PyCallable_Check(dataMethod)) {
                PyObject* dataObj = PyObject_CallObject(dataMethod, nullptr);
                if (dataObj) {
                    att.data = pyObjectToBytes(dataObj);
                    att.size = att.data.size();
                    Py_DECREF(dataObj);
                }
            }
            Py_XDECREF(dataMethod);
            PyErr_Clear();
            
            if (att.data.isEmpty()) {
                PyObject* dataObj = PyObject_GetAttrString(value, "data");
                if (dataObj && dataObj != Py_None) {
                    att.data = pyObjectToBytes(dataObj);
                    att.size = att.data.size();
                }
                Py_XDECREF(dataObj);
                PyErr_Clear();
            }
            
            msg.attachments.append(att);
        }
    }
    Py_XDECREF(attachmentsObj);
    PyErr_Clear();
    
    PyObject* closeMethod = PyObject_GetAttrString(msgObj, "close");
    if (closeMethod && PyCallable_Check(closeMethod)) {
        PyObject_CallObject(closeMethod, nullptr);
    }
    Py_XDECREF(closeMethod);
    
    Py_DECREF(msgObj);
    PyGILState_Release(gstate);
    
    return msg;
}
