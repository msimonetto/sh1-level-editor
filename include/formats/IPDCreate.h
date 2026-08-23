#pragma once
#include <string>
#include <cstdint>

class IPDCreate {
public:
    // Generates a blank .IPD file with an empty collision header and an empty embedded PLM
    static bool CreateBlank(const std::string& outputPath, int8_t xPos, int8_t yPos);
};
