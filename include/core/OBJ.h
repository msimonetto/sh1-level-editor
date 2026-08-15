#pragma once
#include <string>
#include <vector>
class ChunkManager;
class DependencyManager;

class OBJ {
public:
    OBJ() = default;
    ~OBJ() = default;

    // Exports the IPD loaded in the inspector to an OBJ file
    static bool Export(const ChunkManager& inspector, const DependencyManager& depMgr, const std::string& outPath, bool exportCollision = true);
};
