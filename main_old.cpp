// #include <iostream>
// #include <format>
// #include <filesystem>
// #include <algorithm>
// #include <unordered_map>
// #include <ranges>
// #include "PathTree.hpp"

// #define NOMINMAX
// #include "windows.h"

// // https://docs.microsoft.com/en-us/windows/console/classic-vs-vt
// // https://docs.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences

// struct File {
//   std::filesystem::path path;
//   bool dir = false;
//   bool non_empty = false;
  
//   File(const std::filesystem::path& path): path(path) {
//     try {
//       dir = std::filesystem::is_directory(path);
//     } catch (...) {}

//     try {
//       non_empty = !std::filesystem::is_empty(path);
//     } catch (...) {}
//   }
// };

// // find substring (case insensitive)
// template<typename T>
// bool contains_ci(const T& haystack, const T& needle)
// {
//   auto it = std::search(
//     std::begin(haystack), std::end(haystack), 
//     std::begin(needle), std::end(needle), 
//     [](auto c1, auto c2) { return std::toupper(c1) == std::toupper(c2); });

//   return it != std::end(haystack);
// }

// struct State {
//   std::filesystem::path path = "";
//   // std::vector<std::filesystem::path> paths;
//   std::vector<File> paths;
//   size_t selected = 0;
//   size_t last_height = 0;
//   std::unordered_map<std::string, size_t> indexes;
//   bool drives = false;

//   std::string query;
  
//   bool show_caret = false;

//   void enter(std::filesystem::path p, bool drives = false) {
//     this->drives = drives;
//     paths.clear();
//     if (drives) {
//       this->path = (p = "");
//       paths.push_back(File("C:\\"));
//       paths.push_back(File("D:\\"));
//     } else {
//       this->path = p;
//       try {
//         std::error_code ec;
//         for (auto& e : std::filesystem::directory_iterator(p, 
//             std::filesystem::directory_options::skip_permission_denied)) {
//           if (contains_ci(e.path().filename().string(), query))
//             paths.push_back(e.path());
//         }
//         // paths.push_back(p);
//       } catch (...) {
//         std::cout << "exception!\n";
//       }
//       // paths.pop_back();
//       // paths.insert(paths.begin(), File(p));
//       std::ranges::stable_sort(paths, [](auto& l, auto& r) { return (l.dir && !r.dir); });
//     }
//     auto f = indexes.find(p.string());
//     if (f != indexes.end()) {
//       selected = f->second;
//       if (selected >= paths.size()) selected = -1;
//     } else {
//       selected = -1;
//     }
//     if (selected == -1) {
//       // selected = std::min(paths.size() / 2ull, 2ull);
//       // selected = std::min(paths.size() - 1, 1ull);
//       selected = 0;
//     }
//   }

//   bool is_dir(std::filesystem::path path) {
//      try {
//         return std::filesystem::is_directory(path);
//       } catch (...) {}
//       return false;
//   }

//   bool is_non_empty(std::filesystem::path path) {
//     // std::cout << "Checking path: " << path;
//     try {
//       return is_dir(path) && !std::filesystem::is_empty(path);
//     } catch (...) {}
//     return false;
//   }

//   void draw(HANDLE hOut) {
//     CONSOLE_SCREEN_BUFFER_INFO inf;
//     GetConsoleScreenBufferInfo(hOut, &inf);
//     auto cols = inf.srWindow.Right - inf.srWindow.Left + 1;
//     auto rows = inf.srWindow.Bottom - inf.srWindow.Top + 1;

//     std::cout << "\x1B[?25l"; // Hide cursor
//     std::cout << "\x1B[1G"; // Reset to start of line
//     if (last_height > 0)
//       std::cout << "\x1B[" << last_height << "A";
//     // std::cout << "\x1B[u"; // Restore cursor
//     // std::cout << "\x1B[s"; // Save cursor position

//     std::cout << "\x1B[" << (1 + last_height) << "M"; // Clear lines!

//     std::cout << "\x1B[4;32m" << path.string() 
//       << (path.string().ends_with("\\") ? "" : "\\")
//       << "\x1B[0m\n";

//     // size_t before = 4;
//     // size_t max = 9;
//     // size_t before = 7;
//     // size_t max = 15;
//     size_t before = 10;
//     size_t max = 21;

//     auto start = selected >= before ? selected - before : 0;
//     auto end = (std::min)(start + max, paths.size());
//     // last_height = 1 + (end - start) * (show_paths ? 2 : 1);
//     last_height = 1;

//     for (auto i{start}; i < end; ++i) {
//       auto& p = paths[i];

//       using namespace std::string_literals;
//       std::cout << ((i == selected) ? ">> \x1B[4m" : "   ");

//       std::cout << (p.dir && p.non_empty ? "\x1B[33m" : "\x1B[39m");

//       auto parent_path = p.path.parent_path();
//       auto parent = parent_path.string();
//       if (parent.length() + 2 > cols) parent.resize(cols - 2);
      
//       auto name = p.path.filename().string();
//       if (name.empty()) name = parent;
//       else if (name.length() + 4 > cols) name.resize(cols - 4);

//       if (p.path == path) {
//         std::cout << ".";
//       } else {
//         std::cout << name;
//         if (p.dir && !name.ends_with("\\")) std::cout << "\\";
//       }
//       std::cout << "\x1b[0m\n";
//       last_height++;

//       if (!drives && parent_path != path) {
//         std::cout << "\x1B[2m"" " << parent << "\n\x1B[0m";
//         last_height++;
//       }
//     }
//     // if (show_caret)

//     std::cout << "? " << query;
//     std::cout << "\x1B[?25h"; // Show cursor
//   }

//   void clear() {
//     // std::cout << "\x1B[?25l"; // Hide cursor
//     std::cout << "\x1B[1G"; // Reset to start of line
//     if (last_height > 0)
//       std::cout << "\x1B[" << last_height << "A";

//     std::cout << "\x1B[" << (1 + last_height) << "M"; // Clear lines!
//   }
// };

// int main2() {
//   HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
//   HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
//   DWORD dwOriginalOutMode;
//   GetConsoleMode(hOut, &dwOriginalOutMode);
//   DWORD flags = dwOriginalOutMode;
//   flags |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
//   flags |= DISABLE_NEWLINE_AUTO_RETURN;
//   SetConsoleMode(hOut, flags);
//   SetConsoleCP(CP_UTF8);

//   // std::cout << "\x1B[s"; // Save cursor position

//   auto state = State{};
//   state.enter(std::filesystem::current_path());

//   // using namespace std::chrono;
//   // auto start = steady_clock::now();
//   // auto tree = PathTree::load(getenv("USERPROFILE") + std::string("\\.nms\\tree.bin"));
//   // auto treeLoad = duration_cast<milliseconds>(steady_clock::now() - start);
//   // start = steady_clock::now();
//   // auto view = tree.next(nullptr);
//   // state.path = std::filesystem::current_path();
//   // size_t skip = 0;
//   // while (view && state.paths.size() < 500) {
//   //   if ((skip++ > 100'000) && !view.value().path().string().starts_with("C:\\$Recycle.Bin")) {
//   //     state.paths.push_back(view.value().path());
//   //   }
//   //   // std::cout << "path = " << view.value().path() << '\n';

//   //   view = tree.next(&view.value());
//   // }
//   // auto stateLoad = duration_cast<milliseconds>(steady_clock::now() - start);

//   state.draw(hOut);

//   // std::cout << "tree = " << treeLoad << " state = " << stateLoad;

//   INPUT_RECORD inputBuffer[1];
//   INPUT_RECORD& input = inputBuffer[0];
//   DWORD inputsRead;
//   while (ReadConsoleInput(hConsole, inputBuffer, 1, &inputsRead)) {
//     if (inputsRead == 0) {
//       std::cout << "No inputs read!\n";
//       return 0;
//     }
//     if (input.EventType == KEY_EVENT) {
//       KEY_EVENT_RECORD& e = input.Event.KeyEvent;
//       // std::cout << "key event, key = " << e.wVirtualKeyCode << '\n';
//       if (e.bKeyDown) {
//         if (e.wVirtualKeyCode == VK_UP 
//             || (e.wVirtualKeyCode == VK_TAB && (e.dwControlKeyState & SHIFT_PRESSED))) {
//           if (state.selected > 0) {
//             state.selected--;
//             state.indexes[state.path.string()] = state.selected;
//             state.draw(hOut);
//             // std::cout << "Down!";
//           }
//         } else if (e.wVirtualKeyCode == VK_DOWN 
//             || (e.wVirtualKeyCode == VK_TAB && !(e.dwControlKeyState & SHIFT_PRESSED))) {
//           if (state.selected + 1 < state.paths.size()) {
//             state.selected++;
//             state.indexes[state.path.string()] = state.selected;
//             state.draw(hOut);
//             // std::cout << "Up!";
//           }
//         } else if (e.wVirtualKeyCode == VK_LEFT 
//             || (e.wVirtualKeyCode == VK_BACK
//               && state.query.empty()
//               && !(e.dwControlKeyState & SHIFT_PRESSED))) {
//           // std::cout << "going left from " << state.path.string() << " -> " << (state.path == state.path.parent_path());
//           // std::this_thread::sleep_for(std::chrono::milliseconds(1000));

//           if (!state.path.empty()) {
//             auto path = state.path;
//             state.query.clear();
//             state.enter(path.parent_path(), path.parent_path() == path);
//             for (size_t i = 0; i < state.paths.size(); ++i) {
//               if (state.paths[i].path == path) {
//                 state.selected = i;
//                 break;
//               }
//             }
//             state.draw(hOut);
//           }
//         } else if (e.wVirtualKeyCode == VK_RIGHT 
//             || e.uChar.AsciiChar == '\\' || e.uChar.AsciiChar == '/') {
//           if (!state.paths.empty()) {
//             auto selected = state.paths[state.selected];
//             if (selected.dir && selected.non_empty) {
//               state.query.clear();
//               state.enter(selected.path);
//               state.draw(hOut);
//             }
//           }
//         } else if (e.wVirtualKeyCode == VK_RETURN) {
//           if (!state.paths.empty()) {
//             auto selected = state.paths[state.selected];
//             // std::ofstream out("C:\\Users\\Darian\\.nms\\cd", std::ios::out);
//             std::ofstream out("C:\\.nms\\cd", std::ios::out);
//             if (out) {
//               if (selected.dir) {
//                 out << selected.path.string();
//               } else {
//                 out << state.path.string();
//               }
//             }
//             out.flush();
//             out.close();
//           } else {
//             // std::filesystem::remove("C:/Users/Darian/.nms/cd");
//             std::filesystem::remove("C:\\.nms\\cd");
//           }
//           state.clear();
//           return 0;
//         } else if (e.wVirtualKeyCode == VK_ESCAPE) {
//           state.clear();
//           return 0;
//         } else if (e.uChar.AsciiChar >= ' ' && e.uChar.AsciiChar <= '~') {
//           char c = e.uChar.AsciiChar;
//           state.query += c;
//           state.enter(state.path);
//           state.draw(hOut);
//         } else if (e.wVirtualKeyCode == VK_BACK) {
//           if (!state.query.empty()) {
//             if (e.dwControlKeyState & SHIFT_PRESSED) {
//               state.query.clear();
//             } else {
//               state.query.resize(state.query.length() - 1);
//             }
//             state.enter(state.path);
//             state.draw(hOut);
//           }
//         }
//       }
//     }
//   }

//   std::cout << "Error: " << GetLastError() << '\n';

//   return 0;
// }
