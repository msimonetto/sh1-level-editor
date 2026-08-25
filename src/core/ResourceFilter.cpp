#include "core/ResourceFilter.h"
#include "core/FileManager.h"
#include "core/DependencyManager.h"
#include "formats/IPDParse.h"
#include <filesystem>
#include <algorithm>
#include <set>

namespace fs = std::filesystem;

static bool HasTimExtension(const fs::path& p) {
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".tim";
}

static bool MatchesPrefix(const std::string& name, const std::string& prefix) {
    if (prefix.empty()) return true;
    return name.rfind(prefix, 0) == 0;
}

std::vector<std::string> ResourceFilter::GetAssetTextures(const FileManager& fileManager, const std::string& prefixFilter) {
    std::set<std::string> results;
    std::string assetsDir = fileManager.GetAssetsDir();
    if (assetsDir.empty() || !fs::exists(assetsDir)) {
        return {};
    }

    // Binary extracted assets are typically stored in the BG directory
    std::vector<fs::path> searchDirs = {
        fs::path(assetsDir) / "BG",
        fs::path(assetsDir) / "TIM",
        fs::path(assetsDir)
    };

    for (const auto& dir : searchDirs) {
        if (fs::exists(dir) && fs::is_directory(dir)) {
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_regular_file() && HasTimExtension(entry.path())) {
                    std::string stem = entry.path().stem().string();
                    if (MatchesPrefix(stem, prefixFilter)) {
                        results.insert(stem);
                    }
                }
            }
        }
    }

    // Future extension: inter-prefix asset transfer / cross-map dependency tracking
    return std::vector<std::string>(results.begin(), results.end());
}

std::vector<std::string> ResourceFilter::GetWorkspaceTextures(const FileManager& fileManager, const std::string& prefixFilter) {
    std::set<std::string> results;
    std::string workspaceDir = fileManager.GetWorkspaceDir();
    fs::path texDir = fs::path(workspaceDir) / "TIM";

    if (fs::exists(texDir) && fs::is_directory(texDir)) {
        for (const auto& entry : fs::directory_iterator(texDir)) {
            if (entry.is_regular_file() && HasTimExtension(entry.path())) {
                std::string stem = entry.path().stem().string();
                if (MatchesPrefix(stem, prefixFilter)) {
                    results.insert(stem);
                }
            }
        }
    }

    // Future extension: inter-prefix asset transfer / cross-map dependency tracking
    return std::vector<std::string>(results.begin(), results.end());
}

std::vector<std::string> ResourceFilter::GetSelectedChunksTextures(const FileManager& fileManager, const DependencyManager& depManager, const std::string& prefixFilter) {
    std::set<std::string> results;
    std::vector<std::string> selectedChunks = fileManager.GetSelectedChunks();
    if (selectedChunks.empty()) {
        return {};
    }

    std::set<std::string> textures = depManager.GetTexturesForChunks(selectedChunks);
    for (const auto& tex : textures) {
        if (MatchesPrefix(tex, prefixFilter)) {
            results.insert(tex);
        }
    }

    return std::vector<std::string>(results.begin(), results.end());
}

std::vector<std::string> ResourceFilter::GetChunkTextures(const ParsedChunk* chunk, const std::string& prefixFilter) {
    if (!chunk) return {};

    std::set<std::string> results;
    for (const auto& tex : chunk->localTexNames) {
        if (MatchesPrefix(tex, prefixFilter)) {
            results.insert(tex);
        }
    }
    for (const auto& tex : chunk->globalTexNames) {
        if (MatchesPrefix(tex, prefixFilter)) {
            results.insert(tex);
        }
    }

    return std::vector<std::string>(results.begin(), results.end());
}

std::vector<std::string> ResourceFilter::GetMeshTextures(const RenderMesh* mesh, const ParsedChunk* chunk, const RenderObject* obj, const std::string& prefixFilter) {
    if (!mesh) return {};

    std::set<std::string> results;
    const std::vector<std::string>* texList = nullptr;
    if (chunk && obj) {
        texList = obj->isGlobal ? &chunk->globalTexNames : &chunk->localTexNames;
    }

    for (const auto& face : mesh->faces) {
        if (!face.texName.empty()) {
            if (MatchesPrefix(face.texName, prefixFilter)) {
                results.insert(face.texName);
            }
        } else if (texList && face.texNum < texList->size()) {
            const std::string& tName = (*texList)[face.texNum];
            if (MatchesPrefix(tName, prefixFilter)) {
                results.insert(tName);
            }
        }
    }

    return std::vector<std::string>(results.begin(), results.end());
}

std::string ResourceFilter::ResolveTexturePath(const FileManager& fileManager, const std::string& texName) {
    if (texName.empty()) return "";

    std::string workDir = fileManager.GetWorkspaceDir();
    fs::path workPath = fs::path(workDir) / "TIM" / (texName + ".TIM");
    if (fs::exists(workPath)) return workPath.string();

    fs::path workPathLower = fs::path(workDir) / "TIM" / (texName + ".tim");
    if (fs::exists(workPathLower)) return workPathLower.string();

    std::string assetsDir = fileManager.GetAssetsDir();
    if (!assetsDir.empty()) {
        std::vector<fs::path> assetPaths = {
            fs::path(assetsDir) / "BG" / (texName + ".TIM"),
            fs::path(assetsDir) / "BG" / (texName + ".tim"),
            fs::path(assetsDir) / "TIM" / (texName + ".TIM"),
            fs::path(assetsDir) / "TIM" / (texName + ".tim"),
            fs::path(assetsDir) / (texName + ".TIM"),
            fs::path(assetsDir) / (texName + ".tim")
        };
        for (const auto& p : assetPaths) {
            if (fs::exists(p)) return p.string();
        }
    }

    return workPath.string();
}
