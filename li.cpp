#if defined(WIN32)
#define LI_PLATFORM_WINDOWS
#else
#define LI_PLATFORM_LINUX
#endif

#if defined(LI_PLATFORM_WINDOWS)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#if defined(LI_PLATFORM_LINUX)
#include <sys/ioctl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <cassert>

// For tracking shift modifier state in a graphical environment with X11/XWayland available
#include <X11/XKBlib.h>
#include <poll.h>
#endif

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <ranges>
#include <fstream>
#include <vector>
#include <print>

using namespace std::literals;

// https://docs.microsoft.com/en-us/windows/console/classic-vs-vt
// https://docs.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences

namespace fs = std::filesystem;

struct File
{
    fs::path  path;
    bool       dir = false;
    bool non_empty = false;

    File(const fs::path& path)
        : path(path)
    {
        try {
            dir = is_directory(path);
            non_empty = !is_empty(path);
        }
        catch (...) {}
    }

    File(const File& other) = default;
};

struct Dir
{
    std::vector<File> paths;
    bool              cached = false;
};

// find substring (case insensitive)
template<typename T>
bool CaseInsensitiveContains(const T& haystack, const T& needle)
{
    auto it = std::search(
        std::begin(haystack), std::end(haystack),
        std::begin(needle), std::end(needle),
        [](auto c1, auto c2) {
            return std::toupper(c1) == std::toupper(c2);
        });

    return it != std::end(haystack);
}

struct State
{
#if defined(LI_PLATFORM_WINDOWS)
    HANDLE       out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
    fs::path           path = "";
    std::vector<File> paths;
    size_t         selected = 0;
    size_t      last_height = 0;
    bool             drives = false;
    int clear_lines_on_exit = 0;
    bool         show_caret = false;
    std::string       query;

    std::unordered_map<std::string, size_t> indexes;
    std::unordered_map<std::string, Dir>  dir_cache;

    State(const int clear_lines_on_exit)
        : clear_lines_on_exit(clear_lines_on_exit)
    {}

    Dir& OpenDir(const fs::path&dir)
    {
        auto& cached = dir_cache[dir.string()];
        if (!cached.cached) {
            cached.cached = true;
            cached.paths.clear();
            try {
                for (auto&e: fs::directory_iterator(dir,
                        fs::directory_options::skip_permission_denied)) {
                    cached.paths.push_back(e.path());
                }
            }
            catch (...) {
                std::cout << "exception!\n";
            }
            std::ranges::stable_sort(cached.paths, [](auto&l, auto&r) {
                return (l.dir && !r.dir);
            });
        }
        return cached;
    }

    void Enter(const fs::path& p)
    {
        paths.clear();
        this->path = p;

        for (const auto& dir = OpenDir(p); auto& p2: dir.paths) {
            if (CaseInsensitiveContains(p2.path.filename().string(), query)) {
                paths.push_back(p2);
            }
        }

        if (const auto f = indexes.find(p.string()); f != indexes.end()) {
            selected = f->second;
            if (selected >= paths.size()) {
                selected = ~0ull;
            }
        } else {
            selected = ~0ull;
        }

        if (selected == ~0ull) {
            selected = 0;
        }

        Draw();
    }

    void UpdateResults()
    {
        Enter(path);
    }

    static bool IsDir(const fs::path&path)
    {
        try {
            return is_directory(path);
        }
        catch (...) {}
        return false;
    }

    static bool IsNonEmpty(const fs::path& path)
    {
        try {
            return IsDir(path) && !is_empty(path);
        }
        catch (...) {}
        return false;
    }

    void Draw()
    {
#if defined(LI_PLATFORM_WINDOWS)
        CONSOLE_SCREEN_BUFFER_INFO inf;
        GetConsoleScreenBufferInfo(out_handle, &inf);
        const auto cols = inf.srWindow.Right - inf.srWindow.Left + 1;
#endif

#if defined(LI_PLATFORM_LINUX)
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        const auto cols = w.ws_col;
#endif

        std::cout << "\x1B[?25l"; // Hide cursor
        std::cout << "\x1B[1G"; // Reset to start of line
        if (last_height > 0) {
            std::cout << "\x1B[" << last_height << "A";
        }

        std::cout << "\x1B[" << (1 + last_height) << "M"; // Clear lines!

        std::cout << "\x1B[4;32m" << path.string()
                << (path.generic_string().ends_with("/") ? "" : "/")
                << "\x1B[0m\n";

        constexpr size_t before = 10;
        constexpr size_t max = 21;

        const auto start = selected >= before ? selected - before : 0;
        const auto end = (std::min)(start + max, paths.size());
        last_height = 1;

        for (auto i{start}; i < end; ++i) {
            auto& p = paths[i];

            using namespace std::string_literals;
            std::cout << ((i == selected) ? ">> \x1B[4m" : "   ");

            std::cout << (p.dir && p.non_empty ? "\x1B[93m" : "\x1B[39m");

            auto parent_path = p.path.parent_path();
            auto parent = parent_path.string();
            if (parent.length() + 2 > cols) {
                parent.resize(cols - 2);
            }

            auto name = p.path.filename().string();
            if (name.empty()) {
                name = parent;
            } else if (name.length() + 4 > cols) {
                name.resize(cols - 4);
            }

            const bool is_parent = path.has_parent_path() && p.path == path.parent_path();

            if (is_parent) {
                std::cout << "..";
            } else {
                std::cout << name;
                if (p.dir && !name.ends_with("\\")) {
                    std::cout << "\\";
                }
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
        std::cout.flush();
    }

    static void ClearExtra(const int lines)
    {
        if (lines < 0) {
            for (int i = 0; i < lines; i++) {
                std::cout << "\n";
            }
        }
        else if (lines > 0) {
            std::cout << "\x1B[1G"; // Reset to start of line
            std::cout << "\x1B[" << lines << "A";
            std::cout << "\x1B[" << (lines + 1) << "M"; // Clear lines!
        }
    }

    void Clear() const
    {
        std::cout << "\x1B[1G"; // Reset to start of line
        if (last_height > 0) {
            std::cout << "\x1B[" << last_height << "A";
        }

        std::cout << "\x1B[" << (1 + last_height) << "M"; // Clear lines!
    }

    void Leave()
    {
        if (!path.empty()) {
            const auto current_path = path;
            query.clear();
            if (path.has_parent_path()) {
                Enter(path.parent_path());
                for (size_t i = 0; i < paths.size(); ++i) {
                    if (paths[i].path == current_path) {
                        selected = i;
                        break;
                    }
                }
                Draw();
            }
        }
    }

    void Enter()
    {
        if (!paths.empty()) {
            if (const auto selected_file = paths[this->selected]; selected_file.dir && selected_file.non_empty) {
                query.clear();
                Enter(selected_file.path);
                Draw();
            }
        }
    }

    void ReturnToCurrent(const fs::path& cd_output) const
    {
        std::ofstream out(cd_output, std::ios::out);
        if (out) {
            out << ".";
            out.flush();
            out.close();
        }

        Clear();
        ClearExtra(clear_lines_on_exit);
    }

    [[nodiscard]] bool Open(const fs::path& cd_output) const
    {
        fs::path target;
        if (query.empty()) {
            target = path.string();
        } else {
            if (selected >= paths.size()) return false;
            if (const auto selected_dir = paths[selected]; selected_dir.dir) {
                target = selected_dir.path.string();
            } else {
                target = path.string();
            }
        }
        if (!fs::exists(target)) return false;

        std::ofstream out(cd_output, std::ios::out);
        if (out) {
            out << target.string();
            out.flush();
            out.close();
        }

        Clear();
        ClearExtra(clear_lines_on_exit);

        return true;
    }

    void Prev()
    {
        if (selected > 0) {
            selected--;
            indexes[path.string()] = selected;
            Draw();
        }
    }

    void Next()
    {
        if (selected + 1 < paths.size()) {
            selected++;
            indexes[path.string()] = selected;
            Draw();
        }
    }
};

int main(const int argc, char** argv)
{
    if (argc != 3) {
        std::cerr << "Expected temp output file location!\n";
        return 1;
    }

    if (auto* extra_clears_str = std::getenv("LI_EXTRACLEARS")) {
        if (int extra_clears = std::stoi(extra_clears_str)) {
            std::cout << std::format("\x1b[{}M\x1b[0G\x1b[{}A", extra_clears + 1, extra_clears);
        }
    }

    const auto cd_output = fs::path(argv[2]);

#if defined(LI_PLATFORM_WINDOWS)
    const HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
    const HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwOriginalOutMode;
    GetConsoleMode(hOut, &dwOriginalOutMode);
    DWORD flags = dwOriginalOutMode;
    flags |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    flags |= DISABLE_NEWLINE_AUTO_RETURN;
    SetConsoleMode(hOut, flags);
    SetConsoleCP(CP_UTF8);
#endif

    auto state = State{std::stoi(argv[1])};
    state.ClearExtra(1);
    state.Enter(fs::current_path());

#if defined(LI_PLATFORM_WINDOWS)
    INPUT_RECORD inputBuffer[1];
    const INPUT_RECORD& input = inputBuffer[0];
    DWORD inputsRead;
    while (ReadConsoleInput(hConsole, inputBuffer, 1, &inputsRead)) {
        if (inputsRead == 0) {
            std::cout << "No inputs read!\n";
            return 0;
        }
        if (input.EventType == KEY_EVENT) {
            const KEY_EVENT_RECORD& e = input.Event.KeyEvent;
            if (e.bKeyDown) {
                if (e.wVirtualKeyCode == VK_UP
                        || (e.wVirtualKeyCode == VK_TAB && (e.dwControlKeyState & SHIFT_PRESSED))
                        || (e.uChar.AsciiChar == 'k' && (e.dwControlKeyState & LEFT_CTRL_PRESSED))) {
                    state.Prev();

                } else if (e.wVirtualKeyCode == VK_DOWN
                         || (e.wVirtualKeyCode == VK_TAB && !(e.dwControlKeyState & SHIFT_PRESSED))
                         || (e.uChar.AsciiChar == 'j' && (e.dwControlKeyState & LEFT_CTRL_PRESSED))) {
                    state.Next();

                } else if (e.wVirtualKeyCode == VK_LEFT
                         || (e.wVirtualKeyCode == VK_BACK
                             && state.query.empty()
                             && !(e.dwControlKeyState & SHIFT_PRESSED))
                         || (e.uChar.AsciiChar == 'h' && (e.dwControlKeyState & LEFT_CTRL_PRESSED))) {
                    state.Leave();

                } else if (e.wVirtualKeyCode == VK_RIGHT
                         || e.uChar.AsciiChar == '\\' || e.uChar.AsciiChar == '/'
                         || (e.uChar.AsciiChar == 'l' && (e.dwControlKeyState & LEFT_CTRL_PRESSED))) {
                    state.Enter();

                } else if (e.wVirtualKeyCode == VK_RETURN) {
                    if (state.Open(cd_output)) {
                        return 0;
                    }

                } else if (e.wVirtualKeyCode == VK_ESCAPE) {
                    state.ReturnToCurrent(cd_output);
                    return 0;

                } else if (e.uChar.AsciiChar == ':') {
                    if (!state.query.empty()) {
                        state.query[0] = char(std::toupper(state.query[0]));
                        auto path = fs::path(state.query + ":\\");
                        state.query.clear();
                        if (exists(path)) {
                            state.Enter(path);
                        }
                    }

                } else if (e.uChar.AsciiChar >= ' ' && e.uChar.AsciiChar <= '~') {
                    const char c = e.uChar.AsciiChar;
                    state.query += c;
                    state.OnQueryUpdate();

                } else if (e.wVirtualKeyCode == VK_BACK) {
                    if (!state.query.empty()) {
                        if (e.dwControlKeyState & SHIFT_PRESSED) {
                            state.query.clear();
                        } else {
                            state.query.resize(state.query.length() - 1);
                        }
                        state.OnQueryUpdate();
                    }
                }
            }
        }
    }

    std::cout << "Error: " << GetLastError() << '\n';
#endif

#if defined(LI_PLATFORM_LINUX)
    static termios orig_termios;

    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(+[]() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); });

    termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // Try to open an X11 display. If this fails we skip checking for shift states
    auto display = XOpenDisplay(NULL);

    constexpr static auto getch = [] -> char {
        char c = -1;
        auto res = read(STDIN_FILENO, &c, 1);
        return c;
    };

    for (;;) {
        pollfd pdf{ STDIN_FILENO, POLLIN };
        if (poll(&pdf, 1, -1) <= 0) continue;

        for (;;) {
            auto c = getch();
            if (c <= 0) break;

            // if (iscntrl(c)) {
            //     printf("%d\r\n", c);
            // } else {
            //     printf("%d ('%c')\r\n", c, c);
            // }

            if (c == 27 /* control */) {
                if (getch() == '[') {
                    c = getch();
                    if      (c == 'A' /* up */) state.Prev();
                    else if (c == 'B' /* down */) state.Next();
                    else if (c == 'C' /* right */) state.Enter();
                    else if (c == 'D' /* left */) state.Leave();
                    else if (c == 'Z' /* shift-tab */) state.Prev();
                } else {
                    // Must be ESC key
                    state.ReturnToCurrent(cd_output);
                    return EXIT_SUCCESS;
                }
            }
            else if (c == '\t') state.Next();
            else if (c == 23 /* Ctrl-W */) {
                state.query.clear();
                state.UpdateResults();
            }
            else if (c == 127 /* backspace */) {
                XkbStateRec xkb_state = {};
                if (display) XkbGetState(display, XkbUseCoreKbd, &xkb_state);
                if (xkb_state.mods & ShiftMask) {
                    state.query.clear();
                    state.UpdateResults();
                }
                else if (state.query.empty()) {
                    state.Leave();
                } else {
                    state.query.resize(state.query.length() - 1);
                    state.UpdateResults();
                }
            }
            else if (c == 8 /* ctrl-backspace */) {
                state.query.clear();
                state.UpdateResults();
            }
            else if (c == '\n') {
                if (state.Open(cd_output)) {
                    return EXIT_SUCCESS;
                }
            }
            else if (c == '/') state.Enter();
            else if (c == ':') state.Enter("/");
            else if (c == '~') state.Enter(getenv("HOME"));
            else if (c >= ' ' && c <= '~') {
                state.query += c;
                state.UpdateResults();
            }
        }
    }

    return EXIT_SUCCESS;

#endif
}
