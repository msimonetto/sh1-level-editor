#pragma once
#include <string>
#include <vector>
class FileManager;
class DependencyManager;

class OBJ {
public:
    OBJ() = default;
    ~OBJ() = default;

    // Exports the IPD loaded in the inspector to an OBJ file
    static bool Export(const FileManager& inspector, const DependencyManager& depMgr, const std::string& outPath, bool exportCollision = true);
};
