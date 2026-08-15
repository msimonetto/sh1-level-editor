#include "core/Dictionary.h"
#include <stdio.h>
#include <vector>

namespace fs = std::filesystem;

static std::string FindExistingOrFallback(const std::vector<fs::path>& candidates) {
    for (const auto& p : candidates) {
        if (fs::exists(p)) {
            return p.string();
        }
    }
    return candidates.empty() ? "" : candidates[0].string();
}

Dictionary::Dictionary() {
    PrefixNames = {
        {"THR", "Old Silent Hill (Normal)"},
        {"SC", "Midwich Elementary School (Normal)"},
        {"SU", "Midwich Elementary School (Alternate)"},
        {"HP", "Alchemilla Hospital (Normal)"},
        {"HU", "Alchemilla Hospital (Alternate)"},
        {"RSR", "Resort Area (Normal)"},
        {"RSU", "Resort Area (Alternate)"},
        {"DR", "Sewers (Normal)"},
        {"DRU", "Sewers (Alternate)"},
        {"APU", "Amusement Park (Alternate)"},
        {"SPR", "Central Silent Hill (Normal)"},
        {"SPU", "Central Silent Hill (Alternate)"},
        {"ER", "Extra Rooms & Nowhere"}
    };

    fs::path cwd = fs::current_path();

    // 1. Template dictionary path (tracked in git)
    m_templatePath = FindExistingOrFallback({
        cwd / "res" / "dictionary.json",
        cwd / ".." / "res" / "dictionary.json"
    });

    // 2. User-specific workspace dictionary path (local only)
    m_dictPath = FindExistingOrFallback({
        cwd / "data" / "workspace" / "dictionary.json",
        cwd / ".." / "data" / "workspace" / "dictionary.json"
    });

    Load();
}

void Dictionary::ParseJsonFile(const std::string& path) {
    if (path.empty() || !fs::exists(path))
        return;

    try {
        FILE* f = fopen(path.c_str(), "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (len <= 0) {
                fclose(f);
                return;
            }

            std::vector<char> buf(len + 1, 0);
            fread(buf.data(), 1, len, f);
            fclose(f);

            std::string content = buf.data();

            size_t pos = 0;
            while ((pos = content.find("\"", pos)) != std::string::npos) {
                size_t endPos = content.find("\"", pos + 1);
                if (endPos == std::string::npos)
                    break;
                std::string key = content.substr(pos + 1, endPos - pos - 1);

                size_t colonPos = content.find(":", endPos + 1);
                if (colonPos != std::string::npos) {
                    size_t valPos = content.find("\"", colonPos + 1);
                    if (valPos != std::string::npos) {
                        size_t valEndPos = content.find("\"", valPos + 1);
                        if (valEndPos != std::string::npos) {
                            std::string val = content.substr(valPos + 1, valEndPos - valPos - 1);
                            ChunkAliases[key] = val;
                            pos = valEndPos + 1;
                            continue;
                        }
                    }
                }
                pos = endPos + 1;
            }
        }
    } catch (...) {
    }
}

void Dictionary::Load() {
    ChunkAliases.clear();

    // 1. Load centralized template dictionary first (default baseline)
    ParseJsonFile(m_templatePath);

    // 2. Overlay user-specific workspace dictionary if present
    ParseJsonFile(m_dictPath);
}

void Dictionary::Save() {
    try {
        fs::path p(m_dictPath);
        if (p.has_parent_path()) {
            fs::create_directories(p.parent_path());
        }

        FILE* f = fopen(m_dictPath.c_str(), "wb");
        if (f) {
            std::string out = "{\n";
            bool first = true;
            for (const auto& pair : ChunkAliases) {
                if (!first) out += ",\n";
                out += "    \"" + pair.first + "\": \"" + pair.second + "\"";
                first = false;
            }
            out += "\n}\n";
            fwrite(out.c_str(), 1, out.length(), f);
            fclose(f);
        }
    } catch (...) {
    }
}
