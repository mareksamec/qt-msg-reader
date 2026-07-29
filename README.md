# Qt MSG Reader

A Qt-based application for reading Microsoft Outlook `.msg` files.

## Overview

This application provides a simple graphical interface to:
- View email message contents (subject, sender, recipients, date, body)
- Display HTML and plain text email bodies
- List and save attachments
- Browse and open `.msg` files
- View parsing status and errors in a log window

## Architecture

The application is built with:
- **C++/Qt6** for the GUI frontend
- **libmsg** (`libmsg/`), an in-tree pure-C library, for parsing `.msg` files
- **CMake** build system

### Key Components

| File | Description |
|------|-------------|
| `main.cpp` | Application entry point |
| `MainWindow.h/cpp` | Main window with file browser, message view, attachments, and status log |
| `MsgParser.h/cpp` | Thin C++ wrapper around `libmsg` for MSG parsing |
| `EmailTypes.h` | Data structures (EmailMessage, EmailAttachment) |
| `MsgFileModel.h/cpp` | File system model filtered for .msg files |
| `AttachmentModel.h/cpp` | Table model for attachments display |

## Dependencies

- **Qt 6.x** - GUI framework (Qt::Widgets, Qt::Core)
- **CMake 3.16+** - Build system
- **C++17** / **C99** - Language standards

No Python, and no other runtime dependency beyond Qt and libc - `.msg` parsing
is done in-process by `libmsg/`, a small dependency-free C library (see
`libmsg/README.md` for its scope and limitations).

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Deployment

The built binary has no dependency beyond the Qt runtime libraries.

On Arch Linux:
```bash
cd packaging/arch
makepkg -si                    # latest tagged release
makepkg -si -p PKGBUILD-git    # latest git HEAD
```
See `packaging/arch/README.md` for details, including the eventual AUR
publishing path.

## Running

```bash
# Run with file browser
./qt-msg-reader

# Open a specific file
./qt-msg-reader path/to/file.msg
```

## Usage

1. **Browse files**: Use the left panel to navigate to MSG files
2. **Open file**: Double-click a .msg file or use File > Open
3. **View message**: Header, body, and attachments are displayed
4. **Save attachments**: Double-click an attachment to save it
5. **View status**: Check the Status Log at the bottom for parsing details

## Project Structure

```
qt-msg-reader/
├── CMakeLists.txt           # Build configuration
├── README.md                # This file
├── CONTEXT.md               # Development context and notes
├── LICENSE                  # MIT license text
├── packaging/arch/          # Arch Linux PKGBUILDs (release + git) - see its README.md
├── src/
│   ├── main.cpp             # Application entry point
│   ├── MainWindow.h/cpp     # Main window UI
│   ├── MsgParser.h/cpp      # libmsg wrapper for MSG parsing
│   ├── EmailTypes.h         # Data structures
│   ├── MsgFileModel.h/cpp   # File browser model
│   └── AttachmentModel.h/cpp # Attachment table model
├── libmsg/                  # Pure-C .msg parsing library (see libmsg/README.md)
└── build/
    └── qt-msg-reader        # Executable
```
## TODO
- [ ] Improve build system add Releases
- [ ] Remove Windows support - not needed as you can use Outlook on Win.

## Notes
Fun little project built with agentic coding, I'm open to MR but I might not have time to review everything.

## License

MIT License
