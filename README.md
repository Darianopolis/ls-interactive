# Ls Interactive

A tiny tool for quickly navigating through a filesystem inline in a terminal shell.

### Goals:
- Beat `dir`, `ls` and `cd` in number of keypresses required to perform any common actions. (Additional functionality shouldn't come at the cost of overhead)
    - E.g. `cd ..` takes 6 keypresses and two hands. `li`, `Enter`, `Backspace`, `Enter` takes only 5 and one hand.
- Avoid cluttering up console output by reusing console buffer lines.

### To Do:
- Tidy up and refactor.
- Add option to quit **without** clearing output.

# Build, Install & Run

- Build and install CMake project
- Add `alias li='. ls-interactive.sh'` to your shell (E.g. in `.bashrc`)

# Controls

These controls are hardcoded and their actions may depend on context. They are optimized for speed, and may not seem entirely intuitive at first.

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

Arrow keys are intended as a secondary input only. `Tab`, `/` and `Backspace` are intended to be the primary controls for navigation, as they are quicker to press while typing file/folder names.
