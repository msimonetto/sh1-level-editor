# Tasks

This folder holds active development objectives. Each objective lives in its own subdirectory with a `TASK.md` file.

## Structure

```
tasks/
  [objective_name]/
    TASK.md         ← required: goal, status, next steps
    TODO.md         ← task-specific work queue and sub-items
    notes.md        ← optional: working notes, findings
  _completed/       ← finished tasks are moved here
  _deprecated/      ← outdated or abandoned tasks
```

## Rules

- Keep `TASK.md` concise and structured with a clear scope, status, and linked next steps.
- Update the relevant `TASK.md` before switching to a different objective.
- When a task completes: move its folder to `_completed/`, append a line to `CHANGELOG.md`.

## Active Tasks

| Folder | Objective |
|---|---|
| `FullEditor/` | **Master Task Channel**: Unified C++ ImGui level editor coordinating overarching features, phased roadmaps, and cross-module integration. |
| `Dependencies/` | Workspace file manager, bidirectional asset dependency tracking (`dependencies.json`/`dependents.json`), and safety locks. |
| `GlobalObjects/` | PLM Object Manager for discovering, inspecting, instantiating, and editing `_GLB.PLM` global props. |
| `Waypoints/` | Room Linkage Editor for 3D door triggers (`s_EventData`), destination waypoints (`s_MapPoint2d`), and map transitions. |
| `BinaryOverlays/` | Binary overlay analysis (`VIN/MAP*.BIN`), C struct schema mapping, and custom map director pipeline. |
| `Audio/` | PS1 audio player and environment soundscape simulator (`.VAB`, `.SEQ`, `libsd` streaming, BGM layer mixers). |
| `ContextMenus/` | Viewport right-click contextual actions (cut, copy, paste, delete, UV reset, align). |