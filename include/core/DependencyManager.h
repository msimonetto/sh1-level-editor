#pragma once

#include <string>
#include <map>
#include <vector>
#include <set>
#include <filesystem>

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

    // Full maps: Prefix -> (Asset Name -> List of dependencies/dependents)
    std::map<std::string, std::map<std::string, std::vector<std::string>>> m_dependencies;
    std::map<std::string, std::map<std::string, std::vector<std::string>>> m_dependents;

private:
    std::string m_workspaceDir;
    std::set<std::string> m_activeTextures;
    std::set<std::string> m_activePLMs;
    
    // Internal JSON parser
    void LoadJSON(const std::string& filepath, std::map<std::string, std::map<std::string, std::vector<std::string>>>& targetMap);
    void SaveJSON(const std::string& filepath, const std::map<std::string, std::map<std::string, std::vector<std::string>>>& sourceMap);
};
