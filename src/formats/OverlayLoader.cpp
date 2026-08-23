#include "formats/OverlayLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>

namespace {

static std::string GetOverlayBasePath(const std::string& mapKey) {
    std::vector<std::string> candidates = {
        "../../data/workspace/overlays/" + mapKey + "/",
        "../data/workspace/overlays/" + mapKey + "/",
        "data/workspace/overlays/" + mapKey + "/",
        "../../../data/workspace/overlays/" + mapKey + "/"
    };
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c + "map_points.json") || std::filesystem::exists(c + "events.json")) {
            return c;
        }
    }
    return candidates[0];
}

// Helper: Trim whitespace
static std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

// Helper: Extract JSON object key-value pairs from a string block `{ ... }`
static std::vector<std::pair<std::string, std::string>> ParseJsonObject(const std::string& objStr) {
    std::vector<std::pair<std::string, std::string>> pairs;
    size_t i = 0;
    size_t len = objStr.length();

    while (i < len) {
        // Find key start quotes
        size_t keyStart = objStr.find('"', i);
        if (keyStart == std::string::npos) break;
        size_t keyEnd = objStr.find('"', keyStart + 1);
        if (keyEnd == std::string::npos) break;

        std::string key = objStr.substr(keyStart + 1, keyEnd - keyStart - 1);
        size_t colonPos = objStr.find(':', keyEnd + 1);
        if (colonPos == std::string::npos) break;

        // Extract value
        size_t valStart = colonPos + 1;
        while (valStart < len && (objStr[valStart] == ' ' || objStr[valStart] == '\t' || objStr[valStart] == '\n' || objStr[valStart] == '\r')) {
            valStart++;
        }
        if (valStart >= len) break;

        std::string valueStr;
        if (objStr[valStart] == '"') {
            // String value
            size_t valEnd = objStr.find('"', valStart + 1);
            if (valEnd != std::string::npos) {
                valueStr = objStr.substr(valStart + 1, valEnd - valStart - 1);
                i = valEnd + 1;
            } else {
                i = len;
            }
        } else {
            // Primitive value (number, bool, null)
            size_t valEnd = valStart;
            while (valEnd < len && objStr[valEnd] != ',' && objStr[valEnd] != '}' && objStr[valEnd] != '\n' && objStr[valEnd] != '\r') {
                valEnd++;
            }
            valueStr = Trim(objStr.substr(valStart, valEnd - valStart));
            i = valEnd + 1;
        }

        pairs.push_back({key, valueStr});
    }

    return pairs;
}

// Helper: Extract JSON array of objects
static std::vector<std::vector<std::pair<std::string, std::string>>> ParseJsonArrayOfObjects(const std::string& jsonContent) {
    std::vector<std::vector<std::pair<std::string, std::string>>> result;
    size_t i = 0;
    size_t len = jsonContent.length();

    while (i < len) {
        size_t objStart = jsonContent.find('{', i);
        if (objStart == std::string::npos) break;
        size_t objEnd = jsonContent.find('}', objStart + 1);
        if (objEnd == std::string::npos) break;

        std::string objStr = jsonContent.substr(objStart + 1, objEnd - objStart - 1);
        result.push_back(ParseJsonObject(objStr));
        i = objEnd + 1;
    }

    return result;
}

} // namespace

bool OverlayLoader::Load(const std::string& mapKey, OverlayMapData& outData) {
    outData.mapKey = mapKey;
    outData.waypoints.clear();
    outData.links.clear();
    outData.loaded = false;
    outData.dirty = false;

    std::string basePath = GetOverlayBasePath(mapKey);
    std::string wpPath = basePath + "map_points.json";
    std::string evPath = basePath + "events.json";

    // 1. Load map_points.json
    std::ifstream wpFile(wpPath);
    if (wpFile.is_open()) {
        std::stringstream ss;
        ss << wpFile.rdbuf();
        std::string content = ss.str();
        wpFile.close();

        auto objects = ParseJsonArrayOfObjects(content);
        for (const auto& obj : objects) {
            WaypointData wp;
            for (const auto& kv : obj) {
                const std::string& k = kv.first;
                const std::string& v = kv.second;

                if (k == "index") wp.index = std::stoi(v);
                else if (k == "positionX") wp.rawX = std::stol(v);
                else if (k == "positionZ") wp.rawZ = std::stol(v);
                else if (k == "worldX") wp.worldX = std::stof(v);
                else if (k == "worldZ") wp.worldZ = std::stof(v);
                else if (k == "triggerParam0") {
                    wp.triggerParam0 = std::stoi(v);
                    wp.arrivalAngleDeg = (int)round((wp.triggerParam0 / 255.0f) * 360.0f);
                }
                else if (k == "triggerParam1") wp.triggerParam1 = std::stoi(v);
                else if (k == "paperMapIdx") wp.paperMapIdx = v;
                else if (k == "paperMapIdxValue") wp.paperMapIdxValue = std::stoi(v);
                else if (k == "loadingScreenId") wp.loadingScreenId = v;
                else if (k == "loadingScreenIdValue") wp.loadingScreenIdValue = std::stoi(v);
                else if (k == "field_4_5") wp.field_4_5 = std::stoi(v);
                else if (k == "unused_4_12") wp.unused_4_12 = std::stoi(v);
            }
            outData.waypoints.push_back(wp);
        }
    }

    // 2. Load events.json
    std::ifstream evFile(evPath);
    if (evFile.is_open()) {
        std::stringstream ss;
        ss << evFile.rdbuf();
        std::string content = ss.str();
        evFile.close();

        auto objects = ParseJsonArrayOfObjects(content);
        for (const auto& obj : objects) {
            LinkData link;
            for (const auto& kv : obj) {
                const std::string& k = kv.first;
                const std::string& v = kv.second;

                if (k == "index") link.index = std::stoi(v);
                else if (k == "pointOfInterestIdx") link.waypointIdx = std::stoi(v);
                else if (k == "mapIdx") link.destMapKey = v;
                else if (k == "mapIdxValue") link.destMapIdx = std::stoi(v);
                else if (k == "triggerType") link.triggerType = v;
                else if (k == "triggerTypeValue") link.triggerTypeValue = std::stoi(v);
                else if (k == "activationType") link.activationType = v;
                else if (k == "activationTypeValue") link.activationTypeValue = std::stoi(v);
                else if (k == "sysState") link.sysState = v;
                else if (k == "sysStateValue") link.sysStateValue = std::stoi(v);
                else if (k == "eventParam") link.eventParam = std::stoi(v);
                else if (k == "requiredEventFlag") {
                    try { link.requiredEventFlag = std::stoi(v); } catch (...) { link.requiredEventFlag = 0; }
                }
                else if (k == "disabledEventFlag") link.disabledEventFlag = v;
                else if (k == "disabledEventFlagValue") {
                    try { link.disabledEventFlagValue = std::stoi(v); } catch (...) { link.disabledEventFlagValue = 0; }
                }
                else if (k == "requiredItemId") link.requiredItemId = v;
                else if (k == "requiredItemIdValue") {
                    try { link.requiredItemIdValue = std::stoi(v); } catch (...) { link.requiredItemIdValue = 0; }
                }
                else if (k == "flags_8_13") link.flags_8_13 = std::stoi(v);
                else if (k == "sfxPairIdx") link.sfxPairIdx = v;
                else if (k == "sfxPairIdxValue") {
                    try { link.sfxPairIdxValue = std::stoi(v); } catch (...) { link.sfxPairIdxValue = 0; }
                }
                else if (k == "field_8_24") link.field_8_24 = std::stoi(v);
            }
            outData.links.push_back(link);
        }
    }

    outData.mapKey = mapKey;
    outData.loaded = true;
    return true;
}

bool OverlayLoader::Save(const std::string& mapKey, const OverlayMapData& data) {
    std::string basePath = GetOverlayBasePath(mapKey);

    // 1. Write map_points.json
    {
        std::ofstream wpFile(basePath + "map_points.json");
        if (!wpFile.is_open()) return false;

        wpFile << "[\n";
        for (size_t i = 0; i < data.waypoints.size(); ++i) {
            const auto& wp = data.waypoints[i];
            int32_t rawX = (int32_t)round(wp.worldX * 4096.0f);
            int32_t rawZ = (int32_t)round(wp.worldZ * 4096.0f);
            int trigParam0 = (int)round((wp.arrivalAngleDeg / 360.0f) * 255.0f) & 0xFF;

            wpFile << "  {\n";
            wpFile << "    \"index\": " << wp.index << ",\n";
            wpFile << "    \"positionX\": " << rawX << ",\n";
            wpFile << "    \"positionZ\": " << rawZ << ",\n";
            wpFile << "    \"worldX\": " << wp.worldX << ",\n";
            wpFile << "    \"worldZ\": " << wp.worldZ << ",\n";
            wpFile << "    \"paperMapIdx\": \"" << wp.paperMapIdx << "\",\n";
            wpFile << "    \"paperMapIdxValue\": " << wp.paperMapIdxValue << ",\n";
            wpFile << "    \"loadingScreenId\": \"" << wp.loadingScreenId << "\",\n";
            wpFile << "    \"loadingScreenIdValue\": " << wp.loadingScreenIdValue << ",\n";
            wpFile << "    \"triggerParam0\": " << trigParam0 << ",\n";
            wpFile << "    \"triggerParam1\": " << wp.triggerParam1 << ",\n";
            wpFile << "    \"field_4_5\": " << wp.field_4_5 << ",\n";
            wpFile << "    \"unused_4_12\": " << wp.unused_4_12 << "\n";
            wpFile << "  }" << (i + 1 < data.waypoints.size() ? "," : "") << "\n";
        }
        wpFile << "]\n";
        wpFile.close();
    }

    // 2. Write events.json
    {
        std::ofstream evFile(basePath + "events.json");
        if (!evFile.is_open()) return false;

        evFile << "[\n";
        for (size_t i = 0; i < data.links.size(); ++i) {
            const auto& link = data.links[i];
            evFile << "  {\n";
            evFile << "    \"index\": " << link.index << ",\n";
            evFile << "    \"requiredEventFlag\": " << link.requiredEventFlag << ",\n";
            evFile << "    \"disabledEventFlag\": \"" << link.disabledEventFlag << "\",\n";
            evFile << "    \"disabledEventFlagValue\": " << link.disabledEventFlagValue << ",\n";
            evFile << "    \"triggerType\": \"" << link.triggerType << "\",\n";
            evFile << "    \"triggerTypeValue\": " << link.triggerTypeValue << ",\n";
            evFile << "    \"activationType\": \"" << link.activationType << "\",\n";
            evFile << "    \"activationTypeValue\": " << link.activationTypeValue << ",\n";
            evFile << "    \"pointOfInterestIdx\": " << link.waypointIdx << ",\n";
            evFile << "    \"requiredItemId\": \"" << link.requiredItemId << "\",\n";
            evFile << "    \"requiredItemIdValue\": " << link.requiredItemIdValue << ",\n";
            evFile << "    \"sysState\": \"" << link.sysState << "\",\n";
            evFile << "    \"sysStateValue\": " << link.sysStateValue << ",\n";
            evFile << "    \"eventParam\": " << link.eventParam << ",\n";
            evFile << "    \"flags_8_13\": " << link.flags_8_13 << ",\n";
            evFile << "    \"sfxPairIdx\": \"" << link.sfxPairIdx << "\",\n";
            evFile << "    \"sfxPairIdxValue\": " << link.sfxPairIdxValue << ",\n";
            evFile << "    \"field_8_24\": " << link.field_8_24 << ",\n";
            evFile << "    \"mapIdx\": \"" << link.destMapKey << "\",\n";
            evFile << "    \"mapIdxValue\": " << link.destMapIdx << "\n";
            evFile << "  }" << (i + 1 < data.links.size() ? "," : "") << "\n";
        }
        evFile << "]\n";
        evFile.close();
    }

    return true;
}

std::string OverlayLoader::GetMapKeyForChunk(const std::string& chunkName) {
    std::string upperName = chunkName;
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::toupper);

    if (upperName.rfind("MAP", 0) == 0) {
        // e.g. MAP0_S00
        size_t dot = upperName.find('.');
        if (dot != std::string::npos) return upperName.substr(0, dot);
        return upperName;
    }

    if (upperName.rfind("THR", 0) == 0) return "MAP0_S00";
    if (upperName.rfind("ER", 0) == 0) return "MAP0_S02";
    if (upperName.rfind("KG", 0) == 0) return "MAP1_S00";

    return "MAP0_S00"; // Fallback
}
