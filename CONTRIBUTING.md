# Contributing to Silent Hill Level Editor

Thank you for your interest in contributing to the **Silent Hill Level Editor**. This project is an interactive C++ level editor and asset toolkit for PlayStation 1 *Silent Hill* (1999) disc extracts and level data. Initial tests have proven successful, where actual data has been manipulated and viewable in-game, and file formats have been understood better. Contributions are highly encouraged, there is still major progress to be done before the editor is fully furnished. The editor can be assistive to the development of the PC Port for event handling, camera positions and so forth.

Contributions would mainly relate to the creation of unimplemented level features (Enemy placement, Audio trigger points, Map-specific parameters, etc), otherwise it would likely involve adding/fixing Python asset tools, or contributing further to existing documentation for file formats / research.

**Context and Past Development**: As of August 2026, I have worked on this project almost continuously for two months, working on IPD/PLM parsing + reconstruction and later many elements of the application's GUI frontend. I previously held most of my development in a private repository containing extensive but messy research on file formats. I am working on summarising everything in documentation here. The software itself is quite extendable and source code is abstracted well enough to accomodate for that. I'm open to hear and discuss suggestions so don't be afraid to reach out.

**AI Use**: AI has been used extensively in early prototyping, proprietary format parsing and rapid development of the GUI. You are welcome to use AI equivalently. For starters, there are many unnamed functions, enums, structs and file formats that are difficult to reverse-engineer. Known data types and structs are mapped out and understood fairly well, but there are still a multitude of unexplored file formats. Logical consistency can be hard to maintain manually, especially when trying to rapidly develop projects like these. I've used Gemini 3.1 Pro, Gemini 3.6/3.7 Flash, and Claude Sonnet 4.6/5 all within Antigravity IDE, and found that these were more than sufficient for both development and diagnostics. Documentation should be reviewed manually. Just make sure to work through goals incrementally, and use your awareness of the game engine and/or the PC Port to guide judgement.

---

## 1. Guiding Principles & Research Methodology

To maintain accuracy and data integrity across reverse-engineered PlayStation 1 formats, all contributions must adhere to these core principles:

### Evidence Hierarchy
When documenting binary structures or implementing parsers/serializers, prioritize evidence in the following order:
1. **Decompiled Engine Source:** Verified source code and symbols from decompilation efforts (e.g. `game/PC/` and upstream [`silent-hill-decomp`](https://github.com/SlickAmogus/silent-hill-decomp)).
2. **Empirical Binary Tests:** Byte-exact round-trip validation and cross-file verification across multiple official retail map chunks.
3. **Reference Tools & Converters:** Empirically verified community tools (e.g. `belek666/sh_ipd2obj`).
4. **Community Notes & Theories:** Helpful for context, but must be empirically tested before being treated as authoritative.

### Byte Precision & Round-Trip Validation
Parsers and serializers must preserve data integrity. A fundamental rule of this project is that converting an asset forward and then back must yield a bit-identical or fully specification-compliant file. Always verify round-trip integrity (via SHA256 checksums or hex diffs) when modifying conversion logic in `IPDWrite` or `scripts/core/`.

### Documenting Research
- **Confirmed Facts:** Empirical discoveries backed by decompiled code or byte-exact tests must be added with citations to [`docs/research/FACTS.md`](docs/research/FACTS.md).
- **Hypotheses & Open Questions:** Unproven theories, candidate struct layouts, or speculative flags belong in [`docs/research/HYPOTHESES.md`](docs/research/HYPOTHESES.md) until verified.

---

## 2. Asset & Copyright Policy

> [!CAUTION]
> **Do not commit copyrighted game assets.**  
> Never submit pull requests or commits containing original retail game files, PlayStation disc ISOs, raw `.IPD`/`.PLM`/`.TIM`/`.BIN` extracts from commercial discs, or copyrighted game audio/textures.

- Contributors must provide their own legally acquired copy of *Silent Hill* for development and testing.
- All extracted game data must remain in local workspace directories (`data/workspace/`, `data/assets/`, `data/temporary/`, `game/`), which are ignored by `.gitignore`.

---

## 3. Codebase Architecture

- **`src/` & `include/`:** The unified C++ application built on **C++17**, [Raylib](https://www.raylib.com/) (3D rendering and windowing), [Dear ImGui](https://github.com/ocornut/imgui) (docking branch), and [rlImGui](https://github.com/raylib-extras/rlImGui).
  - `core/`: Engine logic, binary parsers (`IPDParse`, `IPDWrite`), undo/redo history (`History`), asset managers, and configuration.
  - `geometry/`: Low-level mesh manipulation, topology editing, and PS1 hardware limits validation (`ChunkValidator`).
  - `panels/`: ImGui UI windows (Chunk manager, Dependencies manager, Texture Map, Local Geometry tools, Waypoints inspector, Outliner, Settings).
  - `viewport/`: 3D viewports, orbit camera controls, frustum culling, wireframes, and multi-mode overlays (`LocalGeometryOverlay`, `CollisionOverlay`, `WaypointsOverlay`).
- **`scripts/`:** Python toolchain for asset extraction, format conversion, and decompilation bridge:
  - `scripts/core/`: Format parsers (`ipd_parser.py`, `plm_parser.py`, `tim_to_png.py`, `png_to_tim.py`, `json_to_obj.py`).
  - `scripts/backend/`: Lean CLI entry points invoked natively by C++ `AssetManager` (`chunk_extractor.py`, `deploy_workspace.py`, etc.).
  - `scripts/convert.py`: Unified CLI dispatcher for standalone conversions.
- **`tasks/`:** Task-based development workflow. [`tasks/FullEditor/TASK.md`](tasks/FullEditor/TASK.md) serves as the master task channel coordinating overarching features and linking to specialized sub-tasks (`Dependencies/`, `GlobalObjects/`, `Waypoints/`, `BinaryOverlays/`, `Audio/`, `ContextMenus/`).
- **`docs/`:** Technical documentation and reverse-engineering research:
  - [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): Authoritative C++ component map, class index, and method reference.
  - [`docs/AI Guidance.md`](docs/AI%20Guidance.md): Operational guidelines and behavioral rules for AI assistants and contributors.
  - [`docs/formats/`](docs/formats/): Byte-level binary format specifications (`IPD.md`, `Collision.md`, `Binary Overlays.md`, `JSON.md`).
  - [`docs/research/`](docs/research/): Proven discoveries (`FACTS.md`) and working hypotheses (`HYPOTHESES.md`).

---

## 4. Development & Coding Standards

### C++ Guidelines
- Target **C++17**.
- **Modularity:** Keep files focused and readable (aim for source files under 1000 lines where practical). Split complex UI panels and geometry routines into specialized modules.
- **PS1 Scale & Coordinate Quantization:** 3D editing operates in floating-point world coordinates ($1\text{ unit} = 256\text{ raw units}$), but all geometry must cleanly quantize to PlayStation signed 16-bit integers (`int16_t`) on disk to prevent rasterizer seams.
- **Hardware Constraints Validation:** Ensure all new geometry tools respect PlayStation limits (max 255 vertices per submesh, 16-unit max height, 40-unit chunk cell boundaries) as enforced by `ChunkValidator`.
- **Memory & Resource Safety:** Use RAII, smart pointers, and standard library containers. Avoid raw memory leaks across texture loads and GPU batch allocations.

### Python Guidelines
- Target **Python 3.8+**.
- Use pure-Python standard libraries (`struct`, `json`, `pathlib`, `argparse`) wherever possible to keep dependencies lightweight.
- Ensure all conversion scripts support deterministic round-trip output.

### Keeping Documentation in Sync
- **Architecture Updates:** If your change adds, modifies, or removes callable classes, methods, panels, or viewports, update [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).
- **Task Logs:** When working on a feature, update the relevant `tasks/[Task]/TASK.md` and `TODO.md`. Once fully complete, append a summary line to [`CHANGELOG.md`](CHANGELOG.md).

### Building & Testing
Before submitting changes, verify that the project builds and runs cleanly:

```bash
# Configure build directory
cmake -B build

# Build in Release mode (MinGW / MSVC / GCC)
cmake --build build --config Release
```

1. **GUI & Viewport Verification:** Launch the built executable (`build/SilentHillLevelEditor.exe`) and test viewport navigation, mode switching, chunk loading, and tool interaction without crashes or memory corruption.
2. **Conversion Script Testing:** If modifying Python tools, verify both forward and reverse conversions:
   ```bash
   python scripts/convert.py ipd-to-json <sample.ipd> -o <output.json>
   python scripts/convert.py json-to-ipd <output.json> -o <rebuilt.ipd>
   ```
3. **Commit Cleanliness:** Confirm no copyrighted game files, build artifacts, or temporary logs are staged.

---

## 5. Submitting Changes

### Pull Requests
1. **Branch Naming:** Create a focused feature branch off `main` (e.g. `feature/door-spline-gizmo`, `fix/plm-matrix-quantization`, `docs/ipd-collision-flags`).
2. **Descriptive Commits:** Write concise commit messages explaining *what* was changed and *why*.
3. **PR Summary:** Describe the problem solved, the technical implementation, and step-by-step instructions to verify the changes.
4. **Clean Diff:** Double-check that your PR does not accidentally include temporary files (`data/temporary/`, `scripts/temporary/`), build folders (`build/`), or proprietary game assets.

### Reporting Issues & Format Discoveries
- **Bug Reports:** Open a GitHub Issue detailing your OS, compiler/environment, steps to reproduce, and any relevant console logs.
- **Format Corrections:** If proposing corrections to binary structures or struct layouts, include relevant hex offsets, sample file context, or decompiled code references.

---

## 6. Questions & Discussions
For feature suggestions, architecture questions, or reverse-engineering discussions, feel free to open a GitHub Issue or Discussion thread.