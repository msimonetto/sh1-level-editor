#pragma once

#include <string>
#include <vector>
#include <set>

class FileManager;
class DependencyManager;
struct ParsedChunk;
struct RenderMesh;
struct RenderObject;

class ResourceFilter {
public:
    // Future extension: Inter-prefix asset transfers and dependency resolution
    // will hook into these query routines when managing cross-map asset sharing.

    // Queries all TIM textures available in the game's assets (checks BG and TIM directories)
    static std::vector<std::string> GetAssetTextures(const FileManager& fileManager, const std::string& prefixFilter = "");

    // Queries all TIM textures currently present in the active workspace
    static std::vector<std::string> GetWorkspaceTextures(const FileManager& fileManager, const std::string& prefixFilter = "");

    // Queries textures referenced by the currently selected chunks
    static std::vector<std::string> GetSelectedChunksTextures(const FileManager& fileManager, const DependencyManager& depManager, const std::string& prefixFilter = "");

    // Queries local and global textures for a specific parsed chunk
    static std::vector<std::string> GetChunkTextures(const ParsedChunk* chunk, const std::string& prefixFilter = "");

    // Queries textures assigned to faces in a specific mesh
    static std::vector<std::string> GetMeshTextures(const RenderMesh* mesh, const ParsedChunk* chunk, const RenderObject* obj, const std::string& prefixFilter = "");

    // Resolves a texture stem name to its absolute file path (checking workspace first, then assets/BG)
    static std::string ResolveTexturePath(const FileManager& fileManager, const std::string& texName);
};
