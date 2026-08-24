#pragma once
#include <string>
#include <vector>
#include "formats/IPDParse.h"

class FileManager;
class DependencyManager;

class OBJExport {
public:
    OBJExport() = default;
    ~OBJExport() = default;

    // Exports a ParsedChunk to a Wavefront .obj file with companion .mtl and baked .png textures
    static bool ExportChunk(const ParsedChunk& chunk,
                            const std::string& outObjPath,
                            const std::string& workspaceDir,
                            const std::string& assetsDir = "",
                            bool exportCollision = true);

    // Legacy / inspector export interface
    static bool Export(const FileManager& inspector,
                       const DependencyManager& depMgr,
                       const std::string& outPath,
                       bool exportCollision = true);
};
