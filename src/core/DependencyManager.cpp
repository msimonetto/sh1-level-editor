#include "core/DependencyManager.h"
#include <fstream>

namespace fs = std::filesystem;

DependencyManager::DependencyManager(const std::string& workspaceDir)
    : m_workspaceDir(workspaceDir) {
    Load();
}

void DependencyManager::Load() {
    LoadJSON(m_workspaceDir + "/dependencies.json", m_dependencies);
    LoadJSON(m_workspaceDir + "/dependents.json", m_dependents);
}

void DependencyManager::Save() {
    SaveJSON(m_workspaceDir + "/dependencies.json", m_dependencies);
    SaveJSON(m_workspaceDir + "/dependents.json", m_dependents);
}

void DependencyManager::LoadIPDDependencies(const std::string& prefix, const std::vector<std::string>& ipdNames) {
    m_activeTextures.clear();
    m_activePLMs.clear();

    if (m_dependencies.find(prefix) == m_dependencies.end()) {
        return;
    }

    const auto& prefixDeps = m_dependencies[prefix];
    for (const auto& ipdName : ipdNames) {
        std::string ipdKey = ipdName;
        if (ipdKey.find(".IPD") == std::string::npos && ipdKey.find(".ipd") == std::string::npos) {
            ipdKey += ".IPD"; 
        }

        if (prefixDeps.find(ipdKey) != prefixDeps.end()) {
            for (const auto& dep : prefixDeps.at(ipdKey)) {
                if (dep.find(".TIM") != std::string::npos || dep.find(".tim") != std::string::npos) {
                    m_activeTextures.insert(dep.substr(0, dep.length() - 4));
                } else if (dep.find(".PLM") != std::string::npos || dep.find(".plm") != std::string::npos) {
                    m_activePLMs.insert(dep);
                }
            }
        }
    }
}

void DependencyManager::LoadJSON(const std::string& filepath, std::map<std::string, std::map<std::string, std::vector<std::string>>>& targetMap) {
    targetMap.clear();
    std::ifstream file(filepath);
    if (!file.is_open()) return;
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    std::string currentPrefix = "";
    std::string currentAsset = "";
    int depth = 0;
    
    size_t pos = 0;
    while (pos < content.length()) {
        char c = content[pos];
        if (c == '{') { depth++; pos++; }
        else if (c == '}') { depth--; pos++; }
        else if (c == '[') { depth++; pos++; }
        else if (c == ']') { depth--; pos++; }
        else if (c == '\"') {
            size_t endPos = content.find("\"", pos + 1);
            if (endPos == std::string::npos) break;
            std::string str = content.substr(pos + 1, endPos - pos - 1);
            
            size_t nextChar = content.find_first_not_of(" \t\n\r", endPos + 1);
            if (nextChar != std::string::npos && content[nextChar] == ':') {
                if (depth == 1) {
                    currentPrefix = str;
                } else if (depth == 2) {
                    currentAsset = str;
                }
                pos = nextChar + 1;
            } else {
                if (depth == 3) {
                    targetMap[currentPrefix][currentAsset].push_back(str);
                }
                pos = endPos + 1;
            }
        } else {
            pos++;
        }
    }
}

void DependencyManager::SaveJSON(const std::string& filepath, const std::map<std::string, std::map<std::string, std::vector<std::string>>>& sourceMap) {
    std::ofstream file(filepath);
    if (!file.is_open()) return;
    
    file << "{\n";
    bool firstPrefix = true;
    for (const auto& prefixPair : sourceMap) {
        if (!firstPrefix) file << ",\n";
        file << "  \"" << prefixPair.first << "\": {\n";
        
        bool firstAsset = true;
        for (const auto& assetPair : prefixPair.second) {
            if (!firstAsset) file << ",\n";
            file << "    \"" << assetPair.first << "\": [\n";
            
            bool firstDep = true;
            for (const auto& dep : assetPair.second) {
                if (!firstDep) file << ",\n";
                file << "      \"" << dep << "\"";
                firstDep = false;
            }
            
            file << "\n    ]";
            firstAsset = false;
        }
        
        file << "\n  }";
        firstPrefix = false;
    }
    file << "\n}\n";
}
