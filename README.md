# Ls Interactive

A simple tool for quickly navigating through a filesystem inline in a terminal shell.

## Build / Install

- Build and install CMake project
- Add `alias li='. ls-interactive.sh'` to your shell

E.g.

```sh
cmake -B build -DCMAKE_INSTALL_PREFIX=~/.local
cmake --build build --target install

alias li='. ls-interactive.sh'
```

## Controls

- `:` - Go to root
- `/` - Enter selected folder
- `Enter`
    - If query is **empty** or selected entry is **not** a folder, exit and `cd` to currently open folder
    - Else, exit and `cd` to selected folder
- `Escape` - Quit and don't `cd`
- `(Shift) Tab` - Move up/down list
- `Up/Down Arrows` - Move up/down list
- `Right Arrow` - Open selected folder
- `Left Arrow` - Leave currently open folder
- `Control Backspace` - Clear query
- `Backspace`
    - If query is **empty**, leave currently open folder
    - Else, delete last character of query.
