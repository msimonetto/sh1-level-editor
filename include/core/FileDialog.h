#pragma once
#include <string>

class FileDialog {
public:
  // Filter format: "Description\0*.ext\0All Files\0*.*\0"
  static std::string OpenFile(const char *filter);
  static std::string OpenDirectory(const char *title = "Select Directory");
};
