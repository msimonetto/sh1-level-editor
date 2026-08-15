#include "core/Patcher.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <cmath>
#include <map>

namespace fs = std::filesystem;

static std::string GetVersionStr(EnginePatcher::Version version) {
    switch(version) {
        case EnginePatcher::Version::USA: return "USA";
        case EnginePatcher::Version::EU: return "EU";
        case EnginePatcher::Version::JP: return "JP";
        default: return "ALL";
    }
}

bool EnginePatcher::PatchMemoryAllocations(const std::string& overrideDir, const std::string& engineSrcDir, Version version) {
    if (overrideDir.empty() || engineSrcDir.empty()) return false;
    
    fs::path targetDir = fs::path(overrideDir);
    if (fs::exists(targetDir / "BG")) targetDir = targetDir / "BG";
    
    if (!fs::exists(targetDir) || !fs::exists(engineSrcDir)) return false;
    
    std::string verStr = GetVersionStr(version);
    std::vector<fs::path> incFiles;
    for (const auto& entry : fs::directory_iterator(engineSrcDir)) {
        std::string filename = entry.path().filename().string();
        if (filename.find("filetable.c.") == 0 && filename.find(".inc") != std::string::npos && filename.find(".bak") == std::string::npos) {
            if (verStr == "ALL" || filename.find("." + verStr + ".inc") != std::string::npos) {
                incFiles.push_back(entry.path());
            }
        }
    }
    
    if (incFiles.empty()) return false;
    
    std::map<std::string, int> requiredBlocks;
    for (const auto& entry : fs::directory_iterator(targetDir)) {
        if (entry.path().extension() == ".IPD" || entry.path().extension() == ".ipd") {
            int blocks = (int)std::ceil(fs::file_size(entry.path()) / 256.0);
            std::string name = entry.path().stem().string();
            for (auto& c : name) c = toupper(c);
            requiredBlocks[name] = blocks;
        }
    }
    
    if (requiredBlocks.empty()) return true;
    
    bool overallSuccess = true;
    
    for (const auto& incFile : incFiles) {
        fs::path bakFile = incFile.string() + ".bak";
        if (!fs::exists(bakFile)) {
            fs::copy_file(incFile, bakFile);
        }
        
        std::ifstream fileIn(incFile);
        if (!fileIn) continue;
        std::stringstream buffer;
        buffer << fileIn.rdbuf();
        std::string content = buffer.str();
        fileIn.close();
        
        bool patched = false;
        for (const auto& [chunk, blocks] : requiredBlocks) {
            if (chunk.length() > 8) continue;
            std::string padded = chunk;
            while (padded.length() < 8) padded += " ";
            
            std::string fn_args = "";
            for (size_t i = 0; i < 8; i++) {
                fn_args += "'";
                fn_args += padded[i];
                fn_args += "'";
                if (i < 7) fn_args += ",";
            }
            
            std::string patternStr = R"((\{\s*0x[0-9a-fA-F]+\s*,\s*)(\d+)(\s*,\s*\d+\s*,\s*\n*\s*FN\()" + fn_args + R"(\)))";
            std::regex pattern(patternStr);
            
            std::smatch match;
            std::string::const_iterator searchStart(content.cbegin());
            std::string newContent;
            bool localPatched = false;
            
            while (std::regex_search(searchStart, content.cend(), match, pattern)) {
                newContent += match.prefix();
                int currentBlocks = std::stoi(match[2]);
                if (blocks > currentBlocks) {
                    newContent += match[1].str() + std::to_string(blocks) + match[3].str();
                    patched = true;
                    localPatched = true;
                } else {
                    newContent += match.str();
                }
                searchStart = match.suffix().first;
            }
            newContent += std::string(searchStart, content.cend());
            if (localPatched) {
                content = newContent;
            }
        }
        
        if (patched) {
            std::ofstream fileOut(incFile, std::ios::binary);
            fileOut << content;
            fileOut.close();
        }
    }
    
    return overallSuccess;
}

bool EnginePatcher::RevertMemoryAllocations(const std::string& overrideDir, const std::string& engineSrcDir, Version version) {
    if (engineSrcDir.empty()) return false;
    fs::path dir = fs::path(engineSrcDir);
    if (!fs::exists(dir)) return false;
    
    std::string verStr = GetVersionStr(version);
    bool reverted = false;
    
    for (const auto& entry : fs::directory_iterator(dir)) {
        std::string filename = entry.path().filename().string();
        if (filename.find("filetable.c.") == 0 && filename.find(".inc") != std::string::npos && filename.find(".bak") == std::string::npos) {
            if (verStr == "ALL" || filename.find("." + verStr + ".inc") != std::string::npos) {
                fs::path bakFile = entry.path().string() + ".bak";
                if (fs::exists(bakFile)) {
                    fs::copy_file(bakFile, entry.path(), fs::copy_options::overwrite_existing);
                    reverted = true;
                }
            }
        }
    }
    return reverted;
}

bool EnginePatcher::CheckPatchingRequired(const std::string& overrideDir, const std::string& engineSrcDir, Version version, std::vector<std::string>* outNeedsPatchingChunks) {
    if (overrideDir.empty() || engineSrcDir.empty()) return false;
    
    fs::path targetDir = fs::path(overrideDir);
    if (fs::exists(targetDir / "BG")) targetDir = targetDir / "BG";
    
    if (!fs::exists(targetDir) || !fs::exists(engineSrcDir)) return false;
    
    std::string verStr = GetVersionStr(version);
    std::vector<fs::path> incFiles;
    for (const auto& entry : fs::directory_iterator(engineSrcDir)) {
        std::string filename = entry.path().filename().string();
        if (filename.find("filetable.c.") == 0 && filename.find(".inc") != std::string::npos && filename.find(".bak") == std::string::npos) {
            if (verStr == "ALL" || filename.find("." + verStr + ".inc") != std::string::npos) {
                incFiles.push_back(entry.path());
            }
        }
    }
    
    if (incFiles.empty()) return false;
    
    std::map<std::string, int> requiredBlocks;
    for (const auto& entry : fs::directory_iterator(targetDir)) {
        if (entry.path().extension() == ".IPD" || entry.path().extension() == ".ipd") {
            int blocks = (int)std::ceil(fs::file_size(entry.path()) / 256.0);
            std::string name = entry.path().stem().string();
            for (auto& c : name) c = toupper(c);
            requiredBlocks[name] = blocks;
        }
    }
    
    if (requiredBlocks.empty()) return false;
    
    bool needsPatching = false;
    if (outNeedsPatchingChunks) outNeedsPatchingChunks->clear();
    
    for (const auto& incFile : incFiles) {
        std::ifstream fileIn(incFile);
        if (!fileIn) continue;
        std::stringstream buffer;
        buffer << fileIn.rdbuf();
        std::string content = buffer.str();
        fileIn.close();
        
        for (const auto& [chunk, blocks] : requiredBlocks) {
            if (chunk.length() > 8) continue;
            std::string padded = chunk;
            while (padded.length() < 8) padded += " ";
            
            std::string fn_args = "";
            for (size_t i = 0; i < 8; i++) {
                fn_args += "'";
                fn_args += padded[i];
                fn_args += "'";
                if (i < 7) fn_args += ",";
            }
            
            std::string patternStr = R"((\{\s*0x[0-9a-fA-F]+\s*,\s*)(\d+)(\s*,\s*\d+\s*,\s*\n*\s*FN\()" + fn_args + R"(\)))";
            std::regex pattern(patternStr);
            std::smatch match;
            if (std::regex_search(content, match, pattern)) {
                int currentBlocks = std::stoi(match[2]);
                if (blocks > currentBlocks) {
                    needsPatching = true;
                    if (outNeedsPatchingChunks) {
                        if (std::find(outNeedsPatchingChunks->begin(), outNeedsPatchingChunks->end(), chunk) == outNeedsPatchingChunks->end()) {
                            outNeedsPatchingChunks->push_back(chunk);
                        }
                    }
                }
            }
        }
    }
    
    return needsPatching;
}

