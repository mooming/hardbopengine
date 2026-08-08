# MacOSApp

A custom software-rendered window application example for the HardBop Engine.

## Overview

MacOSApp demonstrates how to build a window-based graphical application from scratch
on macOS using Cocoa (via Objective-C++). It implements its own window and application
abstractions rather than using the engine's `OS::IWindow` / `OS::IApplication` interfaces,
serving as a learning exercise to understand and eventually replace the engine's
current OSAL (Operating System Abstraction Layer) implementation.

## Features

- **Full Screen Mode**: Toggle with F11 key or the "Full Screen" button in the UI
- **Windowed Application**: Standard resizable macOS window with title bar and traffic light buttons
- **Text Panel with Scroll Bars**: Multi-line text display with a vertical scrollbar (thumb dragging, track click, mouse wheel)
- **Close Button**: A UI button that cleanly terminates the application

## Architecture

```
Layer 5: Application Logic (Main.cpp)
Layer 4: UI Toolkit (TextPanel, ScrollBar, Button)
Layer 3: Software Framebuffer (Framebuffer - GetPixel, SetPixel, FillRect, BitBlt)
Layer 2: Window Abstraction (Window.mm - NSWindow bridge)
Layer 1: OS Layer (Cocoa Foundation - NSApplication, NSView, NSEvent)
```

All UI components are rendered onto a software framebuffer and then presented
to the native NSView for display on screen.

## Build Instructions

### Requirements
- macOS 10.15+ (tested on macOS 12+)
- CMake 3.12+
- C++23 compiler (clang 15+)
- Ninja (optional, recommended)

### Build

```bash
# From the HardBop Engine repository root
cmake --fresh -B build -G "Ninja Multi-Config" -S .
cmake --build build --config Debug
cmake --build build --config Release
```

### Run

```bash
./build/MacOSApp
```

## Controls

| Input | Action |
|-------|--------|
| **F11** | Toggle between full-screen and windowed mode |
| **Escape** | Exit full-screen mode (when in full-screen) |
| **Mouse Move** | Hover over buttons and scrollbar for visual feedback |
| **Left Click** | Click buttons, drag scrollbar thumb |
| **Mouse Wheel** | Scroll text panel content |

## Project Structure

```
MacOSApp/
├── CMakeLists.txt          # CMake build configuration
├── Design/
│   └── WindowAppDesign.html  # Detailed design document
├── Include/
│   ├── Framebuffer.h      # Software framebuffer API
│   ├── Window.h           # Window abstraction (C++ API)
│   ├── Application.h      # Application wrapper (C++ API)
│   └── UI/
│       ├── UIComponent.h  # Abstract base class
│       ├── TextPanel.h    # Scrollable text
│       ├── ScrollBar.h    # Vertical scrollbar
│       └── Button.h       # Clickable button
├── Source/
│   ├── Main.cpp           # Entry point and main loop
│   ├── Framebuffer.cpp    # Pixel operations + font
│   ├── Window.mm          # NSWindow bridge (Objective-C++)
│   ├── Application.mm     # NSApplication bridge (Objective-C++)
│   └── UI/
│       ├── TextPanel.cpp
│       ├── ScrollBar.cpp
│       └── Button.cpp
└── README.md              # This file
```

## Design Document

See [Design/WindowAppDesign.html](Design/WindowAppDesign.html) for the full detailed
design document with architecture diagrams, UI mockups, use case analysis,
class diagrams, and test plans.

## License

Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.
