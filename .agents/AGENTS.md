# AGENTS.md
## 1. Context
- Use `docs/ARCHITECTURE.md` for C++ multi-folder project overview, use as a vantage point for diagnostics. Ensure this is updated whenever a major architectural change is made, or a new method/feature is added (not just small GUI changes, but for callable commands/classes).
- We are developing a Level Editor for Silent Hill 1, taking known data structures for maps and chunks, making use of a wide variety of element-specific panels for user-modifiability of game data using a GUI. The GUI is constructed using C++ (ImGui/Raylib) with 3D viewports, and there is secondarily usage of Python scripts for specific functionality (e.g., extracting disc assets) and for testing.

## 2. Directory and File Rules
- **Read-only Folders**: `data/assets/` (PS1 Game Assets), `data/workspace/` (C++ Program Data), `game/` (unless explicitly asked to do so). Do not add new files or folders to the main project folder. Avoid creating new files/folders in arbitrary locations, consult with user if you have to.
- **Temporary Files**: `scripts/research/` (used to test hypotheses and theories, advised to clean up after use). `data/temporary/` (used for temporary game data, mainly with diagnostics to see if underlying scripts are running correctly on game data, do not place scripts here!).
- **Old Layout**: Memory and documentation may relate to old project layout:
    - `tooling/unified_cpp_editor/` refers to main C++ GUI application (now across project directory)
    - `tooling/` consisted of several Python scripts and GUI applications, all of which are deprecated
    - `external/SlickAmogus_silent-hill-decomp/` has been changed to `game/PC/`
- **New Layout**: The root directory now houses the main C++ application (`src/`, `include/`, `build/`, `CMakeLists.txt`). No unrelated new files/folders without permission.
- **Other**: Never perform Git commit/staging operations.

## 3. Task-Based Workflow
- The project is driven by specific objectives stored in `tasks/`.
- Objectives will automatically relate to `tasks/core_editor/` unless otherwise stated. Most will relate to components of the editor in other task directories.
- If the user says "Start a new session on X" or "Continue with X", you MUST automatically find the relevant folder (e.g. `tasks/X/TASK.md`), read the active goal, and verify the task's `TODO.md` next steps without asking the user for paths or re-explaining the rules.
- Update task-specific `CHANGELOG.md` and `TODO.md` when necessary.

## 4. Documentation
- **`docs/`**: Keep concise and accurate. Ask before creating new files. The knowledge base is critical for more technically-detailed work, although GUI improvements will unlikely relate to anything here. Documentation specific to AI usage is in full caps, knowledge documentation in more natural format.
- **`docs/formats/`**: Should hold carefully considered truths on file structure, the data and structs used. Some relate to the file format itself (`IPD.md`) and others relate to known structures (`Binary Overlays.md`).
- **`docs/AI Guidance.md`**: Read this for the full task workflow, file purposes, and experiment loop.
- **`docs/research/HYPOTHESES.md` & `docs/research/FACTS.md`**: Move ideas to FACTS only with empirical proof (decompiled source or byte-exact test).
- **`CHANGELOG.md`**: Append one bullet per completed task. Format: `- **[YYYY-MM-DD]** Description`.

## 5. Token Usage
- **Reporting Time-Consuming Tasks**: For any prompt that could possibly take longer than 5 minutes, or has taken over 5 minutes to respond to, you need to be transparent about progress and periodically ask for feedback. Give reasonable estimates for the amount of time taken, and overall progress (you must have external reasons based on user input/feedback to justify that the task will be short).
- **Response Degradation**: In long conversations, typically either 8-10 prompts sent or after spending 20+ minutes, you need to actively acknowledge degraded performance and be easy to transfer context, actions and hypotheses into task documentation (specific, temporary, or in `CHANGELOG.md`). You may run into contradictions often, or make overly confident claims. Report these contradictions to the user in a summary. Re-read `docs/research/FACTS.md`, `docs/formats/*.md` or task-specific ideas to confirm your understanding.
- **Answer Succinctly**: Only include information relevant to solving the problem. Credit usage is critical! Unless you are performing detailed diagnostics or reports at the user's request, you should debrief the user on your findings or successes in a comprehensible way.
- **Massive Source Files**: When relevant, it is important for C++ source code or scripting under 1000 lines. This doesn't have to be adhered to strictly, but you should intelligently try to find ways to separate source files into modules, especially when methods are used across multiple files. Ensure you have user agreement/permission.
- **Massive Data Files**: NEVER open/analyse large data files (>100KB) directly. This destroys context. Consult `docs/formats/` or write a Python script to print only the necessary few lines. Make use of existing scripts.
- **Terminal Output**: Do NOT dump massive arrays or hex dumps directly to standard output. Pipe large outputs (and not scripts) into `data/temporary/` files and search them securely.
- **Inline Scripts & Shell Commands**: If you are writing a simple multi-line script in terminal, this is acceptable. If this is moderately complex or is highly reusable (or has been used immediately), you need to use scripts placed in `scripts/temporary/`. In complex cases, prefer Windows-native tools (specific to PowerShell or CMD) over fragile shell commands (e.g., `Format-Hex`, `echo`). You also need to be careful about the location of inputs/outputs as to not clutter the project layout. Don't get caught up in generating multiple Python scripts in trying to parse the editor's source code!
- **No Hallucinations**: ALWAYS cross-reference established documentation (such as `docs/ARCHITECTURE.md`) BEFORE attempting to guessing file locations.