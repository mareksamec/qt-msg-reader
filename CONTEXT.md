# Qt MSG Reader - Context

## Project Overview
Qt MSG Reader is a desktop application for viewing Microsoft Outlook MSG files. It's built with:
- **C++/Qt6** for the GUI frontend
- **Python extract_msg** library for parsing MSG files
- **CMake** build system

## Architecture

### Files Structure
```
qt-msg-reader/
├── src/
│   ├── main.cpp           # Application entry point
│   ├── MainWindow.h/cpp   # Main window UI with file browser, message view, attachments, status log
│   ├── MsgParser.h/cpp    # Python bridge for MSG parsing
│   ├── EmailTypes.h       # Data structures (EmailMessage, EmailAttachment)
│   ├── MsgFileModel.h/cpp # File system model filtered for .msg files
│   └── AttachmentModel.h/cpp # Table model for attachments display
├── .venv/                 # Python virtual environment with extract_msg
├── build/                 # Build output
├── CMakeLists.txt         # Build configuration
├── README.md              # User documentation
└── CONTEXT.md             # This file - development context
```

### Key Components

1. **MsgParser** - Bridge between C++ and Python
   - Uses Python C API to call extract_msg
   - Static module loading (s_pythonInitialized, s_moduleLoaded, s_msgModule)
   - Must use `Py_InitializeEx(0)` for simpler initialization
   - GIL management with `PyGILState_Ensure()`/`PyGILState_Release()`
   - Always call `PyErr_Clear()` after operations that may fail

2. **MainWindow** - Main application window
   - File browser (QTreeView + MsgFileModel) - filtered to show only .msg files
   - Message header display (subject, from, to, cc, date)
   - Body viewer (QTextEdit - supports HTML and plain text)
   - Attachments table (QTableView + AttachmentModel)
   - Status log (QTextEdit with timestamped entries)

3. **EmailMessage** struct contains:
   - subject, bodyPlainText, bodyHtml
   - senderName, senderEmail
   - toRecipients, ccRecipients
   - date (QDateTime)
   - attachments (QList<EmailAttachment>)

## Bug Fixes Applied

### 1. Python Initialization Crash
- **Problem**: App crashed when opening MSG files due to Python init failure
- **Solution**: Changed from `PyConfig_InitPythonConfig()` to simple `Py_InitializeEx(0)`
- **Key insight**: Don't set `config.home` - let Python use system defaults, then add venv site-packages to sys.path

### 2. Attachments Parsing
- **Problem**: Attachments treated as dict (old API), but extract_msg returns list
- **Solution**: Changed from `PyDict_Check()` iteration to `PyList_Check()` iteration

### 3. Sender Email Extraction
- **Problem**: Tried to use `msg.header.from.email` but header is raw string
- **Solution**: Extract email from `msg.sender` string using regex

### 4. Recipients Parsing
- **Problem**: `msg.to` and `msg.cc` are strings, not lists of objects
- **Solution**: Use `msg.recipients` list (Recipient objects with type, email, name)
  - type=1: TO recipient
  - type=2: CC recipient
- Fallback: Parse `msg.to`/`msg.cc` strings directly

### 5. HTML Body Display
- **Problem**: htmlBody returned as bytes, displayed as raw text
- **Solution**: Use `pyObjectToBytes()` then `QString::fromUtf8()`

### 6. Python Error Handling
- **Problem**: Uncleared Python exceptions caused cascading failures
- **Solution**: Add `PyErr_Clear()` after operations that may fail

## extract_msg API Notes

```python
msg = extract_msg.Message(filepath)

# Properties:
msg.subject      # str
msg.body         # str or None (plain text)
msg.htmlBody     # bytes or None
msg.sender       # str like '"Name" <email@example.com>'
msg.to           # str like '<email@example.com>'
msg.cc           # str or None
msg.date         # datetime
msg.recipients   # list of Recipient objects
msg.attachments  # list of Attachment objects

# Recipient object:
recipient.email  # str
recipient.name   # str
recipient.type   # int (1=TO, 2=CC)

# Attachment object:
att.longFilename  # str or None
att.shortFilename # str or None
att.name          # str
att.mimetype      # str or None
att.data          # bytes (call as method if callable)
```

## Build & Run

```bash
cd /home/marek/nosync-Trustsoft/personal/qt-msg-reader/build
cmake ..
make -j$(nproc)
./qt-msg-reader [file.msg]
```

## Python Packages

The application bundles Python packages for deployment:

**Development:** Uses `.venv/lib/python3.14/site-packages` from the source directory

**Deployment:** Bundles `python-packages/` directory next to the executable

The `MsgParser::findSitePackages()` method searches in this order:
1. `<exe_dir>/python-packages/` (bundled, for deployment)
2. `.venv/lib/python3.14/site-packages` (development)

CMake automatically copies packages from `.venv` to `build/python-packages/` during build.

## Recent Changes
- Bundled Python packages with the executable for deployment
- Added `findSitePackages()` to locate packages (bundled or venv)
- CMake copies site-packages to build directory
- Added comprehensive comments to all methods and key code sections
- Updated README.md with current project structure and usage
- Added status log window with timestamped entries
- Log shows: file loading, subject, body type/size, attachments
- Warnings (orange) and errors (red) highlighted
- Made header labels bold in message view
