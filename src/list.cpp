#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>

int terminal_width() {
  if (_isatty(_fileno(stdout)) != 0) {
    return 80;
  }

  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  return csbi.srWindow.Right - csbi.srWindow.Left + 1;
}

#else
#include <sys/ioctl.h>
#include <unistd.h>

int terminal_width() {
  if (isatty(STDOUT_FILENO) == 0) {
    return 80;
  }

  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  return w.ws_col;
}

#endif

namespace fsys = std::filesystem;

constexpr std::string_view print_size_padding = " ";
constexpr std::string_view print_name_padding = " | ";
constexpr int max_name_length = 30;
constexpr int max_column_count = 5;

struct FileData {
  std::string name;
  uintmax_t size;
  std::string formatted_size;
};

struct Flags {
  bool show_hidden = false;
  bool show_size = false;
  bool show_help = false;
};

void print_usage() {
  std::cout << "Usage: ls [-sph] [DIRECTORY]\nUse `-h` for help" << std::endl;
}

void print_help() {
  std::cout << "Flags:" << std::endl;
  std::cout << "  -s: show file sizes" << std::endl;
  std::cout << "  -p: show hidden files" << std::endl;
}

std::string truncate(const std::string& s,
                     std::size_t max_len = max_name_length) {
  if (s.size() <= max_len) return s;
  return s.substr(0, max_len - 3) + "...";
}

int file_count(const std::vector<FileData>& data, const Flags& optn) {
  if (optn.show_hidden) {
    return data.size();
  }

  int count;
  std::for_each(data.begin(), data.end(), [&count](const FileData& file) {
    if (file.name[0] == '.') {
      count++;
    }
  });
  return count;
}

std::string trim_path(const std::string& path) {
  std::size_t last = path.find_last_of("/\\");
  return path.substr(last + 1);
}

std::uintmax_t directory_size(const fsys::directory_entry& dir) {
  std::uintmax_t size = 0;

  for (const fsys::directory_entry& entry :
       fsys::recursive_directory_iterator(dir.path())) {
    if (fsys::is_regular_file(entry.path())) {
      size += entry.file_size();
    }
  }

  return size;
}

std::string format_size(uintmax_t size) {
  int order = 0;

  for (int i = 0; i < 4; i++) {
    if (size / 1000 < 1) {
      break;
    } else {
      size /= 1000;
      order++;
    }
  }

  std::string formatted = std::to_string(size);

  switch (order) {
    case 0:
      formatted += "B ";
      break;
    case 1:
      formatted += "KB";
      break;
    case 2:
      formatted += "MB";
      break;
    case 3:
      formatted += "GB";
      break;
  }

  return formatted;
}

void print_file_data(const std::vector<FileData>& data, const Flags& optn) {
  int colcount = 1;
  int termwidth = terminal_width();
  int nameslength = 0, sizeslength = 0;
  std::vector<FileData> printed_data;

  std::for_each(data.begin(), data.end(),
                [&printed_data, optn](const FileData& file) {
                  if (!optn.show_hidden && file.name[0] == '.') {
                    return;
                  }
                  printed_data.push_back(file);
                });

  for (const FileData& file : printed_data) {
    if (file.name.size() > nameslength) {
      nameslength = std::clamp((int)file.name.size(), 0, max_name_length);
    }
    if (optn.show_size) {
      if (file.formatted_size.size() > sizeslength) {
        sizeslength = file.formatted_size.size();
      }
    }
  }

  int possible_colcount = std::clamp(
      termwidth / (int)(nameslength + sizeslength + print_size_padding.size() +
                        print_name_padding.size()),
      1, max_column_count);  // Account for spacing between columns
  int file_count = printed_data.size();
  if (file_count >
      possible_colcount * 2) {  // Want at least two rows of files, otherwise
                                // don't split them into columns
    colcount = possible_colcount;
  }

  for (int i = 1; i < file_count; i++) {
    if (optn.show_size) {
      std::cout << std::left << std::setw(sizeslength)
                << format_size(printed_data[i].size) << print_size_padding;
    }
    std::cout << std::left << std::setw(nameslength)
              << truncate(printed_data[i].name);
    if (i % colcount == 0) {
      if (i != file_count - 1) {
        std::cout << std::endl;
      }
    } else {
      std::cout << print_name_padding;
    }
  }
  std::cout << std::endl;
}

int main(int argc, char* argv[]) {
  if (argc > 3) {
    print_usage();
    return 1;
  }

  Flags optn;
  std::string dir;
  bool flags_set = false;
  bool dir_set = false;

  // Collect command arguments
  for (size_t i = 1; i < argc; i++) {
    if (*argv[i] == '-' && !flags_set) {
      for (argv[i]; *argv[i]; argv[i]++) {
        switch (*argv[i]) {
          case 's':
            optn.show_size = true;
            break;
          case 'p':
            optn.show_hidden = true;
            break;
          case 'h':
            optn.show_help = true;
            break;
        }
      }
      flags_set = true;
    } else if (!dir_set) {
      dir = argv[i];
      dir_set = true;
    } else {
      print_usage();
      return 1;
    }
  }

  if (optn.show_help) {
    print_help();
    return 0;
  }

  if (!dir_set) {
    dir = fsys::current_path();
  }

  try {
    fsys::directory_iterator it(dir);
  } catch (const fsys::filesystem_error& e) {
    std::cout << "Failed to open directory " << dir << ", make sure it exists"
              << std::endl;
    return 1;
  }

  // Collect file data
  std::vector<FileData> files;
  for (const fsys::directory_entry& entry : fsys::directory_iterator(dir)) {
    FileData file;
    file.name = trim_path(entry.path().string());
    if (entry.is_directory()) {
      file.size = directory_size(entry);
    } else {
      file.size = entry.file_size();
    }

    file.formatted_size = format_size(file.size);
    files.push_back(file);
  }

  // Process file data
  std::sort(files.begin(), files.end(),
            [](const FileData& fileone, const FileData& filetwo) {
              return fileone.name < filetwo.name;
            });

  print_file_data(files, optn);

  /*
  std::for_each(files.begin(), files.end(), [optn](const FileData& fd) {
      print_file_data(fd, optn);
  });
  */

  return 0;
}
