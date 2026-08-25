#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "formats/Structs.h"

// ---------------------------------------------------------------------------
// TIM Processing Data Structures (Pure C++)
// ---------------------------------------------------------------------------
struct TIMColor {
    uint8_t r, g, b, a;
};

struct TIMPalette {
    std::vector<TIMColor> colors;
};

struct DecodedTIM {
    int width = 0;
    int height = 0;
    int bpp = 0;
    uint16_t clutX = 0;
    uint16_t clutY = 0;
    uint16_t imgX = 0;
    uint16_t imgY = 0;
    
    std::vector<TIMPalette> palettes;
    std::vector<uint8_t> rawIndices;    // For 4-bit / 8-bit indexed
    std::vector<TIMColor> directPixels; // For 16-bit direct color
};

class TIMDecoder {
public:
    static bool Decode(const std::string& filepath, DecodedTIM& outTim);
    static TIMColor WordToColor(uint16_t word);
};
