// Copyright (c) 2026 Hansol Park (mooming.go@gmail.com). All rights reserved.

#pragma once

#include <cstdint>
#include <string>

namespace hbe
{

/**
 * @brief A 32-bit RGBA software framebuffer for pixel-level rendering.
 *
 * All UI components draw onto this framebuffer using primitive operations
 * (SetPixel, FillRect, DrawLine, DrawText). The framebuffer is then
 * presented to the native NSView for display on screen.
 *
 * Pixel format: RGBA8, row-major, top-left origin (Y=0 at top).
 */
class Framebuffer final
{
public:
	Framebuffer();
	~Framebuffer();

	Framebuffer(const Framebuffer&) = delete;
	Framebuffer& operator=(const Framebuffer&) = delete;
	Framebuffer(Framebuffer&&) noexcept = delete;
	Framebuffer& operator=(Framebuffer&&) noexcept = delete;

	/**
	 * @brief Allocates the pixel buffer with the given dimensions.
	 * @param width  Buffer width in pixels.
	 * @param height Buffer height in pixels.
	 * @return True if allocation succeeded, false otherwise.
	 */
	bool Initialize(int width, int height);

	/**
	 * @brief Clears the entire framebuffer to the specified ARGB color.
	 * @param color 32-bit ARGB color (0xAARRGGBB).
	 */
	void Clear(uint32_t color) noexcept;

	/**
	 * @brief Sets a single pixel to the given color.
	 * @param x     X coordinate (0 = left).
	 * @param y     Y coordinate (0 = top, due to framebuffer orientation).
	 * @param color 32-bit ARGB color.
	 */
	void SetPixel(int x, int y, uint32_t color) noexcept;

	/**
	 * @brief Retrieves the color of a single pixel.
	 * @param x X coordinate.
	 * @param y Y coordinate.
	 * @return 32-bit ARGB color at (x, y).
	 */
	[[nodiscard]] uint32_t GetPixel(int x, int y) const noexcept;

	/**
	 * @brief Fills a rectangular region with a solid color.
	 * @param x     Top-left X.
	 * @param y     Top-left Y.
	 * @param w     Width in pixels.
	 * @param h     Height in pixels.
	 * @param color Fill color.
	 */
	void FillRect(int x, int y, int w, int h, uint32_t color) noexcept;

	/**
	 * @brief Draws a straight line using Bresenham's algorithm.
	 * @param x0    Start X.
	 * @param y0    Start Y.
	 * @param x1    End X.
	 * @param y1    End Y.
	 * @param color Line color.
	 */
	void DrawLine(int x0, int y0, int x1, int y1, uint32_t color) noexcept;

	/**
	 * @brief Draws a single character using the bitmap font.
	 * @param x      Left X position.
	 * @param y      Top Y position.
	 * @param ch     Character to draw.
	 * @param color  Text color (RGB; alpha ignored, assumed 0xFF).
	 */
	void DrawChar(int x, int y, char ch, uint32_t color) noexcept;

	/**
	 * @brief Draws a null-terminated text string using the bitmap font.
	 * @param x      Left X position.
	 * @param y      Top Y position.
	 * @param text   Null-terminated string to render.
	 * @param color  Text color.
	 */
	void DrawText(int x, int y, const char* text, uint32_t color) noexcept;

	/**
	 * @brief Blits (copies) another framebuffer into this one at the given offset.
	 * @param src   Source framebuffer.
	 * @param dstX  Destination X in this buffer.
	 * @param dstY  Destination Y in this buffer.
	 */
	void BitBlt(const Framebuffer* src, int dstX, int dstY) noexcept;

	/**
	 * @brief Gets a raw pointer to the pixel data.
	 * @return Pointer to the RGBA8 pixel buffer.
	 */
	[[nodiscard]] uint32_t* GetData() noexcept;
	[[nodiscard]] const uint32_t* GetData() const noexcept;

	[[nodiscard]] int GetWidth() const noexcept;
	[[nodiscard]] int GetHeight() const noexcept;
	[[nodiscard]] size_t GetPitch() const noexcept;

private:
	uint32_t* data;
	int width;
	int height;
	size_t pitch;
};

} // namespace hbe
