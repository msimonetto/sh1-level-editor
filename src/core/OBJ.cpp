#include "core/FileManager.h"
#include "core/DependencyManager.h"
#include <fstream>

bool OBJ::Export(const FileManager& inspector, const DependencyManager& depMgr, const std::string& outPath, bool exportCollision) {
    // Removed empty check since Inspector now aggregates files

    std::ofstream objFile(outPath);
    if (!objFile.is_open()) {
        return false;
    }

    // Generate MTL file path
    std::string mtlPath = outPath;
    size_t extPos = mtlPath.find_last_of('.');
    if (extPos != std::string::npos) {
        mtlPath = mtlPath.substr(0, extPos) + ".mtl";
    } else {
        mtlPath += ".mtl";
    }

    // Generate MTL file name for the OBJ header
    std::string mtlFileName = mtlPath;
    size_t slashPos = mtlFileName.find_last_of("/\\");
    if (slashPos != std::string::npos) {
        mtlFileName = mtlFileName.substr(slashPos + 1);
    }

    objFile << "# Exported by Unified C++ Editor\n";
    objFile << "mtllib " << mtlFileName << "\n\n";

    // Write materials to MTL
    std::ofstream mtlFile(mtlPath);
    if (mtlFile.is_open()) {
        mtlFile << "# Exported by Unified C++ Editor\n";
        for (const auto& tex : depMgr.GetActiveTextures()) {
            mtlFile << "\nnewmtl mat_" << tex << "\n";
            mtlFile << "Ka 1.0 1.0 1.0\n";
            mtlFile << "Kd 1.0 1.0 1.0\n";
            mtlFile << "Ks 0.0 0.0 0.0\n";
            mtlFile << "map_Kd " << tex << ".png\n";
        }
        mtlFile.close();
    }

    // Since IpdInspector currently only keeps counts of vertices, we need to extend it 
    // or pass raw data to actually export the OBJ correctly.
    // For now, this is a skeleton implementation.
    
    // TODO: Iterate through actual vertex data and polygons and write 'v', 'vt', 'vn', 'f'.
    
    objFile.close();
    return true;
}
