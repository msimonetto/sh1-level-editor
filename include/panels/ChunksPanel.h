#pragma once

class FileManager;
class History;
class Dictionary;
class DependencyManager;

// ---------------------------------------------------------------------------
// ChunksPanel — ImGui rendering for the Chunks panel.
//
// Owns the "Chunks", "Console", and pipeline-progress overlay windows.
// Accesses FileManager state via friend-class relationship.
// ---------------------------------------------------------------------------
class ChunksPanel {
public:
    // Draw all Chunks-related ImGui windows for this frame.
    // mgr      : the FileManager holding all chunk state.
    // dict     : the Dictionary holding all alias state.
    // depMgr   : the DependencyManager.
    // history  : optional, for future per-panel undo-depth UI.
    static void Draw(FileManager& mgr, Dictionary& dict, DependencyManager& depMgr, History* editHistory = nullptr);

private:
    // Non-instantiable — all methods are static.
    ChunksPanel() = delete;

    static void DrawGrid(FileManager& mgr, Dictionary& dict);
};

