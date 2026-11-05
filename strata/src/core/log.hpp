#pragma once

#include <string>

namespace strata::log {

enum class Level { Debug, Info, Warn, Error };

void setMinLevel(Level level);
// Also append log lines to a file (pass an empty path to stop).
void setFile(const std::string& path);
void write(Level level, const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 2, 3)))
#endif
    ;

}  // namespace strata::log

#define LOG_DEBUG(...) ::strata::log::write(::strata::log::Level::Debug, __VA_ARGS__)
#define LOG_INFO(...) ::strata::log::write(::strata::log::Level::Info, __VA_ARGS__)
#define LOG_WARN(...) ::strata::log::write(::strata::log::Level::Warn, __VA_ARGS__)
#define LOG_ERROR(...) ::strata::log::write(::strata::log::Level::Error, __VA_ARGS__)
