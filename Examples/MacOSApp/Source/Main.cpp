// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#include "Application.h"
#include "Framebuffer.h"
#include "Window.h"

#include "UI/TextPanel.h"
#include "UI/ScrollBar.h"
#include "UI/Button.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>

namespace hbe
{

// Framebuffer dimensions for windowed mode
constexpr int WindowWidth = 1024;
constexpr int WindowHeight = 768;

// Colors (ARGB format)
constexpr uint32_t BgColor = 0xFF0D1117u;
constexpr uint32_t PanelAreaColor = 0xFF161B22u;

// Layout constants
constexpr int PanelLeft = 20;
constexpr int PanelRight = 20;
constexpr int PanelTop = 60;
constexpr int PanelBottom = 80;
constexpr int ScrollBarWidth = 16;
constexpr int ButtonHeight = 30;
constexpr int ButtonWidth = 120;

// Sample text content for the text panel
const char SampleText[] =
    "=== MacOSApp Sample Text Panel ===\n"
    "\n"
    "This is a demonstration of a custom software-rendered text panel\n"
    "with a vertical scrollbar. The panel renders text using a bitmap\n"
    "font drawn directly onto a software framebuffer.\n"
    "\n"
    "Features:\n"
    "  - Full Screen mode (toggle with F11 key or button)\n"
    "  - Windowed mode (standard macOS window)\n"
    "  - Scrollable text panel\n"
    "  - Vertical scrollbar with thumb dragging\n"
    "  - Close button to terminate the application\n"
    "\n"
    "Controls:\n"
    "  - F11: Toggle full-screen / windowed mode\n"
    "  - Escape: Exit full-screen mode\n"
    "  - Mouse: Move, click, drag scrollbar\n"
    "  - Scroll wheel: Scroll text panel\n"
    "\n"
    "Technical Details:\n"
    "  - Rendering: Pure software framebuffer (GetPixel, SetPixel, BitBlt)\n"
    "  - Font: Custom 8x12 bitmap font (96 ASCII characters)\n"
    "  - Platform: macOS Cocoa (NSWindow, NSView)\n"
    "  - Language: C++23 with Objective-C++ bridge\n"
    "  - Build: CMake with Ninja Multi-Config\n"
    "\n"
    "Architecture Layers:\n"
    "  Layer 5: Application Logic (Main.cpp)\n"
    "  Layer 4: UI Toolkit (TextPanel, ScrollBar, Button)\n"
    "  Layer 3: Software Framebuffer (Framebuffer - GetPixel, SetPixel, BitBlt)\n"
    "  Layer 2: Window Abstraction (Window.mm - NSWindow bridge)\n"
    "  Layer 1: OS Layer (Cocoa Foundation - NSApplication, NSView, NSEvent)\n"
    "\n"
    "Data Flow:\n"
    "  Cocoa Events -> Application.PollEvents() -> Window.PollEvents()\n"
    "  -> InputState -> UIComponent.OnUpdate() -> UIComponent.Draw() -> Framebuffer\n"
    "  -> Presentation via CGImage -> NSView.drawRect() -> Screen\n"
    "\n"
    "Memory Layout:\n"
    "  The framebuffer is a contiguous uint32_t array in RGBA8 format,\n"
    "  stored row-major with a pitch (bytes per row) for alignment.\n"
    "  Pixel (x,y) is at offset: y * (pitch/4) + x\n"
    "\n"
    "More sample lines to test scrolling:\n"
    "  Line 40: The scrollbar should allow navigating through all content.\n"
    "  Line 41: Dragging the thumb provides smooth scrolling.\n"
    "  Line 42: Clicking above or below the thumb scrolls a page.\n"
    "  Line 43: The mouse wheel also scrolls the text content.\n"
    "  Line 44: When the mouse is over the scrollbar, it highlights.\n"
    "  Line 45: The close button turns red when hovered.\n"
    "  Line 46: The full-screen button turns blue when hovered.\n"
    "  Line 47: Pressing F11 toggles between windowed and full-screen.\n"
    "  Line 48: In full-screen, the UI resizes to fill the entire display.\n"
    "  Line 49: The Escape key exits full-screen back to windowed mode.\n"
    "  Line 50: All UI components use the software framebuffer for rendering.\n"
    "  Line 51: No external GUI library is used (e.g., no ImGui).\n"
    "  Line 52: All rendering is done via GetPixel, SetPixel, FillRect, BitBlt.\n"
    "  Line 53: Text is rendered with a custom 8x12 bitmap font.\n"
    "  Line 54: Each character is 8 pixels wide and 12 pixels tall.\n"
    "  Line 55: The font supports all 96 printable ASCII characters.\n"
    "  Line 56: Color values are stored as 32-bit ARGB (0xAARRGGBB).\n"
    "  Line 57: The framebuffer pitch ensures proper row alignment.\n"
    "  Line 58: BitBlt copies a source framebuffer to a destination offset.\n"
    "  Line 59: DrawLine uses Bresenham's algorithm for line rasterization.\n"
    "  Line 60: Hit testing checks if mouse coordinates are within bounds.\n"
    "  Line 61: The UI is updated once per frame in the main loop.\n"
    "  Line 62: The framebuffer is cleared and redrawn every frame.\n"
    "  Line 63: This ensures no visual artifacts from previous frames.\n"
    "  Line 64: The Present() method triggers the NSView to redraw.\n"
    "  Line 65: CGImageCreate wraps the raw pixel data for display.\n"
    "  Line 66: This sample text has more than enough lines to scroll.\n";

void RunMainLoop(Application& app, Window& window, Framebuffer& framebuffer,
                 TextPanel& textPanel, ScrollBar& scrollbar,
                 Button& closeButton, Button& fullscreenButton,
                 bool& appShouldExit)
{
    // Layout UI components based on current window dimensions
    const int winW = window.GetWidth();
    const int winH = window.GetHeight();

    const int panelW = winW - PanelLeft - PanelRight - ScrollBarWidth;
    const int panelH = winH - PanelTop - PanelBottom;

    textPanel.SetBounds(PanelLeft, PanelTop, panelW, panelH);

    scrollbar.SetPosition(winW - PanelRight - ScrollBarWidth, PanelTop);
    scrollbar.SetSize(ScrollBarWidth, panelH);

    // Set up scrollbar range based on text content
    const int totalLines = textPanel.GetTotalLines();
    const int visibleLines = textPanel.GetVisibleLineCount();
    const int scrollableLines = std::max(0, totalLines - visibleLines);

    scrollbar.SetRange(0.0f, static_cast<float>(scrollableLines));
    scrollbar.SetPageSize(visibleLines > 0
        ? static_cast<float>(visibleLines) / static_cast<float>(std::max(1, totalLines))
        : 1.0f);

    // Text panel scroll callback
    textPanel.SetScrollCallback([&scrollbar](float value)
    {
        scrollbar.SetValue(value);
    });

    // Scrollbar -> text panel callback
    scrollbar.SetOnValueChanged([&textPanel](float value)
    {
        textPanel.SetScrollOffset(static_cast<int>(value));
    });

    // Position buttons
    fullscreenButton.SetBounds(
        PanelLeft,
        winH - PanelBottom + (PanelBottom - ButtonHeight) / 2 - 5,
        ButtonWidth, ButtonHeight
    );
    closeButton.SetBounds(
        winW - PanelRight - ButtonWidth,
        winH - PanelBottom + (PanelBottom - ButtonHeight) / 2 - 5,
        ButtonWidth, ButtonHeight
    );

    // Poll events
    app.PollEvents();
    window.PollEvents();

    // Handle F11 full-screen toggle
    if (window.WasKeyPressed(Window::KeyF11))
    {
        window.ToggleFullScreen();
    }

    // Handle Escape to exit full-screen
    if (window.IsFullScreen() && window.WasKeyPressed(Window::KeyEscape))
    {
        window.ToggleFullScreen();
    }

    // Build InputState for UI components
    const MouseState& mouse = window.GetMouseState();
    InputState input;
    input.mouseX = mouse.x;
    input.mouseY = mouse.y;
    input.leftMousePressed = mouse.leftButtonPressed;
    input.leftMouseReleased = mouse.leftButtonReleased;
    input.mouseWheelDeltaY = mouse.wheelDeltaY;

    // Update UI components
    textPanel.OnUpdate(input);
    scrollbar.OnUpdate(input);
    fullscreenButton.OnUpdate(input);
    closeButton.OnUpdate(input);

    // Update fullscreen button label based on state
    if (window.IsFullScreen())
    {
        fullscreenButton.SetLabel("Windowed");
    }
    else
    {
        fullscreenButton.SetLabel("Full Screen");
    }

    // Clear framebuffer
    framebuffer.Clear(BgColor);

    // Draw panel area background
    framebuffer.FillRect(
        PanelLeft, PanelTop,
        winW - PanelLeft - PanelRight,
        winH - PanelTop - PanelBottom,
        PanelAreaColor
    );

    // Draw UI components
    textPanel.Draw(framebuffer);
    scrollbar.Draw(framebuffer);
    fullscreenButton.Draw(framebuffer);
    closeButton.Draw(framebuffer);

    // Present to screen
    window.Present(framebuffer);
}

} // namespace hbe


int main(int argc, const char* argv[])
{
    (void)argc;
    (void)argv;

    // Initialize application
    auto app = std::make_unique<hbe::Application>();
    if (!app->Initialize())
    {
        std::cerr << "Error: Failed to initialize Application" << std::endl;
        return 1;
    }

    // Create window
    auto window = hbe::Window::Create(std::string("MacOSApp"), hbe::WindowWidth, hbe::WindowHeight);
    if (!window)
    {
        std::cerr << "Error: Failed to create Window" << std::endl;
        app->ShutDown();
        return 1;
    }

    // Create framebuffer
    hbe::Framebuffer framebuffer;
    if (!framebuffer.Initialize(window->GetWidth(), window->GetHeight()))
    {
        std::cerr << "Error: Failed to initialize Framebuffer" << std::endl;
        window->Close();
        app->ShutDown();
        return 1;
    }

    // Create UI components
    hbe::TextPanel textPanel;
    textPanel.SetText(hbe::SampleText);

    hbe::ScrollBar scrollbar;
    scrollbar.SetPageSize(0.3f);
    scrollbar.SetRange(0.0f, 30.0f);

    hbe::Button closeButton;
    closeButton.SetLabel("Close");
    closeButton.SetType(hbe::Button::Type::Danger);

    hbe::Button fullscreenButton;
    fullscreenButton.SetLabel("Full Screen");
    fullscreenButton.SetType(hbe::Button::Type::Primary);

    // Close button callback
    bool appShouldExit = false;
    closeButton.SetOnClick([&appShouldExit]()
    {
        appShouldExit = true;
    });

    // Fullscreen button callback
    fullscreenButton.SetOnClick([&window]()
    {
        window->ToggleFullScreen();
    });

    // Main application loop
    while (!appShouldExit && !window->IsClosed())
    {
        hbe::RunMainLoop(*app, *window, framebuffer,
                    textPanel, scrollbar,
                    closeButton, fullscreenButton,
                    appShouldExit);
    }

    // Cleanup
    window->Close();
    framebuffer.Clear(0);
    app->ShutDown();

    return 0;
}
