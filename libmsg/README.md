# libmsg

A small, dependency-free C library for reading the fields most applications
need out of Microsoft Outlook `.msg` files. Written to replace the Python
`extract_msg` dependency used elsewhere in this project (see the top-level
`CONTEXT.md`) with something that links directly into the Qt/C++ app and has
no runtime dependency on a Python interpreter.

## Scope

`.msg` files are Compound File Binary (OLE2) containers holding MAPI
properties (MS-CFB + MS-OXMSG). `extract_msg` implements a very large
surface of that format (calendar/contact/task items, S/MIME-signed
messages, named properties, RTF (de)compression, embedded `.msg`
attachments, character-set auto-detection, etc). This library deliberately
covers only the common case, matching what this project's `MsgParser`
actually used:

* Subject, plain text body, HTML body (from PR_HTML if present; otherwise
  recovered from PR_RTF_COMPRESSED - either the real HTML if the RTF is
  "\fromhtml1"-encapsulated (MS-OXRTFEX), or as a last resort synthesized
  from the plain text body, same fallback `extract_msg` uses)
* Sender name and email
* Send/delivery date
* Recipients (name, email, To/Cc/Bcc)
* Attachments (filename, mimetype, content ID for inline/`cid:` images, raw
  bytes)

RTF support (`src/msg_rtf.c`) covers MS-OXRTFCP decompression and enough of
the RTF grammar (destinations, `\uN`/`\'hh` escapes, `\htmlrtf`/`\htmltag`
HTML encapsulation) to recover readable text and, when present, the
original HTML. It does not reconstruct RTF's own formatting (fonts, colors,
tables) as such - a message composed in plain "Rich Text" format (no HTML
encapsulation) degrades to its plain text, not styled output.

Explicitly out of scope for now: named properties, embedded message
attachments, signed messages, non-Message item types, and writing `.msg`
files. ANSI (`...001E`) string streams are decoded as Windows-1252; Unicode
(`...001F`) streams are decoded fully.

The library parses untrusted input (an attacker-supplied `.msg` file), so
`src/cfb.c` is defensive about corrupt/hostile containers: sector chain
walks are bounded, and stream sizes taken from directory entries are
clamped to what the file could actually contain rather than trusted
outright (a raw `uint64_t` size field could otherwise be used to force a
multi-gigabyte allocation from a tiny file).

## Layout

```
include/libmsg/msg.h   public API
src/cfb.[ch]           MS-CFB (OLE2) container reader
src/msg_properties.[ch] MAPI property store parsing + stream lookup helpers
src/msg_strings.[ch]   UTF-16LE/codepage -> UTF-8, FILETIME -> time_t
src/msg_rtf.[ch]       PR_RTF_COMPRESSED decompression + text/HTML extraction
src/msg_reader.c        public API implementation
tools/msgdump.c         CLI demo: prints the fields libmsg extracts
tests/make_test_msg.py  builds tests/sample.msg, a hand-rolled CFB fixture
tests/test_basic.c      asserts parsed fields against that fixture
tests/test_rtf.c        direct msg_rtf.c unit tests (decompression, text/HTML extraction)
tests/make_rtf_test_msg.py builds tests/sample_rtf.msg (RTF-only body, no PR_BODY/PR_HTML)
tests/test_rtf_msg.c    asserts the RTF fallback end-to-end through msg_open()
```

## Building

```
cmake -S . -B build
cmake --build build
ctest --test-dir build   # regenerates nothing; run the fixture generators first
python3 tests/make_test_msg.py tests/sample.msg
python3 tests/make_rtf_test_msg.py tests/sample_rtf.msg
./build/msgdump tests/sample.msg
```

## Usage

```c
#include <libmsg/msg.h>

msg_error_t err;
msg_file_t *msg = msg_open("message.msg", &err);
if (!msg) { fprintf(stderr, "%s\n", msg_error_string(err)); return 1; }

printf("%s\n", msg_get_subject(msg));
for (size_t i = 0; i < msg_get_attachment_count(msg); i++) {
    const msg_attachment_t *a = msg_get_attachment(msg, i);
    /* a->filename, a->mimetype, a->content_id, a->data, a->size */
}

msg_close(msg);
```
