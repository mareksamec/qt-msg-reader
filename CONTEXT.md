# Qt MSG Reader - Context

## Project Overview
Qt MSG Reader is a desktop application for viewing Microsoft Outlook MSG files. It's built with:
- **C++/Qt6** for the GUI frontend
- **libmsg** (`libmsg/`), an in-tree pure-C library, for parsing MSG files
- **CMake** build system

Earlier versions shelled out to Python's `extract_msg` library via the Python
C API (see "History" below). That dependency has been removed entirely -
`.msg` parsing is now done in-process by `libmsg/`, which has no runtime
dependency beyond libc. See `libmsg/README.md` for what it covers and what
it deliberately leaves out.

## Architecture

### Files Structure
```
qt-msg-reader/
├── src/
│   ├── main.cpp           # Application entry point
│   ├── MainWindow.h/cpp   # Main window UI with file browser, message view, attachments, status log
│   ├── MsgParser.h/cpp    # Thin wrapper around libmsg for MSG parsing
│   ├── EmailTypes.h       # Data structures (EmailMessage, EmailAttachment)
│   ├── MsgFileModel.h/cpp # File system model filtered for .msg files
│   └── AttachmentModel.h/cpp # Table model for attachments display
├── libmsg/                # Pure-C .msg parsing library (own README, tests, CLI demo)
├── build/                 # Build output
├── PKGBUILD               # Arch Linux package build
├── CMakeLists.txt         # Build configuration
├── README.md              # User documentation
└── CONTEXT.md             # This file - development context
```

### Key Components

1. **MsgParser** - Wraps `libmsg` and maps its output onto `EmailMessage`/`EmailAttachment`
   - `msg_open()` / `msg_close()` bracket a single parse call; no persistent state
   - Recipients come from `msg_get_recipient()`, bucketed into to/cc/bcc by `msg_recipient_type_t`
   - HTML body falls back automatically if the file has none: libmsg first tries
     PR_RTF_COMPRESSED (decompressing it and recovering the original HTML if it's
     "\fromhtml1"-encapsulated), then as a last resort synthesizes HTML from the
     plain body

2. **MainWindow** - Main application window
   - File browser (QTreeView + MsgFileModel) - filtered to show only .msg files
   - Message header display (subject, from, to, cc, date)
   - Body viewer: a tabbed QTextEdit pair (HTML tab, default; Plain Text tab),
     with inline `cid:` images resolved to data: URIs before rendering the HTML tab
   - Attachments table (QTableView + AttachmentModel)
   - Status log (QTextEdit with timestamped entries)

3. **EmailMessage** struct contains:
   - subject, bodyPlainText, bodyHtml
   - senderName, senderEmail
   - toRecipients, ccRecipients, bccRecipients
   - date (QDateTime)
   - attachments (QList<EmailAttachment>)

## libmsg API Notes

```c
msg_error_t err;
msg_file_t *msg = msg_open(path, &err);   // NULL on failure

msg_get_subject(msg);          // const char* (UTF-8), or NULL
msg_get_body(msg);             // plain text, or NULL
msg_get_html_body(msg);        // HTML, synthesized from plain body if absent
msg_get_sender_name(msg);
msg_get_sender_email(msg);
msg_get_date(msg, &time_t_out);           // 0 on success, -1 if absent

msg_get_recipient_count(msg);
msg_get_recipient(msg, i);     // ->name, ->email, ->type (TO/CC/BCC)

msg_get_attachment_count(msg);
msg_get_attachment(msg, i);    // ->filename, ->mimetype, ->content_id, ->data, ->size

msg_close(msg);                 // frees everything the accessors returned
```

Full scope/limitations are documented in `libmsg/README.md` - notably RTF
support recovers text/HTML but not RTF's own formatting (fonts/colors/tables),
and there's still no support for named properties, embedded-message
attachments, or non-Message item types.

## Build & Run

```bash
cd build
cmake ..
make -j$(nproc)
./qt-msg-reader [file.msg]
```

## GitHub Actions CI

The workflow (`.github/workflows/cmake-multi-platform.yml`) builds for:
- **ubuntu-latest** (GCC)
- **windows-latest** (MSVC)

Key steps:
1. Installs Qt6 via `jurplel/install-qt-action`
2. Builds the application (CMake pulls in `libmsg/` as a subdirectory)
3. Uploads artifacts (single self-contained executable per platform)

### Manual Releases

To create a release:
1. Go to Actions → Build workflow
2. Click "Run workflow"
3. Enter version (e.g., `v1.0.0`)
4. The workflow will create a GitHub Release with:
   - Linux binary (`qt-msg-reader-linux-x86_64.tar.gz`)
   - Windows binary (`qt-msg-reader-windows-x86_64.zip`)
   - Source tarball (`source.tar.gz`)

## Arch Linux Package

A `PKGBUILD` file is provided for Arch Linux users. It only depends on
`qt6-base` - no Python or vendored packages to install.

To install on Arch Linux:
```bash
makepkg -si
```

## History: the Python bridge (removed)

Earlier versions used Python's `extract_msg` library via the Python C API,
loaded through a private `python-packages/` install to avoid clashing with
user-installed packages. That whole layer - `Py_InitializeEx` setup, GIL
management, `PyErr_Clear()` after every fallible call, `findSitePackages()`,
the PKGBUILD's vendored PyPI sources, and the CI steps that bundled a Python
interpreter next to the executable - is gone. It's mentioned here only so
old commit history and PR discussions make sense; none of it applies to the
current codebase.

## Recent Changes
- RTF fallback: libmsg now decompresses PR_RTF_COMPRESSED (MS-OXRTFCP) and, when
  it isn't already present as PR_HTML, recovers the real HTML from RTF that
  encapsulates it (MS-OXRTFEX `\fromhtml1`/`\htmltag`/`\htmlrtf`), or otherwise
  its plain text - fixing messages composed in Outlook's Rich Text format that
  previously showed no body at all, or an unformatted plain-text-only body.
  See `libmsg/src/msg_rtf.c`.
- Message Body is now a two-tab view (HTML tab, default; Plain Text tab) instead
  of a single widget that only showed one or the other.
- Inline images: HTML bodies referencing attachments via `cid:` (PR_ATTACH_CONTENT_ID)
  now render inline in the body viewer, resolved to `data:` URIs instead of only
  appearing in the attachments list. See `MainWindow::resolveInlineImages()`.
- Removed the Python/`extract_msg` dependency entirely; `MsgParser` now wraps
  `libmsg`, an in-tree pure-C library with no runtime dependencies
- Added `libmsg/`: a from-scratch MS-CFB + MS-OXMSG reader, fuzz-tested under
  ASan/UBSan
- Simplified CMakeLists.txt, PKGBUILD, and CI workflow accordingly (no more
  Python setup, package bundling, or site-packages path juggling)
- Added PKGBUILD for Arch Linux packaging
- Added manual release workflow with version input
- Releases include Linux/Windows binaries + source tarball
- Added GitHub Actions CI workflow for Linux and Windows
- Added status log window with timestamped entries
- Log shows: file loading, subject, body type/size, attachments
- Warnings (orange) and errors (red) highlighted
- Made header labels bold in message view
