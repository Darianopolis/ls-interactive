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

constexpr std::string_view separator = "/";

struct File
{
    std::filesystem::path path;
    bool is_dir = false;
    bool non_empty = false;

    File(std::filesystem::path _path)
        : path(std::move(_path))
    {
        try {
            is_dir = std::filesystem::is_directory(path);
            non_empty = !std::filesystem::is_empty(path);
        }
        catch (...) {}
    }

    File(const File& other) = default;
};

struct Dir
{
    std::vector<File> files;
    bool cached = false;
};

template<typename T>
auto case_insensitive_find(const T& haystack, const T& needle)
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
auto case_insensitive_contains(const T& haystack, const T& needle) -> bool
{
    return case_insensitive_find(haystack, needle) != std::end(haystack);
}

struct LsInteractive
{
    std::filesystem::path path = "";
    std::vector<File> paths;
    size_t selected = 0;
    size_t last_height = 0;
    bool drives = false;
    int clear_lines_on_exit = 0;
    bool show_caret = false;
    std::string query;

    std::unordered_map<std::filesystem::path, size_t> index_cache;
    std::unordered_map<std::filesystem::path, Dir> dir_cache;

    Dir& open_dir(const std::filesystem::path& dir)
    {
        auto& cached = dir_cache[dir];
        if (!cached.cached) {
            cached.cached = true;
            cached.files.clear();
            try {
                for (auto&e: std::filesystem::directory_iterator(dir,
                        std::filesystem::directory_options::skip_permission_denied)) {
                    cached.files.push_back(e.path());
                }
            } catch (...) {}
            std::ranges::stable_sort(cached.files, [](const File& l, const File& r) -> bool {

                // Directories first
                if (l.is_dir != r.is_dir) return (l.is_dir && !r.is_dir);

                auto l_name = l.path.filename().string();
                auto r_name = r.path.filename().string();

                // Sort lexicographically
                return l_name < r_name;
            });
        }
        return cached;
    }

    void enter(std::filesystem::path p)
    {
        while (!std::filesystem::exists(p)) {
            p = p.parent_path();
        }

        paths.clear();
        this->path = p;

        for (const auto& dir = open_dir(p); auto& p2: dir.files) {
            if (case_insensitive_contains(p2.path.filename().string(), query)) {
                paths.push_back(p2);
            }
        }

        if (!query.empty()) {
            std::ranges::stable_sort(paths, [&](const File& l, const File& r) -> bool {
                auto l_name = l.path.filename().string();
                auto r_name = r.path.filename().string();
                auto l_pos = case_insensitive_find(l_name, query) - std::begin(l_name);
                auto r_pos = case_insensitive_find(r_name, query) - std::begin(r_name);

                // Prefer earlier matches
                if (l_pos != r_pos) return l_pos < r_pos;

                // Prefer shorter total matches
                return l_name.length() < r_name.length();
            });
        }

        if (const auto f = index_cache.find(p); f != index_cache.end()) {
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

        draw();
    }

    void update_results()
    {
        enter(path);
    }

    void draw()
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
                << (path.string().ends_with(separator) ? "" : separator)
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

            std::cout << (p.is_dir && p.non_empty ? "\x1B[" LI_FOLDER_COLOR : "\x1B[39m");

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
                if (p.is_dir && !name.ends_with(separator)) {
                    std::cout << separator;
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

    static void clear_extra(const int lines)
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

    void clear() const
    {
        std::cout << "\x1B[1G"; // Reset to start of line
        if (last_height > 0) {
            std::cout << "\x1B[" << last_height << "A";
        }

        std::cout << "\x1B[" << (1 + last_height) << "M"; // Clear lines!
    }

    void leave()
    {
        if (!path.empty()) {
            const auto current_path = path;
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

    void enter()
    {
        if (!paths.empty()) {
            if (const auto selected_file = paths[this->selected]; selected_file.is_dir && selected_file.non_empty) {
                query.clear();
                enter(selected_file.path);
                draw();
            }
        }
    }

    void return_to_current(int out_fd) const
    {
        write(out_fd, ".", 1);
        fsync(out_fd);

        clear();
        clear_extra(clear_lines_on_exit);
    }

    [[nodiscard]] bool open(int out_fd) const
    {
        std::filesystem::path target;
        if (query.empty()) {
            target = path.string();
        } else {
            if (selected >= paths.size()) {
                std::cerr << "INVALID SELECTED " << selected << " > " << paths.size() << '\n';
                return false;
            }
            if (const auto selected_dir = paths[selected]; selected_dir.is_dir) {
                target = selected_dir.path.string();
            } else {
                target = path.string();
            }
        }
        if (!std::filesystem::exists(target)) {
            std::cerr << "TARGET DOES NOT EXIST " << target.c_str() << '\n';
            return false;
        }

        auto target_str = target.string();
        write(out_fd, target_str.data(), target_str.size());
        fsync(out_fd);

        clear();
        clear_extra(clear_lines_on_exit);

        return true;
    }

    void prev()
    {
        if (selected > 0) {
            selected--;
            index_cache[path] = selected;
            draw();
        }
    }

    void next()
    {
        if (selected + 1 < paths.size()) {
            selected++;
            index_cache[path] = selected;
            draw();
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

    LsInteractive state = {};
    state.clear_lines_on_exit = std::stoi(argv[1]);
    state.clear_extra(1);
    std::filesystem::path path;
    if (const char* home = getenv("HOME")) {
        path = home;
    } try {
        path = std::filesystem::current_path();
    } catch (...) {}
    state.enter(path);

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
                        break;case 'A': /* up        */ state.prev();
                        break;case 'B': /* down      */ state.next();
                        break;case 'C': /* right     */ state.enter();
                        break;case 'D': /* left      */ state.leave();
                        break;case 'Z': /* shift-tab */ state.prev();
                        break;default:
                            ;
                    }
                } else {
                    // Must be escape key
                    state.return_to_current(output_fd);
                    return EXIT_SUCCESS;
                }
            }
            else if (c == '\t') state.next();
            else if (c == 23 /* ctrl-W */) {
                state.query.clear();
                state.update_results();
            }
            else if (c == 127 /* backspace */) {
                if (state.query.empty()) {
                    state.leave();
                } else {
                    state.query.resize(state.query.length() - 1);
                    state.update_results();
                }
            }
            else if (c == 8 /* ctrl-backspace */) {
                state.query.clear();
                state.update_results();
            }
            else if (c == '\n') {
                if (state.open(output_fd)) {
                    return EXIT_SUCCESS;
                }
            }
            else if (c == '/') state.enter();
            else if (c == ':') {
                state.query.clear();
                state.enter("/");
            }
            else if (c == '~') {
                state.query.clear();
                state.enter(getenv("HOME"));
            }
            else if (c >= ' ' && c <= '~') {
                state.query += c;
                state.update_results();
            }
        }
    }

    return EXIT_SUCCESS;
}
