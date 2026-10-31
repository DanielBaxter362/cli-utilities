#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <regex>
#include <vector>
#include <iterator>
#include <string>

constexpr std::size_t chunkSize = 16000;

namespace fsys = std::filesystem;

struct Flags {
  int recurse_depth = -1;
};

struct Match {
  std::string line;
  int line_number;
};

void print_usage(){std::cout << "Usage: grep [FLAGS] PATTERN [FILE]" << std::endl;}

bool is_readable(const std::string& contents) {
  int length = std::min(contents.size(), chunkSize);
  return std::memchr(contents.data(), '\0', length) == nullptr;
}

void print_matches(const std::vector<Match>& matches) {
  for (const Match& match : matches) {
    auto start = match.line.find_first_not_of(" \t");
    auto end = match.line.find_last_not_of(" \t");
    std::cout << match.line_number << "  " << match.line.substr(start, end - start) << std::endl;
  }
}

int main(int argc, char* argv[]) {
  if (argc > 4 || argc < 2) {
    print_usage();
    return 1;
  }

  Flags optn;
  std::string path;

  int argnum = 1;
  for (int i = 1; i < argc && argv[i][0] == '-'; i++) {
    // Flag handling
    
    argnum++;
    break;
  }

  int args = argc - argnum; // Number of arguments not including flags & grep itself
  
  std::regex pattern(argv[argnum++]);
  if (args > 1) { path = argv[argnum++]; }

  if (path.empty()) {

  } else {
    std::error_code ec;
    fsys::directory_entry file{path, ec};
    
    if (ec) {
      std::cout << "Error opening file " << path << std::endl;
      return 1;
    }
    
    if (!file.exists()) {
      std::cout << "Error: file " << path << " does not exist" << std::endl;
      return 1;
    }

    if (file.is_regular_file()) {
      std::ifstream filestream{path};

      if (!filestream) {
        std::cout << "Error opening file with path: " << path << std::endl;
        return 1;
      }

      std::string contents(chunkSize, '\0');
      filestream.read(contents.data(), chunkSize);
      contents.resize(filestream.gcount());

      if (!is_readable(contents)) {
        std::cout << "Error: non-readable binary provided" << std::endl;
        return 1;
      }

      std::size_t line_number = 1;
      std::string remainder;
      std::vector<Match> matches;

      while (filestream.gcount() > 0) {
        auto left = contents.begin();

        while (left != contents.end()) {
          auto right = std::find(left, contents.end(), '\n');

          if (right == contents.end()) {
            remainder.assign(left, right);
            break;
          }

          if (std::regex_search(left, right, pattern)) {
            matches.push_back({std::string(left, right), line_number});
          }
          line_number++;
          left = right + 1;
        }

        contents.resize(remainder.size() + chunkSize);
        std::copy(remainder.begin(), remainder.end(), contents.begin());
        filestream.read(contents.data() + remainder.size(), chunkSize);
        contents.resize(remainder.size() + filestream.gcount());
        remainder.clear();
      }

      if (!contents.empty() &&
          std::regex_search(contents.begin(), contents.end(), pattern)) {
        matches.push_back({contents, line_number});
      }

      print_matches(matches);
    } else if (file.is_directory()) {
      for (const fsys::directory_entry& file :
           fsys::recursive_directory_iterator{path}) {
      }
    } else {
      std::cout << "Error: file type not recognised" << std::endl;
      return 1;
    }
  }
}
