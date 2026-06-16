#include <sys/ioctl.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <cassert>

#include <poll.h>

#include <iostream>
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <format>

using namespace std::literals;

namespace fs = std::filesystem;

constexpr char Separator = '/';
constexpr char SeparatorStr[2] = { Separator, '\0' };

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
auto CaseInsensitiveFind(const T& haystack, const T& needle)
{
    auto it = std::search(
        std::begin(haystack), std::end(haystack),
        std::begin(needle), std::end(needle),
        [](auto c1, auto c2) {
            return std::toupper(c1) == std::toupper(c2);
        });

    return it;
}

template<typename T>
bool CaseInsensitiveContains(const T& haystack, const T& needle)
{
    return CaseInsensitiveFind(haystack, needle) != std::end(haystack);
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

    Dir& OpenDir(const fs::path& dir)
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
            std::ranges::stable_sort(cached.paths, [](const File& l, const File& r) -> bool {

                // Directories first
                if (l.dir != r.dir) return (l.dir && !r.dir);

                auto l_name = l.path.filename().string();
                auto r_name = r.path.filename().string();

                // Sort lexicographically
                return l_name < r_name;
            });
        }
        return cached;
    }

    void Enter(fs::path p)
    {
        while (!std::filesystem::exists(p)) {
            p = p.parent_path();
        }

        paths.clear();
        this->path = p;

        for (const auto& dir = OpenDir(p); auto& p2: dir.paths) {
            if (CaseInsensitiveContains(p2.path.filename().string(), query)) {
                paths.push_back(p2);
            }
        }

        if (!query.empty()) {
            std::ranges::stable_sort(paths, [&](const File& l, const File& r) -> bool {
                auto l_name = l.path.filename().string();
                auto r_name = r.path.filename().string();
                auto l_pos = CaseInsensitiveFind(l_name, query) - std::begin(l_name);
                auto r_pos = CaseInsensitiveFind(r_name, query) - std::begin(r_name);

                // Prefer earlier matches
                if (l_pos != r_pos) return l_pos < r_pos;

                // Prefer shorter total matches
                return l_name.length() < r_name.length();
            });
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
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        const auto cols = w.ws_col;

        std::cout << "\x1B[?25l"; // Hide cursor
        std::cout << "\x1B[1G"; // Reset to start of line
        if (last_height > 0) {
            std::cout << "\x1B[" << last_height << "A";
        }

        std::cout << "\x1B[" << (1 + last_height) << "M"; // Clear lines!

        std::cout << "\x1B[4;32m" << path.string()
                << (path.string().ends_with(Separator) ? "" : SeparatorStr)
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

#define LI_FOLDER_COLOR "93m"

            std::cout << (p.dir && p.non_empty ? "\x1B[" LI_FOLDER_COLOR : "\x1B[39m");

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
                if (p.dir && !name.ends_with(Separator)) {
                    std::cout << Separator;
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

    void ReturnToCurrent(int out_fd) const
    {
        write(out_fd, ".", 1);
        fsync(out_fd);

        Clear();
        ClearExtra(clear_lines_on_exit);
    }

    [[nodiscard]] bool Open(int out_fd) const
    {
        fs::path target;
        if (query.empty()) {
            target = path.string();
        } else {
            if (selected >= paths.size()) {
                std::cerr << "INVALID SELECTED " << selected << " > " << paths.size() << '\n';
                return false;
            }
            if (const auto selected_dir = paths[selected]; selected_dir.dir) {
                target = selected_dir.path.string();
            } else {
                target = path.string();
            }
        }
        if (!fs::exists(target)) {
            std::cerr << "TARGET DOES NOT EXIST " << target.c_str() << '\n';
            return false;
        }

        auto target_str = target.string();
        write(out_fd, target_str.data(), target_str.size());
        fsync(out_fd);

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
    if (auto* extra_clears_str = std::getenv("LI_EXTRACLEARS")) {
        if (int extra_clears = std::stoi(extra_clears_str)) {
            std::cout << std::format("\x1b[{}M\x1b[0G\x1b[{}A", extra_clears + 1, extra_clears);
        }
    }

    constexpr int output_fd = 3;

    auto state = State{std::stoi(argv[1])};
    state.ClearExtra(1);
    std::filesystem::path path;
    if (const char* home = getenv("HOME")) {
        path = home;
    }
    try {
        path = fs::current_path();
    } catch (...) {}
    state.Enter(path);

    static termios orig_termios;

    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(+[]() { tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios); });

    termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    constexpr static auto getch = []() -> char {
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

            if (c == 27 /* esc */) {
                if (getch() == '[') {
                    c = getch();
                    switch (c) {
                        break;case 'A': /* up        */ state.Prev();
                        break;case 'B': /* down      */ state.Next();
                        break;case 'C': /* right     */ state.Enter();
                        break;case 'D': /* left      */ state.Leave();
                        break;case 'Z': /* shift-tab */ state.Prev();
                        break;default:
                            ;
                    }
                } else {
                    // Must be escape key
                    state.ReturnToCurrent(output_fd);
                    return EXIT_SUCCESS;
                }
            }
            else if (c == '\t') state.Next();
            else if (c == 23 /* ctrl-W */) {
                state.query.clear();
                state.UpdateResults();
            }
            else if (c == 127 /* backspace */) {
                if (state.query.empty()) {
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
                if (state.Open(output_fd)) {
                    return EXIT_SUCCESS;
                }
            }
            else if (c == '/') state.Enter();
            else if (c == ':') {
                state.query.clear();
                state.Enter("/");
            }
            else if (c == '~') {
                state.query.clear();
                state.Enter(getenv("HOME"));
            }
            else if (c >= ' ' && c <= '~') {
                state.query += c;
                state.UpdateResults();
            }
        }
    }

    return EXIT_SUCCESS;
}
