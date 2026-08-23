#pragma once

#include <string>
#include <map>
#include <vector>
#include <set>
#include <filesystem>
#include <nlohmann/json.hpp>

class DependencyManager {
public:
    DependencyManager(const std::string& workspaceDir);
    ~DependencyManager() = default;

    void Load();
    void Save();

    // Loads dependencies for the given IPDs into the active sets
    void LoadIPDDependencies(const std::string& prefix, const std::vector<std::string>& ipdNames);
    
    // The current active set of dependencies for the selected IPDs
    const std::set<std::string>& GetActiveTextures() const { return m_activeTextures; }
    const std::set<std::string>& GetActivePLMs() const { return m_activePLMs; }

    // Add or remove a custom dependency for a chunk
    void AddDependency(const std::string& prefix, const std::string& chunkName, const std::string& dependencyType, const std::string& dependencyFile);
    void RemoveDependency(const std::string& prefix, const std::string& chunkName, const std::string& dependencyType, const std::string& dependencyFile);
    
    // Check if a file is shared among other chunks
    std::set<std::string> GetSharedFiles(const std::vector<std::string>& excludeChunks) const;

    std::map<std::string, nlohmann::json> m_dependenciesData; // chunkName -> json object

private:
    std::string m_workspaceDir;
    std::set<std::string> m_activeTextures;
    std::set<std::string> m_activePLMs;
};
