#include <iostream>
#include <format>
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <ranges>
#include <fstream>

#define NOMINMAX
#include "windows.h"

// https://docs.microsoft.com/en-us/windows/console/classic-vs-vt
// https://docs.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences

struct File {
  std::filesystem::path path;
  bool dir = false;
  bool non_empty = false;
  
  File(const std::filesystem::path& path): path(path) {
    try {
      dir = std::filesystem::is_directory(path);
      non_empty = !std::filesystem::is_empty(path);
    } catch (...) {}
  }

  File(const File& other) = default;
};

struct Dir {
  std::vector<File> paths;
  bool cached = false;
};

// find substring (case insensitive)
template<typename T>
bool contains_ci(const T& haystack, const T& needle)
{
  auto it = std::search(
    std::begin(haystack), std::end(haystack), 
    std::begin(needle), std::end(needle), 
    [](auto c1, auto c2) { return std::toupper(c1) == std::toupper(c2); });

  return it != std::end(haystack);
}

struct State {
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  std::filesystem::path path = "";
  std::vector<File> paths;
  size_t selected = 0;
  size_t last_height = 0;
  std::unordered_map<std::string, size_t> indexes;
  bool drives = false;
  int clear_lines_on_exit = 0;

  std::string query;
  
  bool show_caret = false;

  State(int clear_lines_on_exit)
    : clear_lines_on_exit(clear_lines_on_exit) {}

  std::unordered_map<std::string, Dir> dir_cache;
  Dir& open_dir(const std::filesystem::path& dir) {
    auto& cached = dir_cache[dir.string()];
    if (!cached.cached) {
      cached.cached = true;
      cached.paths.clear();
      try {
        std::error_code ec;
        for (auto& e : std::filesystem::directory_iterator(dir, 
            std::filesystem::directory_options::skip_permission_denied)) {
          cached.paths.push_back(e.path());
        }
      } catch (...) {
        std::cout << "exception!\n";
      }
      std::ranges::stable_sort(cached.paths, [](auto& l, auto& r) { return (l.dir && !r.dir); });
    }
    return cached;
  }

  void enter(std::filesystem::path p) {
    this->drives = drives;
    paths.clear();
    this->path = p;

    auto& dir = open_dir(p);
    for (auto& p2 : dir.paths)
      if (contains_ci(p2.path.filename().string(), query))
        paths.push_back(p2);

    auto f = indexes.find(p.string());
    if (f != indexes.end()) {
      selected = f->second;
      if (selected >= paths.size()) selected = -1;
    } else {
      selected = -1;
    }
    if (selected == -1) {
      selected = 0;
    }
  }

  bool is_dir(std::filesystem::path path) {
     try {
        return std::filesystem::is_directory(path);
      } catch (...) {}
      return false;
  }

  bool is_non_empty(std::filesystem::path path) {
    try {
      return is_dir(path) && !std::filesystem::is_empty(path);
    } catch (...) {}
    return false;
  }

  void draw() {
    CONSOLE_SCREEN_BUFFER_INFO inf;
    GetConsoleScreenBufferInfo(hOut, &inf);
    auto cols = inf.srWindow.Right - inf.srWindow.Left + 1;
    auto rows = inf.srWindow.Bottom - inf.srWindow.Top + 1;

    std::cout << "\x1B[?25l"; // Hide cursor
    std::cout << "\x1B[1G"; // Reset to start of line
    if (last_height > 0)
      std::cout << "\x1B[" << last_height << "A";

    std::cout << "\x1B[" << (1 + last_height) << "M"; // Clear lines!

    std::cout << "\x1B[4;32m" << path.string() 
      << (path.string().ends_with("\\") ? "" : "\\")
      << "\x1B[0m\n";

    size_t before = 10;
    size_t max = 21;

    auto start = selected >= before ? selected - before : 0;
    auto end = (std::min)(start + max, paths.size());
    last_height = 1;

    for (auto i{start}; i < end; ++i) {
      auto& p = paths[i];

      using namespace std::string_literals;
      std::cout << ((i == selected) ? ">> \x1B[4m" : "   ");

      std::cout << (p.dir && p.non_empty ? "\x1B[33m" : "\x1B[39m");

      auto parent_path = p.path.parent_path();
      auto parent = parent_path.string();
      if (parent.length() + 2 > cols) parent.resize(cols - 2);
      
      auto name = p.path.filename().string();
      if (name.empty()) name = parent;
      else if (name.length() + 4 > cols) name.resize(cols - 4);

      bool is_parent = path.has_parent_path() && p.path == path.parent_path();

      if (is_parent) {
        std::cout << "..";
      } else {
        std::cout << name;
        if (p.dir && !name.ends_with("\\")) std::cout << "\\";
      }
      std::cout << "\x1b[0m\n";
      last_height++;

      if (!drives && !is_parent && parent_path != path) {
        std::cout << "\x1B[2m"" " << parent << "\n\x1B[0m";
        last_height++;
      }
    }

    std::cout << "? " << query;
    std::cout << "\x1B[?25h"; // Show cursor
  }

  void clearExtra(int lines)
  {
    if (lines < 0) {
      for (int i = 0; i < lines; i++) {
        std::cout << "\n";
      }
    } else if (lines > 0) {
      std::cout << "\x1B[1G"; // Reset to start of line
      std::cout << "\x1B[" << lines << "A";
      std::cout << "\x1B[" << (lines + 1) << "M"; // Clear lines!
    }
  }

  void clear() {
    std::cout << "\x1B[1G"; // Reset to start of line
    if (last_height > 0)
      std::cout << "\x1B[" << last_height << "A";

    std::cout << "\x1B[" << (1 + last_height) << "M"; // Clear lines!
  }

  void leave() {
    if (!path.empty()) {
      auto current_path = path;
      query.clear();
      if (path.has_parent_path()) {
        enter(path.parent_path());
        for (size_t i = 0; i < paths.size(); ++i) {
          if (paths[i].path == current_path) {
            selected = i;
            break;
          }
        }
        draw();
      }
    }
  }

  void enter() {
    if (!paths.empty()) {
      auto selected = paths[this->selected];
      if (selected.dir && selected.non_empty) {
        query.clear();
        enter(selected.path);
        draw();
      }
    }
  }

  void return_to_current(const std::filesystem::path& cd_output) {
    std::ofstream out(cd_output, std::ios::out);
    if (out) {
      out << ".";
      out.flush();
      out.close();
    }

    clear();
    clearExtra(clear_lines_on_exit);
  }

  void open(const std::filesystem::path& cd_output) {
    std::ofstream out(cd_output, std::ios::out);
    if (out) {
      if (query.empty()) {
        out << path.string();
      } else {
        auto selected = paths[this->selected];
        if (selected.dir) {
          out << selected.path.string();
        } else {
          out << path.string();
        }
      }
      out.flush();
      out.close();
    }

    clear();
    clearExtra(clear_lines_on_exit);
  }

  void prev() {
    if (selected > 0) {
      selected--;
      indexes[path.string()] = selected;
      draw();
    }
  }

  void next() {
    if (selected + 1 < paths.size()) {
      selected++;
      indexes[path.string()] = selected;
      draw();
    }
  }
};

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "Expected temp output file location!\n";
    return 1;
  }

  auto cd_output = std::filesystem::path(argv[2]);

  HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  DWORD dwOriginalOutMode;
  GetConsoleMode(hOut, &dwOriginalOutMode);
  DWORD flags = dwOriginalOutMode;
  flags |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  flags |= DISABLE_NEWLINE_AUTO_RETURN;
  SetConsoleMode(hOut, flags);
  SetConsoleCP(CP_UTF8);

  auto state = State{std::stoi(argv[1])};
  state.enter(std::filesystem::current_path());

  state.clearExtra(1);
  state.draw();

  INPUT_RECORD inputBuffer[1];
  INPUT_RECORD& input = inputBuffer[0];
  DWORD inputsRead;
  while (ReadConsoleInput(hConsole, inputBuffer, 1, &inputsRead)) {
    if (inputsRead == 0) {
      std::cout << "No inputs read!\n";
      return 0;
    }
    if (input.EventType == KEY_EVENT) {
      KEY_EVENT_RECORD& e = input.Event.KeyEvent;
      if (e.bKeyDown) {
        if (e.wVirtualKeyCode == VK_UP 
            || (e.wVirtualKeyCode == VK_TAB && (e.dwControlKeyState & SHIFT_PRESSED))
            || (e.uChar.AsciiChar == 'k' && (e.dwControlKeyState & LEFT_CTRL_PRESSED))) {
          state.prev();
        } else if (e.wVirtualKeyCode == VK_DOWN 
            || (e.wVirtualKeyCode == VK_TAB && !(e.dwControlKeyState & SHIFT_PRESSED))
            || (e.uChar.AsciiChar == 'j' && (e.dwControlKeyState & LEFT_CTRL_PRESSED))) {
          state.next();
        } else if (e.wVirtualKeyCode == VK_LEFT 
            || (e.wVirtualKeyCode == VK_BACK
              && state.query.empty()
              && !(e.dwControlKeyState & SHIFT_PRESSED))
            || (e.uChar.AsciiChar == 'h' && (e.dwControlKeyState & LEFT_CTRL_PRESSED))) {
          state.leave();
        } else if (e.wVirtualKeyCode == VK_RIGHT 
            || e.uChar.AsciiChar == '\\' || e.uChar.AsciiChar == '/'
            || (e.uChar.AsciiChar == 'l' && (e.dwControlKeyState & LEFT_CTRL_PRESSED))) {
          state.enter();
        } else if (e.wVirtualKeyCode == VK_RETURN) {
          state.open(cd_output);
          return 0;
        } else if (e.wVirtualKeyCode == VK_ESCAPE) {
          state.return_to_current(cd_output);
          return 0;
        } else if (e.uChar.AsciiChar == ':') {
          if (!state.query.empty()) {
            state.query[0] = std::toupper(state.query[0]);
            auto path = std::filesystem::path(state.query +":\\");
            state.query.clear();
            if (std::filesystem::exists(path)) {
              state.enter(path);
            }
            state.draw();
          }
        } else if (e.uChar.AsciiChar >= ' ' && e.uChar.AsciiChar <= '~') {
          char c = e.uChar.AsciiChar;
          state.query += c;
          state.enter(state.path);
          state.draw();
        } else if (e.wVirtualKeyCode == VK_BACK) {
          if (!state.query.empty()) {
            if (e.dwControlKeyState & SHIFT_PRESSED) {
              state.query.clear();
            } else {
              state.query.resize(state.query.length() - 1);
            }
            state.enter(state.path);
            state.draw();
          }
        }
      }
    }
  }

  std::cout << "Error: " << GetLastError() << '\n';
}
