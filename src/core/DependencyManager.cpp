#include "core/DependencyManager.h"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace fs = std::filesystem;

DependencyManager::DependencyManager(const std::string& workspaceDir)
    : m_workspaceDir(workspaceDir) {
    Load();
}

void DependencyManager::Load() {
    m_dependenciesData.clear();
    fs::path depsPath = fs::path(m_workspaceDir) / "dependencies.json";
    if (fs::exists(depsPath)) {
        std::ifstream file(depsPath);
        if (file.is_open()) {
            try {
                nlohmann::json root;
                file >> root;
                for (auto it = root.begin(); it != root.end(); ++it) {
                    m_dependenciesData[it.key()] = it.value();
                }
            } catch (const nlohmann::json::parse_error& e) {
                std::cerr << "[DependencyManager] JSON Parse Error: " << e.what() << "\n";
            }
        }
    }
}

void DependencyManager::Save() {
    fs::path depsPath = fs::path(m_workspaceDir) / "dependencies.json";
    std::ofstream file(depsPath);
    if (file.is_open()) {
        nlohmann::json root;
        for (const auto& [chunkName, data] : m_dependenciesData) {
            root[chunkName] = data;
        }
        file << root.dump(4);
    }
}

void DependencyManager::LoadIPDDependencies(const std::string& prefix, const std::vector<std::string>& ipdNames) {
    Load(); // Reload from disk in case it was modified by a background thread
    
    m_activeTextures.clear();
    m_activePLMs.clear();

    for (const auto& ipdName : ipdNames) {
        std::string chunkKey = ipdName;
        // Strip .IPD extension if present
        if (chunkKey.length() > 4 && chunkKey.substr(chunkKey.length() - 4) == ".IPD") {
            chunkKey = chunkKey.substr(0, chunkKey.length() - 4);
        } else if (chunkKey.length() > 4 && chunkKey.substr(chunkKey.length() - 4) == ".ipd") {
            chunkKey = chunkKey.substr(0, chunkKey.length() - 4);
        }

        if (m_dependenciesData.find(chunkKey) != m_dependenciesData.end()) {
            const auto& chunkData = m_dependenciesData[chunkKey];
            if (chunkData.contains("textures")) {
                for (const auto& tex : chunkData["textures"]) {
                    std::string texName = tex.get<std::string>();
                    // active textures expect names without extension
                    if (texName.length() > 4 && texName.substr(texName.length() - 4) == ".TIM") {
                        m_activeTextures.insert(texName.substr(0, texName.length() - 4));
                    } else if (texName.length() > 4 && texName.substr(texName.length() - 4) == ".tim") {
                        m_activeTextures.insert(texName.substr(0, texName.length() - 4));
                    } else {
                        m_activeTextures.insert(texName);
                    }
                }
            }
            if (chunkData.contains("geometry")) {
                for (const auto& geom : chunkData["geometry"]) {
                    m_activePLMs.insert(geom.get<std::string>());
                }
            }
        }
    }
}

void DependencyManager::AddDependency(const std::string& prefix, const std::string& chunkName, const std::string& dependencyType, const std::string& dependencyFile) {
    if (m_dependenciesData.find(chunkName) == m_dependenciesData.end()) {
        m_dependenciesData[chunkName] = nlohmann::json::object();
    }
    
    if (!m_dependenciesData[chunkName].contains(dependencyType)) {
        m_dependenciesData[chunkName][dependencyType] = nlohmann::json::array();
    }
    
    bool exists = false;
    for (const auto& item : m_dependenciesData[chunkName][dependencyType]) {
        if (item.get<std::string>() == dependencyFile) {
            exists = true;
            break;
        }
    }
    
    if (!exists) {
        m_dependenciesData[chunkName][dependencyType].push_back(dependencyFile);
        Save();
    }
}

void DependencyManager::RemoveDependency(const std::string& prefix, const std::string& chunkName, const std::string& dependencyType, const std::string& dependencyFile) {
    if (m_dependenciesData.find(chunkName) != m_dependenciesData.end()) {
        if (m_dependenciesData[chunkName].contains(dependencyType)) {
            auto& arr = m_dependenciesData[chunkName][dependencyType];
            auto newEnd = std::remove_if(arr.begin(), arr.end(), [&](const nlohmann::json& item) {
                return item.get<std::string>() == dependencyFile;
            });
            if (newEnd != arr.end()) {
                arr.erase(newEnd, arr.end());
                Save();
            }
        }
    }
}

std::set<std::string> DependencyManager::GetSharedFiles(const std::vector<std::string>& excludeChunks) const {
    std::set<std::string> shared;
    for (const auto& [chunkName, data] : m_dependenciesData) {
        bool skip = false;
        for (const auto& excl : excludeChunks) {
            if (excl == chunkName || excl + ".IPD" == chunkName || excl + ".ipd" == chunkName) {
                skip = true;
                break;
            }
        }
        if (skip) continue;

        if (data.contains("textures")) {
            for (const auto& tex : data["textures"]) {
                shared.insert(tex.get<std::string>());
            }
        }
        if (data.contains("geometry")) {
            for (const auto& geom : data["geometry"]) {
                shared.insert(geom.get<std::string>());
            }
        }
    }
    return shared;
}
