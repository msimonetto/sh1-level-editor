#pragma once
#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "viewport/ViewportBase.h"
#include "formats/IPDParse.h"
#include "core/Textures.h"
#include "raylib.h"

// ---------------------------------------------------------------------------
// GlobalGeometryViewport
//
// Integrated PLM Object Manager for the Unified C++ Editor.
// Inherits from ViewportBase to leverage the background color,
// 40x40 grid, billboarded XYZ axis labels, post-processing dither shader,
// and orbital camera navigation.
// ---------------------------------------------------------------------------

class GlobalGeometryViewport : public ViewportBase {
public:
    GlobalGeometryViewport();
    ~GlobalGeometryViewport() override;

    // Custom 2-column panel layout
    void Draw();

    void SetWorkspaceDir(const std::string& dir);

protected:
    // ViewportBase virtual interface
    void DrawViewportGrid() override;
    void DrawScene() override;
    void DrawContextMenu() override;
    size_t GetChunkCount() const override { return m_objects.empty() ? 0 : 1; }

private:
    struct TextureSlot {
        std::string name;
        uint8_t palRow = 0;
    };

    struct PlmEntry {
        std::string  name;        // e.g. "THR1702"
        int          mesh_id = 0; // position index in PLM obj_headers
        int          pack_count = 0;
        int          vertex_count = 0;
        int          estimated_bytes = 0;
        bool         is_dirty = false;
        
        // Textures actually used by THIS specific object
        std::vector<TextureSlot> used_textures;

        RenderObject render_obj;
        BoundingBox  bounds;
    };

    struct ObjectChunkRef {
        std::string chunkName;    // e.g. "THR0000"
        int         instanceCount = 0;
        bool        isInWorkspace = false;
    };

    // Configuration & State
    std::string m_workspaceDir;
    std::string m_currentFile = "THR_GLB";
    std::vector<std::string> m_availableFiles;
    std::string m_loadedGlbPath;
    std::string m_statusMsg;

    std::vector<PlmEntry> m_objects;
    int                   m_selectedIdx = -1;

    // Per-object dependency index: plm_name -> list of referencing chunks across assets & workspace
    std::map<std::string, std::vector<ObjectChunkRef>> m_objectDependencies;
    std::atomic<bool> m_depIndexBuilding{false};
    std::mutex        m_depMutex;
    std::thread       m_depThread;

    // GPU rendering batches for currently selected object
    std::vector<GpuBatch> m_selectedBatches;

    // Private UI & Logic
    void DrawLeftPanel(float width);

    void DrawFileSelector();
    void DrawObjectList();
    void DrawSelectedDetails();
    void DrawDependencyPanel();
    void DrawCapacityBar();

    void AutoLoadPlmFile(const std::string& filename);
    void BuildDependencyIndex();
    void RebuildGpuBatches(int idx);
    void ClearGpuBatches();

    static int EstimateObjectBytes(const PlmEntry& e);

    static constexpr int PLM_FILE_HEADER_SIZE = 20;
    static constexpr int PLM_OBJ_HEADER_SIZE  = 20;
    static constexpr int PLM_DATA_HEADER_SIZE = 16;
    static constexpr int PLM_PACK_HEADER_SIZE = 16;
    static constexpr int PLM_VERTEX_XY_SIZE   = 4;
    static constexpr int PLM_VERTEX_Z_SIZE    = 2;
    static constexpr int TEX_NAME_ENTRY_SIZE  = 24;

    static constexpr float CAPACITY_WARN_KB = 80.f;
    static constexpr float CAPACITY_MAX_KB  = 120.f;
};
