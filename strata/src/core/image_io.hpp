#pragma once

#include <cstdint>
#include <string>

namespace strata {

// Writes an 8-bit RGB (channels = 3) or RGBA (channels = 4) PNG. Rows are
// expected top to bottom unless flipVertically is set (OpenGL read-back order).
bool writePng(const std::string& path, int width, int height, int channels, const uint8_t* pixels,
              bool flipVertically = false);

uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc = 0);

}  // namespace strata
