# Qt MSG Reader

A Qt-based application for reading Microsoft Outlook `.msg` files.

## Overview

This application provides a simple graphical interface to:
- View email message contents (subject, sender, recipients, date, body)
- List and save attachments
- Browse and open `.msg` files

## MSG File Format

Microsoft Outlook `.msg` files use the **Compound File Binary Format (CFB)**, also known as OLE2 or Structured Storage. The format organizes data in a filesystem-like structure with:
- **Streams**: Named data sequences (like files)
- **Storages**: Containers for streams (like directories)

Key properties stored in MSG files:
- `Subject`, `Body` (plain text and HTML)
- `Sender`, `To`, `Cc`, `Bcc`
- `Date/Time` information
- `Attachments` (embedded files)

## Project Structure

```
qt-msg-reader/
├── CMakeLists.txt           # Build configuration
├── README.md                # This file
├── src/
│   ├── main.cpp             # Application entry point
│   ├── MainWindow.h         # Main window header
│   ├── MainWindow.cpp       # Main window implementation
│   ├── MsgParser.h          # MSG file parser interface
│   ├── MsgParser.cpp        # MSG parser implementation
│   ├── MsgFileModel.h       # File browser model for .msg files
│   ├── MsgFileModel.cpp     # File model implementation
│   └── AttachmentModel.h    # Model for attachments list
│   └── AttachmentModel.cpp  # Attachment model implementation
├── resources/
│   └── icons/               # Application icons
└── tests/
    └── test_msgparser.cpp   # Unit tests for parser
```

## Development Plan

### Phase 1: Project Setup
1. Create CMakeLists.txt with Qt6 configuration
2. Set up basic MainWindow with menu bar
3. Implement file open dialog

### Phase 2: MSG Parser
1. Implement CFB/OLE2 compound file parser
2. Extract basic email properties (subject, body, sender)
3. Handle property streams and named properties

### Phase 3: Message Display
1. Create email view widget (header + body)
2. Support both plain text and HTML body
3. Display sender, recipients, date

### Phase 4: Attachments
1. Extract attachment information
2. Display attachment list
3. Implement save functionality with file dialog

### Phase 5: File Browser
1. Create file browser panel (QFileSystemModel filtered to .msg)
2. Implement double-click to open
3. Add recent files list

## Dependencies

- **Qt 6.x** - GUI framework
  - `Qt::Widgets` - Main window, dialogs, widgets
  - `Qt::Core` - Core functionality
- **CMake 3.16+** - Build system
- **C++17** - Language standard

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Qt Concepts Overview

### Core Concepts

1. **QObject**: Base class for all Qt objects, provides:
   - Parent-child memory management
   - Signal-slot communication
   - Property system

2. **Signals and Slots**: Qt's event handling mechanism
   - Signals: Events emitted by objects
   - Slots: Functions that respond to signals
   - Connection: `connect(sender, &Sender::signal, receiver, &Receiver::slot)`

3. **Event Loop**: Qt applications run an event loop
   - Processes events (mouse, keyboard, timers)
   - Managed by `QApplication`

### GUI Components

1. **QMainWindow**: Standard application window
   - Menu bar, tool bars, status bar
   - Central widget for main content

2. **Widgets**: UI building blocks
   - `QLabel` - Text/images
   - `QTextEdit` - Rich text display
   - `QTreeView` - Hierarchical data
   - `QListView` - List data

3. **Model/View Pattern**: Separation of data and display
   - Model: Data source (e.g., `QFileSystemModel`)
   - View: Display (e.g., `QTreeView`)
   - Delegate: Rendering and editing

4. **Layouts**: Automatic widget positioning
   - `QVBoxLayout` - Vertical
   - `QHBoxLayout` - Horizontal
   - `QGridLayout` - Grid-based

## License

MIT License
