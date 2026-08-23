#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <set>
#include "core/AssetManager.h"
#include "formats/Structs.h"
#include "core/OBJ.h"
#include "core/Patcher.h"

class History; // forward decl (defined in core/History.h)

struct ChunkInfo {
    std::string name;
    std::string prefix;
    int x;
    int z;
    bool hasCoords;
};

class ChunkManager {
    friend class ChunksPanel; // Grants panels/Chunks.cpp access to private UI state
public:
    ChunkManager();
    ~ChunkManager() = default;

    std::vector<std::string> GetSelectedChunks() const { return m_selectedChunks; }
    void SetSelectedChunks(const std::vector<std::string>& chunks) { m_selectedChunks = chunks; }
    std::string GetSelectedPrefix() const { return m_selectedPrefix; }
    std::vector<std::string> GetViewportChunks()  const { return m_viewportChunks; }
    void SetViewportChunks(const std::vector<std::string>& chunks) { m_viewportChunks = chunks; }
    void QueueReloadChunks(const std::vector<std::string>& chunks) {
        std::lock_guard<std::mutex> lock(m_reloadMutex);
        m_reloadChunks.insert(m_reloadChunks.end(), chunks.begin(), chunks.end());
    }
    std::vector<std::string> ConsumeReloadChunks();
    std::string GetWorkspaceDir() const;
    std::string GetAssetsDir() const;
    std::string GetOverrideDir() const;

    void ScanAssets();

private:
    AssetManager m_pipeline;

    std::string GetBuildDir() const;
    std::string GetGameBinSource() const;

    std::vector<std::string> m_consoleLines;
    std::mutex m_consoleMutex;

    // Chunk & UI state (accessed by ChunkPanel via friend)
    std::vector<std::string> m_reloadChunks;
    std::mutex m_reloadMutex;
    std::map<std::string, std::vector<ChunkInfo>> m_parsedChunks;
    std::string m_selectedPrefix;
    std::vector<std::string> m_selectedChunks;
    std::vector<std::string> m_viewportChunks;  // grid selection → 3D viewport
    std::string m_lastClickedChunk;
    char m_aliasBuffer[256] = "";
    bool m_isDragSelecting = true;

    EnginePatcher::Version m_patchVersion = EnginePatcher::Version::USA;
    bool m_deleteTexturesFlag = true;
    bool m_revertDependenciesFlag = true;

    // Pipeline progress state
    std::mutex m_progressMutex;
    int m_progressCurrent = 0;
    int m_progressTotal = 0;
    std::string m_progressOp;

public:
    void Log(const std::string& msg, bool isError = false);
};