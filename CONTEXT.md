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
├── packaging/arch/        # Arch Linux PKGBUILDs (release + git), own README.md
├── LICENSE                # MIT license text
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

The workflow (`.github/workflows/cmake-multi-platform.yml`) has four jobs:
- **prepare**: computes the `build` job's OS matrix (see "Windows is
  opt-in" below) - needed because a job's `if:` can't see `matrix.*`, so
  gating a single matrix leg has to happen by shaping the matrix itself
  instead.
- **build**: `ubuntu-latest` (GCC), plus `windows-latest` (MSVC) when
  requested. Installs Qt6 via `jurplel/install-qt-action`, builds (CMake
  pulls in `libmsg/` as a subdirectory), and uploads one self-contained
  executable archive per platform.
- **build-arch**: builds the Arch Linux package in an `archlinux:base-devel`
  container, on every push/PR, not just releases. See "Arch Linux Package"
  below.
- **release**: needs `build` + `build-arch`; see "Releases" below.

### Windows is opt-in

The Windows leg only runs on a manual `workflow_dispatch` with the
`build_windows` checkbox input ticked - plain pushes/PRs (and releases
derived from them) are Linux-only by default. This matches the project's own
TODO ("Remove Windows support - not needed as you can use Outlook on Win"),
short of removing it outright.

### Releases

The `release` job runs after every push to `main` (e.g. a PR merge) as well
as on manual `workflow_dispatch` - but not on `pull_request` builds. Either
way it's gated behind the `release` GitHub Environment's required-reviewer
approval, so nothing publishes until someone clicks "Review deployments" →
"Approve" on that job in the Actions UI, after confirming the build
succeeded. **This requires one-time setup**: create an Environment named
`release` under repo Settings → Environments, and add at least one required
reviewer - the `environment: release` key in the workflow does nothing by
itself until that environment has protection rules.

Version tagging:
- Manual `workflow_dispatch` run with a `version` input (e.g. `v1.2.0`) uses
  that tag.
- Any other run (i.e. a plain push to `main`) auto-derives a tag from
  `CMakeLists.txt`'s `project(... VERSION x.y.z ...)` plus the short commit
  SHA, e.g. `v1.0.0-abc1234`, since there's no version input to type on a push.

Either way the release includes:
- Linux binary (`qt-msg-reader-linux-x86_64.tar.gz`)
- Arch Linux package (`qt-msg-reader-<version>-<rel>-x86_64.pkg.tar.zst`)
- Windows binary (`qt-msg-reader-windows-x86_64.zip`), only if the `build`
  job's Windows leg actually ran (see above)
- Source tarball (`source.tar.gz`)

To release from a merge you approve of: go to the Actions run for that push,
find the pending `release` job, review it, and approve. To release
ad hoc with a specific version instead: Actions → Build workflow → "Run
workflow" → enter a version → approve the same way when it reaches the gate.

## Arch Linux Package

`packaging/arch/` holds two PKGBUILDs (own `README.md` there has the full
picture): `PKGBUILD` (versioned, `qt-msg-reader`, sourced from a GitHub
release tag) and `PKGBUILD-git` (`qt-msg-reader-git`, always builds latest
git HEAD). Both depend only on `qt6-base` - no Python or vendored packages.

The `build-arch` CI job builds both on every push/PR - the `-git` one
against the exact commit under test (via a `git+file://` override, since
there's no need to wait for it to land on GitHub), and the versioned one
using a locally generated source tarball instead of a real release tag
(which may not exist yet for an arbitrary commit). Only the versioned
package's `.pkg.tar.zst` is uploaded as a build artifact / attached to
releases; the `-git` build is a build-only smoke test of the packaging
recipe.

To install on Arch Linux:
```bash
cd packaging/arch
makepkg -si
```

Neither PKGBUILD is published to the AUR yet - see
`packaging/arch/README.md` for what that would take.

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
- CI: Windows is now opt-in (`build_windows` workflow_dispatch input) instead
  of building on every push/PR/release; added a `build-arch` job that builds
  an Arch Linux package on every push/PR and attaches it to releases. See
  `packaging/arch/` (moved out of the repo root, now with a `PKGBUILD-git`
  alongside the original `PKGBUILD`) and "GitHub Actions CI" above. Added the
  `LICENSE` file the PKGBUILD's `package()` installs but which didn't
  previously exist.
- MainWindow now persists window geometry, both splitters' sizes, the file
  browser's column widths, and its set of expanded folders via `QSettings`,
  restoring them on the next launch. A path line edit above the file browser
  mirrors the current selection and jumps to a pasted path on Enter.
- Releases now also run off a plain push to `main` (not just manual
  `workflow_dispatch`), gated behind the `release` GitHub Environment's
  required-reviewer approval; push-triggered releases auto-derive their
  version tag from CMakeLists.txt + short SHA since there's no version input
  to type. See "Releases" above - requires one-time Environment setup.
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
