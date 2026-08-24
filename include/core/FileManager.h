#pragma once
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <filesystem>
#include "formats/Structs.h"
#include "formats/OBJExport.h"
#include "core/Patcher.h"

struct ChunkInfo {
    std::string name;
    std::string prefix;
    int x;
    int z;
    bool hasCoords;
};

class FileManager {
    friend class ChunksPanel; // Grants UI access to internal state
public:
    using LogCallback = std::function<void(const std::string& message, bool isError)>;
    using ProgressCallback = std::function<void(int current, int total, const std::string& operation)>;

    FileManager();
    ~FileManager() = default;

    void SetLogCallback(LogCallback cb) { m_logCallback = cb; }
    void SetProgressCallback(ProgressCallback cb) { m_progressCallback = cb; }

    std::vector<std::string> GetSelectedChunks() const { return m_selectedChunks; }
    void SetSelectedChunks(const std::vector<std::string>& chunks) { m_selectedChunks = chunks; }
    std::string GetSelectedPrefix() const { return m_selectedPrefix; }
    std::vector<std::string> GetViewportChunks() const { return m_viewportChunks; }
    void SetViewportChunks(const std::vector<std::string>& chunks) { m_viewportChunks = chunks; }
    void QueueReloadChunks(const std::vector<std::string>& chunks);
    std::vector<std::string> ConsumeReloadChunks();

    std::string GetWorkspaceDir() const;
    std::string GetAssetsDir() const;
    std::string GetOverrideDir() const;
    std::string GetBuildDir() const;
    std::string GetGameBinSource() const;

    void ScanAssets();
    void Log(const std::string& msg, bool isError = false);

    // Native file operations (Replacing python scripts)
    bool ExtractToWorkspace(const std::vector<std::string>& chunks, const std::string& completeDir, const std::string& workspaceDir, const std::string& projectDir);
    bool DeployToTarget(const std::vector<std::string>& chunks, const std::string& workspaceDir, const std::string& overrideDir, const std::string& projectDir);
    bool DeleteSelected(const std::string& targetType, const std::vector<std::string>& chunks, bool deleteTextures, const std::string& workspaceDir, const std::string& overrideDir, const std::string& projectDir);
    bool ClearEntire(const std::string& targetType, const std::string& workspaceDir, const std::string& overrideDir, const std::string& projectDir);
    bool RevertSelected(const std::vector<std::string>& chunks, bool revertDependencies, const std::string& workspaceDir, const std::string& assetsDir);
    bool ExportToOBJ(const std::vector<std::string>& chunks, const std::string& workspaceDir, const std::string& assetsDir, const std::string& projectDir);
    
    // Kept as script invocation since it deals with C generation
    bool DeployOverlayToDecomp(const std::string& mapKey);

private:
    std::string GetSHA256(const std::filesystem::path& filepath) const;
    std::filesystem::path FindAssetPath(const std::filesystem::path& assetsDir, const std::string& filename) const;

    std::vector<std::string> m_consoleLines;
    std::mutex m_consoleMutex;

    // Chunk & UI state (accessed by ChunksPanel via friend)
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

    LogCallback m_logCallback;
    ProgressCallback m_progressCallback;
};
