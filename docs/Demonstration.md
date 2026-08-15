# Level Editor Install and Demonstration
- You'll need to install the PC Port (https://github.com/SlickAmogus/silent-hill-decomp) separately, rename the folder to `PC`, and place it in `game/` (as per `game/README.md` instructions).

## Install

### Windows (MSVC)
- Requires MSVC, Windows 11 SDK, Git (for dependencies). All of which are downloadable using [Visual Studio Community](https://visualstudio.microsoft.com/downloads/).

Apply the following commands from the repository directory `sh1-level-editor/`:
```bash
cmake -S . -B build
cmake --build build
```

### Windows (MinGW)
- Should be the same as above. Fully confirmed soon.

### Linux
- Support will come later.

### macOS
- Support will come much later.

### FreeBSD
- No idea.

## Usage
### Configuration
- Project directory will simply be `(yada yada)/sh1-level-editor/data/` (assets + workspace).
    - This is customisable to allow for multiple workspace/data directories.
- Game directory will simply be `(yada yada)/sh1-level-editor/game/` (PC Port + original BIN contained within at `game/`).

### Workspace
...