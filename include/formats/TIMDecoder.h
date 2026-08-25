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

    bool operator==(const TIMColor &o) const {
        return r == o.r && g == o.g && b == o.b && a == o.a;
    }
    bool operator!=(const TIMColor &o) const {
        return !(*this == o);
    }

    inline void ToR5G5B5(int &outR5, int &outG5, int &outB5, bool &outStp) const {
        outR5 = (int)r * 31 / 255;
        outG5 = (int)g * 31 / 255;
        outB5 = (int)b * 31 / 255;
        outStp = (a == 0) ? false : (a < 255 || (r == 0 && g == 0 && b == 0));
    }

    static inline TIMColor FromR5G5B5(int r5, int g5, int b5, bool stp) {
        uint8_t r8 = (uint8_t)(r5 * 255 / 31);
        uint8_t g8 = (uint8_t)(g5 * 255 / 31);
        uint8_t b8 = (uint8_t)(b5 * 255 / 31);
        uint8_t a8 = 255;
        if (r5 == 0 && g5 == 0 && b5 == 0 && !stp) {
            a8 = 0; // Pure 0x0000 is transparent
        } else if (stp && (r5 != 0 || g5 != 0 || b5 != 0)) {
            a8 = 180; // Semi-transparent
        }
        return {r8, g8, b8, a8};
    }
};

struct TIMPalette {
    std::vector<TIMColor> colors;

    bool operator==(const TIMPalette &o) const {
        return colors == o.colors;
    }
    bool operator!=(const TIMPalette &o) const {
        return !(*this == o);
    }
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

    bool operator==(const DecodedTIM &o) const {
        return width == o.width &&
               height == o.height &&
               bpp == o.bpp &&
               clutX == o.clutX &&
               clutY == o.clutY &&
               imgX == o.imgX &&
               imgY == o.imgY &&
               palettes == o.palettes &&
               rawIndices == o.rawIndices &&
               directPixels == o.directPixels;
    }
    bool operator!=(const DecodedTIM &o) const {
        return !(*this == o);
    }
};

class TIMDecoder {
public:
    static bool Decode(const std::string& filepath, DecodedTIM& outTim);
    static TIMColor WordToColor(uint16_t word);
};
