#pragma once
#include <string>
#include <cstdint>
#include <regex>

namespace Core {

// ---------------------------------------------------------------------------
// Standard Silent Hill 1 Chunk Naming Regex:
// Prefix (1+ alphanumeric characters) followed by exactly 4 hex digits (XXZZ).
// Examples:
//   "THR0000" -> Prefix: "THR", X: 0, Z: 0
//   "THRFBFE" -> Prefix: "THR", X: -5 (0xFB), Z: -2 (0xFE)
//   "SC0102"  -> Prefix: "SC",  X: 1, Z: 2
// ---------------------------------------------------------------------------

// Check if a string follows valid Silent Hill 1 chunk naming conventions
bool IsValidChunkName(const std::string& name);

// Parse chunk name into its prefix and signed 8-bit grid coordinates (xPos, yPos/zPos)
bool ParseChunkName(const std::string& name, std::string& outPrefix, int8_t& outX, int8_t& outY);

// Extract the prefix from a chunk name (e.g. "THRFBFE" -> "THR", fallback to original if non-conforming)
std::string ExtractChunkPrefix(const std::string& name);

// Format a standard 8-character chunk name from a prefix and signed grid coordinates (e.g. "THR", 1, 2 -> "THR0102")
std::string FormatChunkName(const std::string& prefix, int8_t x, int8_t y);

} // namespace Core
