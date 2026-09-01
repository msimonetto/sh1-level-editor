#include "core/ChunkUtils.h"
#include <cstdio>
#include <cctype>

namespace Core {

static const std::regex CHUNK_REGEX(R"(^([A-Za-z0-9_]+?)([0-9A-Fa-f]{2})([0-9A-Fa-f]{2})$)");

bool IsValidChunkName(const std::string& name) {
    return std::regex_match(name, CHUNK_REGEX);
}

bool ParseChunkName(const std::string& name, std::string& outPrefix, int8_t& outX, int8_t& outY) {
    std::smatch match;
    if (std::regex_match(name, match, CHUNK_REGEX) && match.size() == 4) {
        outPrefix = match[1].str();
        
        try {
            int xHex = std::stoi(match[2].str(), nullptr, 16);
            int yHex = std::stoi(match[3].str(), nullptr, 16);

            outX = static_cast<int8_t>(xHex);
            outY = static_cast<int8_t>(yHex);
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

std::string ExtractChunkPrefix(const std::string& name) {
    std::smatch match;
    if (std::regex_match(name, match, CHUNK_REGEX) && match.size() == 4) {
        return match[1].str();
    }
    return name;
}

std::string FormatChunkName(const std::string& prefix, int8_t x, int8_t y) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%s%02X%02X",
             prefix.c_str(),
             static_cast<uint8_t>(x),
             static_cast<uint8_t>(y));
    return std::string(buf);
}

} // namespace Core
