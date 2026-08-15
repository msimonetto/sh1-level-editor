# Implementation Plan (Revised): Direct Binary Editing Pipeline

---

## Background & Constraints

1. **Keep `AssetPipeline::ExportToJson/ExportToObj/ReconstructIpd` alive** for side-by-side comparison testing against the new C++ write path. Deletion is a separate, later task.
2. **Rename `IpdBuilder` → `IpdWriter`**. The old `BuildFromObj` stub is replaced entirely.
3. **Undo/redo is global** (one shared history across all loaded chunks). Default depth: 50.
4. **UV precision: uint8** (matching game data types). Sub-pixel accuracy investigation is deferred — do not clamp arbitrarily; round-trip fidelity to the original binary is the priority.
5. **Variable-size writes must be architected for** even where they are not implemented yet (the current editable surface — UV/tex/clut — is always same-size; vertices and mesh structure additions are future work).

---

## Critical Design Observation: Shared PLM Objects

A key subtlety that affects the writer design:

`ParsedChunk.objects` has **one entry per placement**, not per unique PLM object. The same `PLM_OBJ_HEADER` (identified by `objName`) can back multiple `RenderObject` entries in `out.objects`. The UV/tex data lives in `PLM_PACK_HEADER`, which is part of the PLM *template*, not the placement. **Editing a face's UV edits it for every placement of that object.** The UI and the writer must both reflect this: the save target is the PLM entry identified by `(objectName, meshIdx, packIdx)`, not by `(renderObjectIdx, meshIdx, packIdx)`.

Additionally, the `meshCache` declared in `IpdParser::Parse` at L541–543 is never populated — it is dead code from a prior refactor. It should be removed during Phase 1 cleanup.

---

## The Write-Back Strategy: "Intelligent Section Patcher"

Full re-serialisation (rebuilding the entire file from `ParsedChunk`) is avoided because `ParsedChunk` does not retain all raw binary fields (e.g. `unk1_data`, `unk3`, normals). A pure byte-offset patch-in-place is fragile against size changes.

The chosen approach is a **section-aware buffer patcher**:

1. The entire source file is read into a `std::vector<uint8_t>` (the *working buffer*).
2. For **same-size changes** (UV/CBA/texnum): seek to the known `PLM_PACK_HEADER` offset and overwrite the 20 bytes in-place. No relocation needed.
3. For **size-changing changes** (future: add/remove vertex, add/remove pack): isolate the affected section in the working buffer, replace it with a resized version, then walk the file header(s) and PLM header(s) to **add the delta to every absolute offset that points past the insertion point**. Sections that were not touched are preserved byte-for-byte.

This relies on a `OffsetPatcher` utility (new internal class in `IpdWriter.cpp`) that knows the schema of every offset field in `IPD_FILE_HEADER`, `PLM_FILE_HEADER`, `PLM_OBJ_HEADER`, and `PLM_DATA_HEADER`, and can apply a `(insertionPoint, delta)` relocation to all of them in one pass.

---

## File Change Summary

| File | Action |
|------|--------|
| `AssetPipeline.h/.cpp` | **No change** — kept for comparison testing |
| `IpdBuilder.h/.cpp` | **Rename** to `IpdWriter.h/.cpp`; replace stub with new interface |
| `IpdParser.h` | Add `FaceAddress` struct; add 2 fields to `RenderFace`; remove `meshCache` |
| `IpdParser.cpp` | Record `FaceAddress` per face; remove dead meshCache; fix 2 comments |
| `IpdWriter.h` | **[NEW]** `IpdWriter` class with patch-in-place + section-relocation design |
| `IpdWriter.cpp` | **[NEW]** Implementation (patch path now; relocation path stubbed) |
| `EditHistory.h` | **[NEW]** Global undo/redo command stack |
| `EditHistory.cpp` | **[NEW]** Implementation |
| `main.cpp` | Remove `saveToJson` lambda/button; add "Save" + Undo/Redo wiring; keep "Export to JSON" button for comparison |

---

## Phase 1 — Parser Augmentation

### 1a. New `FaceAddress` struct in `IpdParser.h`

This is the **canonical, location-stable address** of a face. It identifies where in the PLM binary a pack lives, independent of file byte positions (which shift when sizes change).

```cpp
struct FaceAddress {
    std::string plmObjectName; // 8-char PLM object name (key into PLM_OBJ_HEADER)
    int         meshIdx;       // index into PLM_DATA_HEADER array for this object
    int         packIdx;       // index into PLM_PACK_HEADER array for this mesh
    bool        isGlobal;      // false = local PLM inside .IPD, true = _GLB.PLM
    int32_t     packRawOffset; // cached absolute byte offset of PLM_PACK_HEADER
                               // within its source file (IPD or GLB).
                               // Valid at parse time; must be recomputed after
                               // any size-changing operation on the same file.
};
```

> [!NOTE]
> `packRawOffset` is a performance cache. `IpdWriter` always re-derives the true offset from `(plmObjectName, meshIdx, packIdx)` by navigating the live working buffer — it does not blindly trust `packRawOffset` after a relocation. For the current same-size patch path, the cache is used directly; after any relocation, the cache is invalidated.

### 1b. Additions to `RenderFace` in `IpdParser.h`

```cpp
struct RenderFace {
    uint8_t     v[4];
    float       uv[4][2];
    uint8_t     texNum;
    std::string texName;
    uint8_t     paletteRow;
    uint16_t    cbaRaw;
    // --- NEW ---
    FaceAddress addr;          // Logical + cached physical address for write-back
    // Raw uint8 UVs, preserved for precision round-trip
    uint8_t     rawU[4], rawV[4];  // Original bytes from PLM_PACK_HEADER before bias
};
```

`rawU/rawV` preserve the original `uint8_t` values from the binary before the UV-bias and normalisation step. When `IpdWriter` encodes UVs back, it will:
1. Denormalise the editor's `float uv[i][j]` back to `[0, 256)` space (× 256).
2. Round to nearest integer.
3. Reverse the bias (detect whether bias was applied via `rawU/rawV` comparison, subtract 1 from max vertices accordingly).
4. Clamp to `[0, 255]`.

This means sub-pixel float edits are preserved in memory but rounded on save — matching the game's uint8 precision. The `rawU/rawV` cache enables a future "precision audit" without data loss.

### 1c. Changes to `IpdParser.cpp`

- In `ParseAndPlaceObject`, after computing `pkOff`, populate:
  ```cpp
  face.addr.plmObjectName = outObj.name;
  face.addr.meshIdx       = m;    // loop variable
  face.addr.packIdx       = p;    // loop variable
  face.addr.isGlobal      = isGlobal;
  face.addr.packRawOffset = pkOff; // absolute offset within srcBuf
  // Store raw UVs before bias
  face.rawU[0]=pk->u0; face.rawU[1]=pk->u1; face.rawU[2]=pk->u2; face.rawU[3]=pk->u3;
  face.rawV[0]=pk->v0; face.rawV[1]=pk->v1; face.rawV[2]=pk->v2; face.rawV[3]=pk->v3;
  ```
- Remove the unused `meshCache` declaration (L541–543).
- Fix comments on L10, L226: replace `json_to_blender.py` → `coordinate_math.py`.

---

## Phase 2 — IpdWriter (Rename + Implement)

### 2a. `IpdWriter.h`

```cpp
#pragma once
#include "IpdParser.h"   // ParsedChunk, RenderFace, FaceAddress
#include <string>
#include <vector>

class IpdWriter {
public:
    // ---------------------------------------------------------------------------
    // WriteChunk: applies all in-memory modifications from 'chunk' back to disk.
    // Targets:
    //   ipdPath   = workspace/chunks/{CHUNK}.IPD  (always modified)
    //   glbPath   = workspace/geometry/{PREFIX}_GLB.PLM (modified only if
    //               chunk contains global-face edits; empty string = skip)
    // Returns false and logs on error.
    // ---------------------------------------------------------------------------
    static bool WriteChunk(const std::string& ipdPath,
                           const std::string& glbPath,
                           const ParsedChunk& chunk,
                           int* outPatchedCount = nullptr);

    // ---------------------------------------------------------------------------
    // Future-facing: Validate basic structural integrity before writing.
    // Currently always returns empty. Will surface issues without blocking save.
    // ---------------------------------------------------------------------------
    static std::vector<std::string> Validate(const ParsedChunk& chunk);

private:
    // --------------- Same-size patch path (current) --------------------------
    // Apply UV/CBA/texnum changes for faces in 'faces' onto 'buf'.
    // 'buf' is the in-memory working copy of the source file.
    // Returns number of faces successfully patched.
    static int PatchFaces(std::vector<uint8_t>& buf,
                          const std::vector<const RenderFace*>& faces,
                          bool isGlobal);

    // Encode one RenderFace's UV/CBA/texnum back into a PLM_PACK_HEADER in buf
    // at the address given by face.addr.packRawOffset.
    // Does NOT change any other fields (normals, vertex indices).
    static void EncodeFace(std::vector<uint8_t>& buf, const RenderFace& face);

    // --------------- Section-relocation path (future size changes) -----------
    // Relocate all absolute offsets in an IPD file buffer when a section of
    // 'delta' bytes was inserted at (or removed from) 'insertionPoint'.
    // 'delta' is positive for insertions, negative for removals.
    // Walks: IPD_FILE_HEADER offsets, PLM_FILE_HEADER offsets,
    //        PLM_OBJ_HEADER.data_offset, PLM_DATA_HEADER offset fields.
    static void RelocateIPDOffsets(std::vector<uint8_t>& buf,
                                   int insertionPoint,
                                   int delta);

    // Same as above but for a standalone PLM file (e.g. _GLB.PLM).
    static void RelocatePLMOffsets(std::vector<uint8_t>& buf,
                                   int insertionPoint,
                                   int delta);

    // Utility: re-derive the byte offset of a PLM_PACK_HEADER from a
    // FaceAddress by navigating the live buffer.  Used after any relocation
    // to refresh stale packRawOffset caches.
    static int ResolveFaceOffset(const std::vector<uint8_t>& buf,
                                 const FaceAddress& addr,
                                 int plmBase);
};
```

### 2b. `IpdWriter.cpp` — Phase 2 implementation (same-size path only; relocation stubs)

**`WriteChunk()`**:
1. Read `ipdPath` → `ipdBuf`.
2. Collect all faces from `chunk.objects[*].meshes[*].faces` where `isGlobal == false` → call `PatchFaces(ipdBuf, localFaces, false)`.
3. Collect all global faces → if any and `glbPath` non-empty, read `glbPath` → `glbBuf` → `PatchFaces(glbBuf, globalFaces, true)`.
4. Atomic write: write `ipdBuf` to a temp file, rename over `ipdPath`. Same for GLB.

> [!IMPORTANT]
> The atomic write (write-to-temp, then rename) is essential. A crash mid-write would otherwise corrupt the workspace file.

**`EncodeFace()`** reverse-UV logic:
```
// Denormalise: float [0,1] → uint8 space
float rawFU[4], rawFV[4];
for i in 0..numVerts:
    rawFU[i] = face.uv[i][0] * 256.0f;   // undo / 256 normalisation
    rawFV[i] = (1.0f - face.uv[i][1]) * 256.0f;  // undo Y-flip

// Determine which vertex received the +1 bias (the one with the highest U or V
// that is strictly greater than all others → that vertex had bias applied)
// Reverse bias: if biased vertex detected, subtract 1 before rounding
// Round to nearest integer, clamp to [0, 255]
// Write back pk->u0..u3, v0..v3

// Re-encode CBA: patch palette row into bits [14:6], preserve rest of cbaRaw
uint16_t newCba = (face.cbaRaw & ~0x7FC0) | ((face.paletteRow & 0xFF) << 6);
write16(buf, face.addr.packRawOffset + offsetof(PLM_PACK_HEADER, cba), newCba);

// Re-encode texnum: patch low 7 bits, preserve high bit (unk2)
uint8_t origByte = buf[face.addr.packRawOffset + offsetof(PLM_PACK_HEADER, tex_num_and_unk2_byte)];
uint8_t newByte  = (origByte & 0x80) | (face.texNum & 0x7F);
buf[face.addr.packRawOffset + offsetof(PLM_PACK_HEADER, tex_num_and_unk2_byte)] = newByte;
```

**`RelocateIPDOffsets()` and `RelocatePLMOffsets()`** — stubs that `printf("[IpdWriter] Relocation not yet implemented")` and return. These are the hooks for Phase 2 future work (vertex/face addition).

**`ResolveFaceOffset()`** — navigates the buffer by reading `PLM_FILE_HEADER → PLM_OBJ_HEADER[name] → PLM_DATA_HEADER[meshIdx] → pack_offset + packIdx * sizeof(PLM_PACK_HEADER)`. Used to refresh `packRawOffset` after future relocations, and as a consistency check in debug builds.

---

## Phase 3 — EditHistory (Global Undo/Redo)

### `EditHistory.h`

```cpp
#pragma once
#include "IpdParser.h"
#include <deque>
#include <string>

struct FaceSnapshot {
    std::string chunkName;   // which LoadedChunk this face came from
    int         objectIdx;   // index into chunk.objects
    int         meshIdx;
    int         faceIdx;
    RenderFace  before;      // full copy of face state before edit
    RenderFace  after;       // full copy of face state after edit
    std::string description; // human-readable: "UV edit THR0000 obj 0 mesh 1 face 3"
};

class EditHistory {
public:
    explicit EditHistory(int maxDepth = 50);

    // Call AFTER applying the edit to the live ParsedChunk in memory.
    // Clears the redo stack.
    void Push(FaceSnapshot snap);

    // Apply 'before' state back to the live face, triggers GPU batch rebuild.
    // Returns false if nothing to undo.
    bool Undo(class Viewport3D& vp, const std::string& workspaceDir);
    bool Redo(class Viewport3D& vp, const std::string& workspaceDir);

    bool CanUndo() const { return !m_undo.empty(); }
    bool CanRedo() const { return !m_redo.empty(); }

    const std::string& PeekUndoDesc() const; // for tooltip display
    const std::string& PeekRedoDesc() const;

    void SetMaxDepth(int depth);
    int  GetMaxDepth() const { return m_maxDepth; }
    void Clear();

private:
    int                    m_maxDepth;
    std::deque<FaceSnapshot> m_undo;
    std::deque<FaceSnapshot> m_redo;

    // Restore a face in the viewport using a FaceSnapshot state.
    // Locates the live face via chunkName + objectIdx + meshIdx + faceIdx.
    static RenderFace* FindLiveFace(class Viewport3D& vp,
                                    const FaceSnapshot& snap);
};
```

**Key behaviour**:
- `Push`: appends to `m_undo`, clears `m_redo`, pops front if `m_undo.size() > m_maxDepth`.
- `Undo`: pops from `m_undo`, restores `before` to the live face, pushes to `m_redo`, calls `vp.RebuildChunkBatches(chunkName, workspaceDir)`.
- `Redo`: pops from `m_redo`, applies `after`, pushes to `m_undo`.
- The history operates by **chunkName** (the string name like `"THR0000"`), not by pointer — chunk pointers can be invalidated by unload/reload.

---

## Phase 4 — UI Wiring in `main.cpp`

### Remove `saveToJson` lambda
Delete lines 645–705 (the `saveToJson` lambda, "Save Changes to JSON" button, and Ctrl-S binding to it).

### Add `EditHistory history` instance
Declared at the top of `main()`, lifetime spans the loop.

### Snapshot wiring
Wrap every face modification site (UV drag, palette slider, "Assign Current Texture to Face") with:
```cpp
RenderFace snapBefore = *activeFace;   // copy before
// ... apply edit ...
RenderFace snapAfter = *activeFace;    // copy after
history.Push({
    viewport3D.m_selectedChunk,
    viewport3D.m_selectedObjectIdx,
    viewport3D.m_selectedMeshIdx,
    viewport3D.m_selectedFaceIdx,
    snapBefore, snapAfter,
    "UV edit " + viewport3D.m_selectedChunk + " ..."
});
```

### Keyboard shortcuts
```cpp
if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
    // Save C++ path
    std::string ipdPath  = workspaceDir + "/chunks/"   + chunkName + ".IPD";
    std::string glbPath  = workspaceDir + "/geometry/" + chunkPrefix + "_GLB.PLM";
    int count = 0;
    bool ok = IpdWriter::WriteChunk(ipdPath, glbPath, *chunk, &count);
    pipelineManager.Log(ok ? "[SAVE] Wrote " + chunkName + ".IPD (" + std::to_string(count) + " faces patched)"
                           : "[SAVE] FAILED to write " + chunkName + ".IPD", !ok);
}
if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) { history.Undo(viewport3D, workspaceDir); }
if (ImGui::GetIO().KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y, false) ||
    (ImGui::GetIO().KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false)))) { history.Redo(viewport3D, workspaceDir); }
```

### UI buttons in Texture Manager panel
```
[Undo (Ctrl-Z)]   [Redo (Ctrl-Y)]   |   [Save Chunk (Ctrl-S)]   [Validate]
```
- Undo/Redo buttons greyed when stack is empty; tooltip shows `PeekUndoDesc()`.
- "Save Chunk" always visible; operates on the chunk whose face is currently selected.
- **Keep** the old "Save Changes to JSON" button temporarily, now labelled **"[Legacy] Export face to JSON"** — for side-by-side comparison testing only.

### Undo depth setting in Configuration panel (ChunkManager)
Add a slider/input to `ChunkManager::Draw()` → Configuration header:
```cpp
ImGui::Text("Undo Depth:");
ImGui::SameLine(labelWidth);
int depth = history.GetMaxDepth();
if (ImGui::SliderInt("##UndoDepth", &depth, 1, 200)) {
    history.SetMaxDepth(depth);
}
```
`history` must be accessible — pass it in as a reference or expose via a global (the existing architecture already uses a pattern similar to this for `ChunkManager`).

---

## Phase 5 — Comparison Testing (JSON vs C++)

Before removing the JSON pipeline:
1. Load a chunk (`THR0000`) in the editor.
2. Make a UV edit, then:
   - **[Legacy] Export face to JSON** → run `json_to_ipd.py` → compare output `.IPD` against original.
   - **Save Chunk (C++)** → compare output `.IPD` against original.
3. Diff the two output files byte-for-byte. They should agree on all modified pack bytes and be identical elsewhere.
4. Once parity is confirmed, file a note in `CHANGELOG.md` and await deletion instruction.

---

## Phase 6 — Validation Stub

```cpp
std::vector<std::string> IpdWriter::Validate(const ParsedChunk& chunk) {
    std::vector<std::string> warnings;
    // Future checks:
    // - File size > 256KB (patched engine limit) → warning
    // - UV coordinates outside [0,255] range
    // - Face vertex indices ≥ mesh.vert_num
    // - texNum 0x7F on a textured face, etc.
    return warnings; // always empty for now
}
```

---

## Open Questions / Deferred

| Topic | Status |
|-------|--------|
| Vertex addition/removal (PLM section rebuild) | Deferred — relocation stubs in `IpdWriter` preserve the path |
| Sub-pixel UV precision audit | Deferred — `rawU/rawV` fields preserved for future analysis |
| Full re-serialiser (for structural IPD rebuild) | Explicitly not planned; section patcher is the ceiling |
| `ExportToJson/Obj/Reconstruct` deletion | Awaiting user instruction after parity testing |
| `meshCache` dead code removal | Phase 1, minor |
