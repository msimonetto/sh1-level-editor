# Geometry Editing & Undo/Redo Progress

## Context and Goal
The goal of the recent sessions was to introduce a vertex and face translation tool within the `Edit` viewport of the Unified C++ Editor, adhering to the project's exact constraints:
- PS1 geometry dictates coordinates are stored as `int16_t` internally, necessitating strict alignment and fixed-point scale (`IPD_SCALE`).
- The user requires "intentional overhang" mechanics, meaning vertices cannot drift over time or produce visual tearing.
- Modifications must update the active GPU buffers seamlessly.
- Every geometry modification must be hooked up to a robust Undo/Redo history stack to allow rapid prototyping.

## Accomplishments
1. **Frontend Architecture**
   - Added an `EditMode` enumerator (Object, Face, Vertex) to `EditViewport`.
   - Created UI toggles to switch between these modes.
   - Wired picking logic to highlight the active mesh, face (with yellow outlines), and vertex (with a magenta cube).
   - Added keyboard handling to translate the current selection using the Arrow Keys and PageUp/PageDown.
   - Made keyboard gestures relative to the camera's dominant axis to make editing intuitive in a 3D environment.

2. **Backend Architecture & Optimizations**
   - Implemented `TranslateSelection(Vector3 delta)` in `EditViewport` to directly manipulate the `mesh.vx, vy, vz` arrays.
   - Refactored `RebuildChunkBatches` in `EditViewport` and `ViewViewport` to utilize `memcmp` against the Raylib GPU buffers. This prevents unnecessary uploads and correctly uses `UpdateMeshBuffer` to patch positions and texture coordinates cleanly.
   
3. **Undo / Redo Buffer**
   - Designed a deep-copy `MeshSnapshot` stack in `EditHistory.cpp`.
   - Refactored the `EditHistory` class to apply structural patches simultaneously to both the `ViewViewport` and `EditViewport`.
   - Implemented `PushOrMerge` logic to group continuous key-press translations into single discrete Undo actions.

## Resolved Issue (Ctrl+Z Does Not Revert Geometry)
The bug where `Ctrl+Z` did not revert geometry visually (and `Ctrl+S` did not save) has been resolved. The root cause was not mathematical or tied to Raylib's GPU buffers, but rather a UI scope issue.

The global keyboard shortcut logic in `main.cpp` was incorrectly nested inside the `Texture Manager` UI block and explicitly gated behind `if (activeFace)` and `if (testTexture.GetTexture().id != 0)`. This meant that when translating objects or vertices (where no specific face is necessarily selected, or when working with untextured collision geometry), the global Undo/Redo and Save handlers were unreachable.

**Fix:**
- Extracted the `Ctrl+Z`, `Ctrl+Y`, and `Ctrl+S` handlers to the global main loop scope, ensuring they always trigger regardless of UI selection state.
- Decoupled the `IpdWriter::WriteChunk` save logic from requiring an `activeObjName`.
- The `EditHistory::Undo` and `EditHistory::Redo` mechanics, as well as `IpdWriter` vertex patch encoding, were already fully functional and are now correctly utilized.
