#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "formats/Structs.h"

class PLMCreate {
public:
    // Generates a standalone _GLB.PLM file with 0 textures and 0 objects.
    static bool CreateBlankGlobal(const std::string& outputPath);

    // Generates a minimal PLM in-memory, typically to be appended inside a new IPD file.
    static void GenerateBlankEmbedded(std::vector<uint8_t>& outBuf);
};
