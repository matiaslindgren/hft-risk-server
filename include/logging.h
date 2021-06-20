#ifndef INCLUDED_RISKSERVICE_LOGGING_HEADER
#define INCLUDED_RISKSERVICE_LOGGING_HEADER
/*
 * Simple replacement for spdlog.
 */

#include "format.h"
#include <ctime>
#include <iostream>
#include <memory>
#include <string>

namespace rs::logging {

enum class Level : unsigned {
  DEBUG = 0,
  INFO = 10,
  WARN = 20,
  ERROR = 30,
  CRITICAL = 40,
};

inline std::string to_string(Level level) {
  switch (level) {
  case Level::DEBUG:
    return "DEBUG";
  case Level::INFO:
    return "INFO";
  case Level::WARN:
    return "WARN";
  case Level::ERROR:
    return "ERROR";
  case Level::CRITICAL:
    return "CRITICAL";
  default:
    return "UNKNOWN";
  };
}

class Logger {

public:
  std::string name;
  Level threshold;

  explicit Logger(const std::string &name, Level l = Level::INFO)
      : name(name), threshold(l) {}

  template <typename... Args> void debug(Args &&...args) const {
    log(Level::DEBUG, std::forward<Args>(args)...);
  }

  template <typename... Args> void info(Args &&...args) const {
    log(Level::INFO, std::forward<Args>(args)...);
  }

  template <typename... Args> void warn(Args &&...args) const {
    log(Level::WARN, std::forward<Args>(args)...);
  }

  template <typename... Args> void error(Args &&...args) const {
    log(Level::ERROR, std::forward<Args>(args)...);
  }

  template <typename... Args> void critical(Args &&...args) const {
    log(Level::CRITICAL, std::forward<Args>(args)...);
  }

private:
  std::string now_str() const {
    std::string buffer(128, '\0');
    auto now = std::time(nullptr);
    auto now_str_length = std::strftime(buffer.data(), buffer.size(), "%F %T",
                                        std::localtime(&now));
    return buffer.substr(0, now_str_length);
  }

  template <typename... FormatArgs>
  void log(Level level, const std::string &fmt, FormatArgs &&...args) const {
    if (level >= threshold) {
      std::cerr << rs::format("[{}] {}:{}: ", now_str(), to_string(level), name)
                << rs::format(fmt, std::forward<FormatArgs>(args)...) << '\n';
    }
  }
};

template <typename... Args> inline auto make_logger(Args &&...args) {
  return std::make_unique<Logger>(std::forward<Args>(args)...);
}

} // namespace rs::logging

#endif // INCLUDED_RISKSERVICE_LOGGING_HEADER
