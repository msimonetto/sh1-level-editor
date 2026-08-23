#include "core/FileManager.h"
#include "core/Config.h"
#include "core/DependencyManager.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>

namespace fs = std::filesystem;

// ---------------------------------------------------------
// Mini IPD/PLM parsers for dependency extraction 
// ---------------------------------------------------------


static bool FilesAreDifferent(const fs::path& p1, const fs::path& p2) {
    if (!fs::exists(p1) || !fs::exists(p2)) return true;
    if (fs::file_size(p1) != fs::file_size(p2)) return true;

    std::ifstream f1(p1, std::ios::binary);
    std::ifstream f2(p2, std::ios::binary);
    
    std::vector<char> buf1(8192);
    std::vector<char> buf2(8192);

    do {
        f1.read(buf1.data(), buf1.size());
        f2.read(buf2.data(), buf2.size());
        if (f1.gcount() != f2.gcount() || std::memcmp(buf1.data(), buf2.data(), f1.gcount()) != 0) {
            return true;
        }
    } while (f1.good() && f2.good());

    return false;
}

FileManager::FileManager() {
    m_selectedPrefix = Config::Get().SelectedPrefix;
    std::string assetsDir = GetAssetsDir();
    if (!assetsDir.empty() && fs::exists(assetsDir)) {
        ScanAssets();
        
        if (m_parsedChunks.find(m_selectedPrefix) == m_parsedChunks.end() && !m_parsedChunks.empty()) {
            m_selectedPrefix = m_parsedChunks.begin()->first;
        }
    }
}

std::string FileManager::GetWorkspaceDir() const {
    return (fs::path(Config::Get().ProjectDirectory) / "workspace").string();
}
std::string FileManager::GetAssetsDir() const {
    return (fs::path(Config::Get().ProjectDirectory) / "assets").string();
}
std::string FileManager::GetOverrideDir() const {
    return (fs::path(Config::Get().GameDirectory) / "pc_port" / "build" / "gamedata" / "load").string();
}
std::string FileManager::GetBuildDir() const {
    return (fs::path(Config::Get().GameDirectory) / "pc_port" / "build").string();
}
std::string FileManager::GetGameBinSource() const {
    return (fs::path(Config::Get().GameDirectory) / "pc_port" / "build" / "gamedata" / "SLUS-00707.bin").string();
}

std::vector<std::string> FileManager::ConsumeReloadChunks() {
    std::lock_guard<std::mutex> lock(m_reloadMutex);
    std::vector<std::string> chunks = m_reloadChunks;
    m_reloadChunks.clear();
    return chunks;
}

void FileManager::QueueReloadChunks(const std::vector<std::string>& chunks) {
    std::lock_guard<std::mutex> lock(m_reloadMutex);
    m_reloadChunks.insert(m_reloadChunks.end(), chunks.begin(), chunks.end());
}

void FileManager::Log(const std::string &msg, bool isError) {
    std::lock_guard<std::mutex> lock(m_consoleMutex);
    std::string prefix = isError ? "[ERROR] " : "";
    m_consoleLines.push_back(prefix + msg);
    if (m_logCallback) {
        m_logCallback(msg, isError);
    }
}

void FileManager::ScanAssets() {
    m_parsedChunks.clear();
    m_selectedChunks.clear();

    std::string assetsDir = GetAssetsDir();
    if (assetsDir.empty() || !fs::exists(assetsDir)) {
        Log("Invalid Assets Directory.", true);
        return;
    }

    fs::path searchDir = assetsDir;
    if (fs::exists(searchDir / "BG")) {
        searchDir = searchDir / "BG";
    }

    for (const auto &entry : fs::directory_iterator(searchDir)) {
        if (entry.path().extension() == ".IPD" || entry.path().extension() == ".ipd") {
            std::string stem = entry.path().stem().string();
            std::string prefix = stem;
            int x = 0, z = 0;
            bool hasCoords = false;

            if (stem.length() > 4) {
                std::string rest = stem.substr(stem.length() - 4);
                try {
                    int x_hex = std::stoi(rest.substr(0, 2), nullptr, 16);
                    int z_hex = std::stoi(rest.substr(2, 2), nullptr, 16);

                    if (x_hex >= 128) x_hex -= 256;
                    if (z_hex >= 128) z_hex -= 256;

                    x = x_hex;
                    z = z_hex;
                    hasCoords = true;
                    prefix = stem.substr(0, stem.length() - 4);
                } catch (...) { }
            }

            ChunkInfo info;
            info.name = stem;
            info.prefix = prefix;
            info.x = x;
            info.z = z;
            info.hasCoords = hasCoords;
            m_parsedChunks[prefix].push_back(info);
        }
    }
}

bool FileManager::ExtractToWorkspace(const std::vector<std::string>& chunks, const std::string& completeDir, const std::string& workspaceDir, const std::string& projectDir) {
    Log("[EXTRACT] Starting native C++ extraction to workspace...");
    
    fs::path compDir = fs::path(completeDir);
    fs::path workDir = fs::path(workspaceDir);
    
    fs::path sourceDir = fs::exists(compDir / "BG") ? compDir / "BG" : compDir;
    
    fs::create_directories(workDir / "IPD");
    fs::create_directories(workDir / "PLM");
    fs::create_directories(workDir / "TIM");
    fs::create_directories(workDir / "OBJ");
    fs::create_directories(workDir / "misc");
    fs::create_directories(workDir / "audio");
    fs::create_directories(workDir / "overlays");
    
    DependencyManager depMgr(workspaceDir);
    
    for (size_t i = 0; i < chunks.size(); i++) {
        std::string chunk = chunks[i];
        fs::path sourceIpd = sourceDir / (chunk + ".IPD");
        if (!fs::exists(sourceIpd)) {
            Log("[EXTRACT] " + chunk + ".IPD not found.", true);
            continue;
        }
        
        fs::path targetIpd = workDir / "IPD" / (chunk + ".IPD");
        fs::copy_file(sourceIpd, targetIpd, fs::copy_options::overwrite_existing);
        
        std::string prefix = chunk;
        if (chunk.length() > 4) prefix = chunk.substr(0, chunk.length() - 4);
        
        std::string prefix2 = chunk.substr(0, 2);
        std::string prefix3 = chunk.substr(0, 3);
        
        for (const auto& entry : fs::directory_iterator(sourceDir)) {
            if (!entry.is_regular_file()) continue;
            
            std::string fname = entry.path().filename().string();
            if (fname.rfind(prefix3, 0) == 0 || fname.rfind(prefix2, 0) == 0) {
                std::string ext = entry.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
                
                if (ext == ".BIN") {
                    fs::copy_file(entry.path(), workDir / "misc" / fname, fs::copy_options::overwrite_existing);
                } else if (ext == ".TIM") {
                    fs::copy_file(entry.path(), workDir / "TIM" / fname, fs::copy_options::overwrite_existing);
                    depMgr.AddDependency(prefix, chunk, "textures", fname);
                } else if (ext == ".PLM") {
                    fs::copy_file(entry.path(), workDir / "PLM" / fname, fs::copy_options::overwrite_existing);
                    depMgr.AddDependency(prefix, chunk, "geometry", fname);
                }
            }
        }
        
        if (m_progressCallback) m_progressCallback(i + 1, chunks.size(), "Extracting " + chunk);
    }
    
    Log("[EXTRACT] Complete.");
    return true;
}

std::filesystem::path FileManager::FindAssetPath(const std::filesystem::path& assetsDir, const std::string& filename) const {
    fs::path bgPath = assetsDir / "BG" / filename;
    if (fs::exists(bgPath)) return bgPath;
    
    for (const auto& entry : fs::recursive_directory_iterator(assetsDir)) {
        if (entry.path().filename().string() == filename) {
            return entry.path();
        }
    }
    return bgPath;
}

bool FileManager::DeployToTarget(const std::vector<std::string>& chunks, const std::string& workspaceDir, const std::string& overrideDir, const std::string& projectDir) {
    Log("[DEPLOY] Starting native smart deploy...");
    
    fs::path workDir = fs::path(workspaceDir);
    fs::path ovrBgDir = fs::path(overrideDir) / "BG";
    fs::path astDir = fs::path(projectDir) / "data" / "assets";
    fs::create_directories(ovrBgDir);
    
    DependencyManager depMgr(workspaceDir);
    std::set<std::string> filesToCheck;
    
    for (const auto& chunk : chunks) {
        filesToCheck.insert(chunk + ".IPD");
        
        if (depMgr.m_dependenciesData.find(chunk) != depMgr.m_dependenciesData.end()) {
            auto& data = depMgr.m_dependenciesData[chunk];
            if (data.contains("textures")) {
                for (const auto& tex : data["textures"]) filesToCheck.insert(tex.get<std::string>());
            }
            if (data.contains("geometry")) {
                for (const auto& geom : data["geometry"]) filesToCheck.insert(geom.get<std::string>());
            }
        }
        
        std::string p2 = chunk.substr(0, 2);
        std::string p3 = chunk.substr(0, 3);
        if (fs::exists(workDir / "misc")) {
            for (const auto& entry : fs::directory_iterator(workDir / "misc")) {
                if (entry.path().extension() == ".BIN") {
                    std::string fname = entry.path().filename().string();
                    if (fname.rfind(p3, 0) == 0 || fname.rfind(p2, 0) == 0) {
                        filesToCheck.insert(fname);
                    }
                }
            }
        }
    }
    
    int copied = 0, deleted = 0;
    
    for (const auto& fname : filesToCheck) {
        fs::path wsPath;
        if (fname.find(".IPD") != std::string::npos) wsPath = workDir / "IPD" / fname;
        else if (fname.find(".PLM") != std::string::npos) wsPath = workDir / "PLM" / fname;
        else if (fname.find(".TIM") != std::string::npos) wsPath = workDir / "TIM" / fname;
        else if (fname.find(".BIN") != std::string::npos) wsPath = workDir / "misc" / fname;
        else wsPath = workDir / fname;
        
        if (!fs::exists(wsPath)) continue;
        
        fs::path assetPath = FindAssetPath(astDir, fname);
        fs::path tgtPath = ovrBgDir / fname;
        
        if (FilesAreDifferent(wsPath, assetPath)) {
            if (FilesAreDifferent(wsPath, tgtPath)) {
                fs::copy_file(wsPath, tgtPath, fs::copy_options::overwrite_existing);
                copied++;
            }
        } else {
            if (fs::exists(tgtPath)) {
                fs::remove(tgtPath);
                deleted++;
            }
        }
    }
    
    Log("[DEPLOY] Complete (Copied: " + std::to_string(copied) + ", Cleaned: " + std::to_string(deleted) + ")");
    return true;
}

bool FileManager::DeleteSelected(const std::string& targetType, const std::vector<std::string>& chunks, bool deleteTextures, const std::string& workspaceDir, const std::string& overrideDir, const std::string& projectDir) {
    Log("[DELETE] Starting deletion from " + targetType + "...");
    
    fs::path workDir = fs::path(workspaceDir);
    DependencyManager depMgr(workspaceDir);
    std::set<std::string> shared = depMgr.GetSharedFiles(chunks);
    
    fs::path chunksDir, geomDir, texDir, miscDir;
    if (targetType == "workspace") {
        chunksDir = workDir / "IPD"; geomDir = workDir / "PLM"; texDir = workDir / "TIM"; miscDir = workDir / "misc";
    } else {
        fs::path bgDir = fs::path(overrideDir) / "BG";
        chunksDir = bgDir; geomDir = bgDir; texDir = bgDir; miscDir = bgDir;
    }
    
    for (const auto& chunk : chunks) {
        fs::path ipdPath = chunksDir / (chunk + ".IPD");
        if (fs::exists(ipdPath)) fs::remove(ipdPath);
        
        std::string p2 = chunk.substr(0, 2);
        std::string p3 = chunk.substr(0, 3);
        if (fs::exists(miscDir)) {
            for (const auto& entry : fs::directory_iterator(miscDir)) {
                if (entry.path().extension() == ".BIN") {
                    std::string fname = entry.path().filename().string();
                    if (fname.rfind(p3, 0) == 0 || fname.rfind(p2, 0) == 0) {
                        fs::remove(entry.path());
                    }
                }
            }
        }
        
        if (depMgr.m_dependenciesData.find(chunk) != depMgr.m_dependenciesData.end()) {
            auto& data = depMgr.m_dependenciesData[chunk];
            if (data.contains("geometry")) {
                for (const auto& geom : data["geometry"]) {
                    std::string gName = geom.get<std::string>();
                    if (shared.find(gName) == shared.end()) {
                        fs::path p = geomDir / gName;
                        if (fs::exists(p)) fs::remove(p);
                    }
                }
            }
            if (deleteTextures && data.contains("textures")) {
                for (const auto& tex : data["textures"]) {
                    std::string tName = tex.get<std::string>();
                    if (shared.find(tName) == shared.end()) {
                        fs::path p = texDir / tName;
                        if (fs::exists(p)) fs::remove(p);
                    }
                }
            }
            if (targetType == "workspace") {
                depMgr.m_dependenciesData.erase(chunk);
            }
        }
    }
    
    if (targetType == "workspace") depMgr.Save();
    Log("[DELETE] Complete.");
    return true;
}

bool FileManager::ClearEntire(const std::string& targetType, const std::string& workspaceDir, const std::string& overrideDir, const std::string& projectDir) {
    Log("[CLEAR] Clearing entire " + targetType + "...");
    
    auto ClearDir = [](const fs::path& p) {
        if (!fs::exists(p)) return;
        for (const auto& entry : fs::directory_iterator(p)) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
            if (ext == ".IPD" || ext == ".PLM" || ext == ".TIM" || ext == ".BIN") {
                fs::remove(entry.path());
            }
        }
    };
    
    if (targetType == "workspace") {
        fs::path w = fs::path(workspaceDir);
        ClearDir(w / "IPD");
        ClearDir(w / "PLM");
        ClearDir(w / "TIM");
        ClearDir(w / "OBJ");
        ClearDir(w / "misc");
        
        DependencyManager depMgr(workspaceDir);
        depMgr.m_dependenciesData.clear();
        depMgr.Save();
    } else {
        ClearDir(fs::path(overrideDir) / "BG");
    }
    
    Log("[CLEAR] Complete.");
    return true;
}

bool FileManager::RevertSelected(const std::vector<std::string>& chunks, bool revertDependencies, const std::string& workspaceDir, const std::string& assetsDir) {
    Log("[REVERT] Starting native revert...");
    return ExtractToWorkspace(chunks, assetsDir, workspaceDir, "");
}

bool FileManager::DeployOverlayToDecomp(const std::string& mapKey) {
    Log("[DEPLOY OVERLAY] Packing " + mapKey + " to decomp C source...");
    
    fs::path cwd = fs::current_path();
    fs::path scriptPath;
    fs::path p1 = cwd / "scripts" / "backend" / "pack_overlay_to_decomp.py";
    fs::path p2 = cwd / ".." / "scripts" / "backend" / "pack_overlay_to_decomp.py";
    if (fs::exists(p1)) scriptPath = p1;
    else if (fs::exists(p2)) scriptPath = p2;
    
    if (scriptPath.empty()) {
        Log("[DEPLOY OVERLAY] Python script pack_overlay_to_decomp.py not found.", true);
        return false;
    }
    
    std::string cmd = "python \"" + scriptPath.string() + "\" --map " + mapKey;
    int res = std::system(cmd.c_str());
    if (res != 0) {
        Log("[DEPLOY OVERLAY] Failed.", true);
        return false;
    }
    Log("[DEPLOY OVERLAY] Complete.");
    return true;
}
