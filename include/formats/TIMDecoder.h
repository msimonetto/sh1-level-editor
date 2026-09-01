#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "formats/Structs.h"

// ---------------------------------------------------------------------------
// TIM Processing Data Structures (Pure C++)
// ---------------------------------------------------------------------------
struct TIMColor {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;

    bool operator==(const TIMColor &o) const;
    bool operator!=(const TIMColor &o) const;

    void ToR5G5B5(int &outR5, int &outG5, int &outB5, bool &outStp) const;
    static TIMColor FromR5G5B5(int r5, int g5, int b5, bool stp);
};

struct TIMPalette {
    std::vector<TIMColor> colors;

    bool operator==(const TIMPalette &o) const;
    bool operator!=(const TIMPalette &o) const;
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

    bool operator==(const DecodedTIM &o) const;
    bool operator!=(const DecodedTIM &o) const;
};

class TIMDecoder {
public:
    static bool Decode(const std::string& filepath, DecodedTIM& outTim);
    static TIMColor WordToColor(uint16_t word);
};
