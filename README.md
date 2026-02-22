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
- **Python extract_msg** library for parsing MSG files
- **Python C API** for bridging C++ and Python
- **CMake** build system

### Key Components

| File | Description |
|------|-------------|
| `main.cpp` | Application entry point |
| `MainWindow.h/cpp` | Main window with file browser, message view, attachments, and status log |
| `MsgParser.h/cpp` | Python bridge for MSG parsing using extract_msg |
| `EmailTypes.h` | Data structures (EmailMessage, EmailAttachment) |
| `MsgFileModel.h/cpp` | File system model filtered for .msg files |
| `AttachmentModel.h/cpp` | Table model for attachments display |

## Dependencies

- **Qt 6.x** - GUI framework (Qt::Widgets, Qt::Core)
- **Python 3.14** - For extract_msg library (system Python is used, packages are bundled)
- **CMake 3.16+** - Build system
- **C++17** - Language standard

## Building

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

The build process automatically copies Python packages (including extract_msg) to `build/python-packages/`.

## Deployment

The application can be deployed by copying:
- `qt-msg-reader` executable
- `python-packages/` directory (next to the executable)

No Python installation is required on the target system.

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
├── src/
│   ├── main.cpp             # Application entry point
│   ├── MainWindow.h/cpp     # Main window UI
│   ├── MsgParser.h/cpp      # Python bridge for MSG parsing
│   ├── EmailTypes.h         # Data structures
│   ├── MsgFileModel.h/cpp   # File browser model
│   └── AttachmentModel.h/cpp # Attachment table model
├── .venv/                   # Python virtual environment (development)
├── build/
│   ├── qt-msg-reader        # Executable
│   └── python-packages/     # Bundled Python packages (deployment)
└── resources/
    └── icons/               # Application icons
```
## TODO
- [ ] Re-write MsgParser to clean C or C++ to avoid Python dependency msg-extract
- [ ] Improve build system add Releases
- [ ] Remove Windows support - not needed as you can use Outlook on Win.

## Notes
Fun little project built with agentic coding, I'm open to MR but I might not have time to review everything.

## License

MIT License
