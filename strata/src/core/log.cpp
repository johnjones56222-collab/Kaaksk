#include "core/log.hpp"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <mutex>

namespace strata::log {

namespace {
std::mutex gMutex;
Level gMinLevel = Level::Info;
std::ofstream gFile;
const auto gStart = std::chrono::steady_clock::now();

const char* levelName(Level l) {
  switch (l) {
    case Level::Debug: return "debug";
    case Level::Info: return "info";
    case Level::Warn: return "warn";
    case Level::Error: return "error";
  }
  return "?";
}
}  // namespace

void setMinLevel(Level level) {
  std::lock_guard<std::mutex> lock(gMutex);
  gMinLevel = level;
}

void setFile(const std::string& path) {
  std::lock_guard<std::mutex> lock(gMutex);
  if (gFile.is_open()) gFile.close();
  if (!path.empty()) gFile.open(path, std::ios::out | std::ios::trunc);
}

void write(Level level, const char* fmt, ...) {
  char message[2048];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(message, sizeof(message), fmt, args);
  va_end(args);
  double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - gStart).count();
  std::lock_guard<std::mutex> lock(gMutex);
  if (level < gMinLevel) return;
  std::fprintf(stderr, "[%8.3f] %-5s %s\n", t, levelName(level), message);
  std::fflush(stderr);
  if (gFile.is_open()) {
    char line[2200];
    std::snprintf(line, sizeof(line), "[%8.3f] %-5s %s\n", t, levelName(level), message);
    gFile << line;
    gFile.flush();
  }
}

}  // namespace strata::log
