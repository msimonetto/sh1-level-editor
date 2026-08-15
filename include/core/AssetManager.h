#pragma once
#include <string>
#include <vector>
#include <functional>

class AssetManager {
public:
    using LogCallback = std::function<void(const std::string& message, bool isError)>;
    using ProgressCallback = std::function<void(int current, int total, const std::string& operation)>;

    AssetManager() = default;
    ~AssetManager() = default;

    void SetLogCallback(LogCallback cb) { m_logCallback = cb; }
    void SetProgressCallback(ProgressCallback cb) { m_progressCallback = cb; }

    bool ExtractToWorkspace(const std::vector<std::string>& chunks, const std::string& completeDir, const std::string& workspaceDir, const std::string& projectDir);
    bool DeployToTarget(const std::vector<std::string>& chunks, const std::string& workspaceDir, const std::string& overrideDir, const std::string& projectDir);
    bool DeleteSelected(const std::string& targetType, const std::vector<std::string>& chunks, bool deleteTextures, const std::string& workspaceDir, const std::string& overrideDir, const std::string& projectDir);
    bool ClearEntire(const std::string& targetType, const std::string& workspaceDir, const std::string& overrideDir, const std::string& projectDir);
    bool RevertSelected(const std::vector<std::string>& chunks, bool revertDependencies, const std::string& workspaceDir, const std::string& assetsDir);
    bool DeployOverlayToDecomp(const std::string& mapKey);

private:
    void Log(const std::string& msg, bool isError = false) const {
        if (m_logCallback) {
            m_logCallback(msg, isError);
        }
    }

    LogCallback m_logCallback;
    ProgressCallback m_progressCallback;
};
