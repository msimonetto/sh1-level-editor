#include "formats/TIMEncoder.h"
#include <cstdio>

uint16_t TIMEncoder::ColorToWord(const TIMColor& c) {
    if (c.a == 0 && c.r == 0 && c.g == 0 && c.b == 0) return 0x0000;
    
    // Scale 0-255 back to 0-31 with rounding
    uint16_t r = (c.r * 31 + 127) / 255;
    uint16_t g = (c.g * 31 + 127) / 255;
    uint16_t b = (c.b * 31 + 127) / 255;
    
    // STP bit: solid black (0,0,0) uses STP=1 (0x8000) so it is not 0x0000 (transparent).
    // Semi-transparent colors (a > 0 && a < 255) set STP=1.
    uint16_t stp = 0;
    if (r == 0 && g == 0 && b == 0 && c.a > 0) {
        stp = 1;
    } else if (c.a > 0 && c.a < 255) {
        stp = 1;
    }

    return (r & 0x1F) | ((g & 0x1F) << 5) | ((b & 0x1F) << 10) | ((stp & 1) << 15);
}

bool TIMEncoder::Encode(const DecodedTIM& tim, const std::string& outPath) {
    FILE* file = fopen(outPath.c_str(), "wb");
    if (!file) return false;

    TIM_FILE_HEADER hdr;
    hdr.id = 0x10;
    hdr.ver = 0;
    hdr.pad1_0 = 0; hdr.pad1_1 = 0;
    hdr.pad2_0 = 0; hdr.pad2_1 = 0; hdr.pad2_2 = 0;
    
    bool has_clut = (tim.bpp == 0 || tim.bpp == 1);
    hdr.bpp_and_flags = tim.bpp | (has_clut ? 0x08 : 0x00);
    
    fwrite(&hdr, sizeof(TIM_FILE_HEADER), 1, file);

    if (has_clut) {
        TIM_CLUT_HEADER clut_hdr;
        clut_hdr.x = tim.clutX;
        clut_hdr.y = tim.clutY;
        clut_hdr.width = (tim.palettes.empty()) ? 0 : tim.palettes[0].colors.size();
        clut_hdr.height = tim.palettes.size();
        
        int clut_data_len = clut_hdr.width * clut_hdr.height * 2;
        clut_hdr.clut_length = sizeof(TIM_CLUT_HEADER) + clut_data_len;
        
        fwrite(&clut_hdr, sizeof(TIM_CLUT_HEADER), 1, file);
        
        std::vector<uint16_t> clut_data(clut_hdr.width * clut_hdr.height);
        for (int row = 0; row < clut_hdr.height; ++row) {
            for (int col = 0; col < clut_hdr.width; ++col) {
                clut_data[row * clut_hdr.width + col] = ColorToWord(tim.palettes[row].colors[col]);
            }
        }
        fwrite(clut_data.data(), 1, clut_data_len, file);
    }

    TIM_IMG_HEADER img_hdr;
    img_hdr.x = tim.imgX;
    img_hdr.y = tim.imgY;
    img_hdr.height = tim.height;
    
    std::vector<uint8_t> img_data;
    if (tim.bpp == 0) { // 4-bit
        img_hdr.width = tim.width / 4;
        int stride = tim.width / 2;
        img_data.resize(tim.height * stride);
        for (int y = 0; y < tim.height; ++y) {
            for (int x = 0; x < stride; ++x) {
                uint8_t low = tim.rawIndices[y * tim.width + x * 2 + 0] & 0x0F;
                uint8_t high = tim.rawIndices[y * tim.width + x * 2 + 1] & 0x0F;
                img_data[y * stride + x] = low | (high << 4);
            }
        }
    } else if (tim.bpp == 1) { // 8-bit
        img_hdr.width = tim.width / 2;
        img_data = tim.rawIndices;
    } else if (tim.bpp == 2) { // 16-bit
        img_hdr.width = tim.width;
        img_data.resize(tim.width * tim.height * 2);
        uint16_t* words = (uint16_t*)img_data.data();
        for (int i = 0; i < tim.width * tim.height; ++i) {
            words[i] = ColorToWord(tim.directPixels[i]);
        }
    } else {
        fclose(file);
        return false;
    }
    
    img_hdr.img_length = sizeof(TIM_IMG_HEADER) + img_data.size();
    fwrite(&img_hdr, sizeof(TIM_IMG_HEADER), 1, file);
    fwrite(img_data.data(), 1, img_data.size(), file);
    
    fclose(file);
    return true;
}
