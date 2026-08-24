#include "formats/OBJExport.h"
#include "formats/TIMDecoder.h"
#include "core/FileManager.h"
#include "core/DependencyManager.h"
#include "raylib.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <set>
#include <algorithm>

namespace fs = std::filesystem;

bool OBJExport::ExportChunk(const ParsedChunk& chunk,
                            const std::string& outObjPath,
                            const std::string& workspaceDir,
                            const std::string& assetsDir,
                            bool exportCollision)
{
    fs::path objPath = fs::path(outObjPath);
    fs::path outDir = objPath.parent_path();
    if (!outDir.empty()) {
        fs::create_directories(outDir);
    }

    std::string stem = objPath.stem().string();
    std::string mtlFileName = stem + ".mtl";
    fs::path mtlPath = outDir / mtlFileName;

    std::ofstream objFile(outObjPath);
    if (!objFile.is_open()) {
        return false;
    }

    objFile << "# Wavefront OBJ exported by Silent Hill Level Editor\n";
    objFile << "# Chunk: " << chunk.chunkName << " (Prefix: " << chunk.chunkPrefix 
            << ", Grid: " << (int)chunk.xPos << ", " << (int)chunk.yPos << ")\n";
    objFile << "mtllib " << mtlFileName << "\n\n";

    uint64_t vOffset = 0;
    uint64_t vtOffset = 0;
    std::set<std::pair<std::string, uint8_t>> usedMaterials;

    // Iterate through placed objects
    for (size_t objIdx = 0; objIdx < chunk.objects.size(); ++objIdx) {
        const auto& obj = chunk.objects[objIdx];
        std::string safeName = obj.name;
        // Could be overwritten repeatedly if the safeName is empty
        if (safeName.empty()) safeName = "obj";

        objFile << "o " << safeName << "_" << objIdx << "\n";
        objFile << "g " << safeName << "\n";

        for (size_t meshIdx = 0; meshIdx < obj.meshes.size(); ++meshIdx) {
            const auto& mesh = obj.meshes[meshIdx];
            size_t vertNum = mesh.vx.size();
            if (vertNum == 0) continue;

            // 1. Write vertices
            for (size_t vi = 0; vi < vertNum; ++vi) {
                objFile << "v " << mesh.vx[vi] << " " << mesh.vy[vi] << " " << mesh.vz[vi] << "\n";
            }

            // 2. Write UV coordinates (vt) for all faces in this mesh
            std::vector<uint64_t> faceVtStart(mesh.faces.size(), 0);
            uint64_t currentMeshVt = 0;
            for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
                const auto& face = mesh.faces[fi];
                faceVtStart[fi] = vtOffset + currentMeshVt + 1; // 1-based index
                bool isQuad = (face.v[3] != 0xFF);
                int count = isQuad ? 4 : 3;
                for (int k = 0; k < count; ++k) {
                    float u = face.uv[k][0];
                    float v = 1.0f - face.uv[k][1];
                    objFile << "vt " << u << " " << v << "\n";
                    currentMeshVt++;
                }
            }

            // 3. Write faces grouped by material
            std::string currentMat = "";
            for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
                const auto& face = mesh.faces[fi];
                std::string matName;
                if (!face.texName.empty()) {
                    matName = "mat_" + face.texName + "_pal" + std::to_string(face.paletteRow);
                    usedMaterials.insert({face.texName, face.paletteRow});
                } else {
                    matName = "mat_untextured";
                    usedMaterials.insert({"", 0});
                }

                if (matName != currentMat) {
                    objFile << "usemtl " << matName << "\n";
                    currentMat = matName;
                }

                uint64_t vtBase = faceVtStart[fi];
                bool isQuad = (face.v[3] != 0xFF);
                if (isQuad) {
                    uint64_t v0 = vOffset + face.v[0] + 1;
                    uint64_t v1 = vOffset + face.v[1] + 1;
                    uint64_t v2 = vOffset + face.v[2] + 1;
                    uint64_t v3 = vOffset + face.v[3] + 1;
                    objFile << "f " << v0 << "/" << (vtBase + 0) << " "
                                    << v1 << "/" << (vtBase + 1) << " "
                                    << v2 << "/" << (vtBase + 2) << " "
                                    << v3 << "/" << (vtBase + 3) << "\n";
                } else {
                    uint64_t v0 = vOffset + face.v[0] + 1;
                    uint64_t v1 = vOffset + face.v[1] + 1;
                    uint64_t v2 = vOffset + face.v[2] + 1;
                    objFile << "f " << v0 << "/" << (vtBase + 0) << " "
                                    << v1 << "/" << (vtBase + 1) << " "
                                    << v2 << "/" << (vtBase + 2) << "\n";
                }
            }

            vOffset += vertNum;
            vtOffset += currentMeshVt;
        }
    }

    objFile.close();

    // 4. Write MTL Material Library
    std::ofstream mtlFile(mtlPath);
    if (mtlFile.is_open()) {
        mtlFile << "# Material Library exported by Silent Hill Level Editor\n";
        for (const auto& [texName, palRow] : usedMaterials) {
            std::string matName = texName.empty() ? "mat_untextured" : ("mat_" + texName + "_pal" + std::to_string(palRow));
            mtlFile << "\nnewmtl " << matName << "\n";
            mtlFile << "Ka 1.000 1.000 1.000\n";
            mtlFile << "Kd 1.000 1.000 1.000\n";
            mtlFile << "Ks 0.000 0.000 0.000\n";
            mtlFile << "d 1.0\n";
            mtlFile << "illum 2\n";
            if (!texName.empty()) {
                std::string pngName = texName + "_pal" + std::to_string(palRow) + ".png";
                mtlFile << "map_Kd " << pngName << "\n";
            }
        }
        mtlFile.close();
    }

    // 5. Bake companion PNG textures for each used material
    for (const auto& [texName, palRow] : usedMaterials) {
        if (texName.empty()) continue;

        std::string pngFileName = texName + "_pal" + std::to_string(palRow) + ".png";
        fs::path pngPath = outDir / pngFileName;
        if (fs::exists(pngPath)) continue;

        fs::path timPath = fs::path(workspaceDir) / "TIM" / (texName + ".TIM");
        if (!fs::exists(timPath) && !assetsDir.empty()) {
            if (fs::exists(fs::path(assetsDir) / "BG" / (texName + ".TIM"))) {
                timPath = fs::path(assetsDir) / "BG" / (texName + ".TIM");
            } else if (fs::exists(fs::path(assetsDir) / (texName + ".TIM"))) {
                timPath = fs::path(assetsDir) / (texName + ".TIM");
            }
        }

        if (fs::exists(timPath)) {
            DecodedTIM tim;
            if (TIMDecoder::Decode(timPath.string(), tim)) {
                int w = tim.width;
                int h = tim.height;
                if (w > 0 && h > 0) {
                    std::vector<Color> pixels(w * h, Color{0, 0, 0, 255});
                    if (tim.bpp == 0 || tim.bpp == 1) {
                        if (!tim.palettes.empty() && !tim.rawIndices.empty()) {
                            size_t pRow = std::min((size_t)palRow, tim.palettes.size() - 1);
                            const auto& pal = tim.palettes[pRow];
                            for (int i = 0; i < w * h && i < (int)tim.rawIndices.size(); ++i) {
                                uint8_t idx = tim.rawIndices[i];
                                if (idx < pal.colors.size()) {
                                    const auto& c = pal.colors[idx];
                                    pixels[i] = Color{ c.r, c.g, c.b, c.a };
                                }
                            }
                        }
                    } else {
                        for (int i = 0; i < w * h && i < (int)tim.directPixels.size(); ++i) {
                            const auto& c = tim.directPixels[i];
                            pixels[i] = Color{ c.r, c.g, c.b, c.a };
                        }
                    }
                    Image raylibImg = { pixels.data(), w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
                    ExportImage(raylibImg, pngPath.string().c_str());
                }
            }
        }
    }

    return true;
}

bool OBJExport::Export(const FileManager& inspector, const DependencyManager& depMgr, const std::string& outPath, bool exportCollision) {
    return true;
}
