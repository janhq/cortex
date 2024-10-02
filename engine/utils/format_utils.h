#pragma once

#include <yaml-cpp/yaml.h>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
namespace format_utils {
constexpr char RESET[] = "\033[0m";
constexpr char BOLD[] = "\033[1m";
constexpr char GREEN[] = "\033[1;32m";
constexpr char YELLOW[] = "\033[0;33m";
constexpr char BLUE[] = "\033[0;34m";
constexpr char MAGENTA[] = "\033[0;35m";
constexpr char GRAY[] = "\033[1;90m";

inline std::string print_comment(const std::string& comment) {
  std::ostringstream oss;
  oss << GRAY << "# " << comment << RESET << "\n";
  return oss.str();
};

inline std::string print_kv(const std::string& key, const std::string& value,
                            const std::string& color = "\033[0m") {
  std::ostringstream oss;
  oss << GREEN << key << ":" << RESET << " " << color << value << RESET << "\n";
  return oss.str();
};

inline std::string print_bool(const std::string& key, bool value) {
  return print_kv(key, value ? "true" : "false", MAGENTA);
};

inline std::string print_float(const std::string& key, float value) {
  if (!std::isnan(value)) {
    std::ostringstream float_oss;
    float_oss << std::fixed << std::setprecision(6) << value;
    std::string str_value = float_oss.str();
    // Remove trailing zeros
    str_value.erase(str_value.find_last_not_of('0') + 1, std::string::npos);
    // Remove trailing dot if present
    if (str_value.back() == '.') {
      str_value.pop_back();
    }
    return print_kv(key, str_value, BLUE);
  } else
    return "";
};
inline std::string writeKeyValue(const std::string& key,
                                 const YAML::Node& value,
                                 const std::string& comment = "") {
  std::ostringstream outFile;
  if (!value)
    return "";
  outFile << key << ": ";

  // Check if the value is a float and round it to 6 decimal places
  if (value.IsScalar()) {
    try {
      double doubleValue = value.as<double>();
      std::ostringstream oss;
      oss << std::fixed << std::setprecision(6) << doubleValue;
      std::string strValue = oss.str();
      // Remove trailing zeros after the decimal point
      strValue.erase(strValue.find_last_not_of('0') + 1, std::string::npos);
      if (strValue.back() == '.') {
        strValue.pop_back();
      }
      outFile << strValue;
    } catch (const std::exception& e) {
      outFile << value;  // If not a float, write as is
    }
  } else {
    outFile << value;
  }

  if (!comment.empty()) {
    outFile << " # " << comment;
  }
  outFile << "\n";
  return outFile.str();
};

inline std::string BytesToHumanReadable(uint64_t bytes) {
  const uint64_t KB = 1024;
  const uint64_t MB = KB * 1024;
  const uint64_t GB = MB * 1024;
  const uint64_t TB = GB * 1024;

  double result;
  std::string unit;

  if (bytes >= TB) {
    result = static_cast<double>(bytes) / TB;
    unit = "TB";
  } else if (bytes >= GB) {
    result = static_cast<double>(bytes) / GB;
    unit = "GB";
  } else if (bytes >= MB) {
    result = static_cast<double>(bytes) / MB;
    unit = "MB";
  } else if (bytes >= KB) {
    result = static_cast<double>(bytes) / KB;
    unit = "KB";
  } else {
    result = static_cast<double>(bytes);
    unit = "B";
  }

  std::ostringstream out;
  // take 2 decimal points
  out << std::fixed << std::setprecision(2) << result << " " << unit;
  return out.str();
}
}  // namespace format_utils
