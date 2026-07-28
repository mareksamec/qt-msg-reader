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

* Subject, plain text body, HTML body (synthesized from the plain body if
  the file has no HTML part, same fallback `extract_msg` uses)
* Sender name and email
* Send/delivery date
* Recipients (name, email, To/Cc/Bcc)
* Attachments (filename, mimetype, content ID for inline/`cid:` images, raw
  bytes)

Explicitly out of scope for now: RTF bodies, named properties, embedded
message attachments, signed messages, non-Message item types, and writing
`.msg` files. ANSI (`...001E`) string streams are decoded as Windows-1252;
Unicode (`...001F`) streams are decoded fully.

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
src/msg_reader.c        public API implementation
tools/msgdump.c         CLI demo: prints the fields libmsg extracts
tests/make_test_msg.py  builds tests/sample.msg, a hand-rolled CFB fixture
tests/test_basic.c      asserts parsed fields against that fixture
```

## Building

```
cmake -S . -B build
cmake --build build
ctest --test-dir build   # regenerates nothing; run tests/make_test_msg.py first
python3 tests/make_test_msg.py tests/sample.msg
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

This is not yet wired into the Qt application (`src/MsgParser.cpp` still
uses the Python bridge) — that integration is a follow-up.
