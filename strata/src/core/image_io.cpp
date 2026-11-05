#include "core/image_io.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <vector>

namespace strata {

namespace {
const std::array<uint32_t, 256>& crcTable() {
  static const std::array<uint32_t, 256> table = [] {
    std::array<uint32_t, 256> t{};
    for (uint32_t n = 0; n < 256; ++n) {
      uint32_t c = n;
      for (int k = 0; k < 8; ++k) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
      t[n] = c;
    }
    return t;
  }();
  return table;
}

void putU32(std::vector<uint8_t>& out, uint32_t v) {
  out.push_back(static_cast<uint8_t>(v >> 24));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

void writeChunk(std::ofstream& f, const char type[4], const std::vector<uint8_t>& data) {
  std::vector<uint8_t> buf;
  buf.reserve(data.size() + 12);
  putU32(buf, static_cast<uint32_t>(data.size()));
  for (int i = 0; i < 4; ++i) buf.push_back(static_cast<uint8_t>(type[i]));
  buf.insert(buf.end(), data.begin(), data.end());
  uint32_t crc = crc32(buf.data() + 4, data.size() + 4);
  putU32(buf, crc);
  f.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
}
}  // namespace

uint32_t crc32(const uint8_t* data, size_t size, uint32_t crc) {
  const auto& t = crcTable();
  crc = ~crc;
  for (size_t i = 0; i < size; ++i) crc = t[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
  return ~crc;
}

bool writePng(const std::string& path, int width, int height, int channels, const uint8_t* pixels,
              bool flipVertically) {
  if (width <= 0 || height <= 0 || (channels != 3 && channels != 4) || !pixels) return false;
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  static const uint8_t kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  f.write(reinterpret_cast<const char*>(kSignature), 8);

  std::vector<uint8_t> ihdr;
  putU32(ihdr, static_cast<uint32_t>(width));
  putU32(ihdr, static_cast<uint32_t>(height));
  ihdr.push_back(8);                                  // bit depth
  ihdr.push_back(channels == 4 ? 6 : 2);              // colour type
  ihdr.push_back(0);                                  // compression
  ihdr.push_back(0);                                  // filter
  ihdr.push_back(0);                                  // interlace
  writeChunk(f, "IHDR", ihdr);

  // Raw scanlines, each prefixed with filter type 0.
  const size_t rowBytes = static_cast<size_t>(width) * static_cast<size_t>(channels);
  std::vector<uint8_t> raw;
  raw.reserve((rowBytes + 1) * static_cast<size_t>(height));
  for (int y = 0; y < height; ++y) {
    int srcY = flipVertically ? (height - 1 - y) : y;
    raw.push_back(0);
    const uint8_t* row = pixels + static_cast<size_t>(srcY) * rowBytes;
    raw.insert(raw.end(), row, row + rowBytes);
  }

  // zlib stream made of stored (uncompressed) deflate blocks.
  std::vector<uint8_t> z;
  z.reserve(raw.size() + raw.size() / 65535 * 5 + 16);
  z.push_back(0x78);
  z.push_back(0x01);
  size_t pos = 0;
  do {
    size_t len = std::min<size_t>(65535, raw.size() - pos);
    bool final = pos + len == raw.size();
    z.push_back(final ? 1 : 0);
    z.push_back(static_cast<uint8_t>(len & 0xFF));
    z.push_back(static_cast<uint8_t>(len >> 8));
    z.push_back(static_cast<uint8_t>(~len & 0xFF));
    z.push_back(static_cast<uint8_t>((~len >> 8) & 0xFF));
    z.insert(z.end(), raw.begin() + static_cast<std::ptrdiff_t>(pos),
             raw.begin() + static_cast<std::ptrdiff_t>(pos + len));
    pos += len;
  } while (pos < raw.size());
  uint32_t a = 1, b = 0;
  for (uint8_t v : raw) {
    a = (a + v) % 65521u;
    b = (b + a) % 65521u;
  }
  putU32(z, (b << 16) | a);
  writeChunk(f, "IDAT", z);
  writeChunk(f, "IEND", {});
  return static_cast<bool>(f);
}

}  // namespace strata
