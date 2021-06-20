#ifndef INCLUDED_RISKSERVICE_FORMAT_HEADER
#define INCLUDED_RISKSERVICE_FORMAT_HEADER
/*
 * Simple replacement for fmtlib.
 */

#include <sstream>
#include <string>

namespace rs {

// to_string with no-op overloads for string-like types.
template <typename T> inline std::string to_string(const T &val) {
  return std::to_string(val);
}
inline std::string to_string(char *val) { return val; }
inline std::string to_string(const char *val) { return val; }
inline std::string to_string(const std::string &val) { return val; }
inline std::string to_string(std::string &val) { return val; }

// Base case, format string with no arguments.
inline std::string format(const std::string &fmt) { return fmt; }

// Recursive string formatter.
template <typename T, typename... Args>
inline std::string format(const std::string &fmt, const T &arg,
                          const Args &...rest) {
  auto field_begin = fmt.find("{}");
  if (field_begin == std::string::npos) {
    throw std::runtime_error("Invalid format string: mismatching amount of "
                             "replacement fields and format args");
  }
  // Construct new string by converting one format arg and call format
  // recursively for rest.
  std::stringstream out;
  out << fmt.substr(0, field_begin) << to_string(arg)
      << format(fmt.substr(field_begin + 2), rest...);
  return out.str();
}

} // namespace rs

#endif // INCLUDED_RISKSERVICE_FORMAT_HEADER
