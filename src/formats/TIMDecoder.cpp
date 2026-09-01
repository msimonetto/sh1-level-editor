#include "formats/TIMDecoder.h"
#include <cstdio>

// ---------------------------------------------------------------------------
// TIMColor Implementation
// ---------------------------------------------------------------------------
bool TIMColor::operator==(const TIMColor &o) const {
    return r == o.r && g == o.g && b == o.b && a == o.a;
}

bool TIMColor::operator!=(const TIMColor &o) const {
    return !(*this == o);
}

void TIMColor::ToR5G5B5(int &outR5, int &outG5, int &outB5, bool &outStp) const {
    outR5 = (int)r * 31 / 255;
    outG5 = (int)g * 31 / 255;
    outB5 = (int)b * 31 / 255;
    outStp = (a == 0) ? false : (a < 255 || (r == 0 && g == 0 && b == 0));
}

TIMColor TIMColor::FromR5G5B5(int r5, int g5, int b5, bool stp) {
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

// ---------------------------------------------------------------------------
// TIMPalette Implementation
// ---------------------------------------------------------------------------
bool TIMPalette::operator==(const TIMPalette &o) const {
    return colors == o.colors;
}

bool TIMPalette::operator!=(const TIMPalette &o) const {
    return !(*this == o);
}

// ---------------------------------------------------------------------------
// DecodedTIM Implementation
// ---------------------------------------------------------------------------
bool DecodedTIM::operator==(const DecodedTIM &o) const {
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

bool DecodedTIM::operator!=(const DecodedTIM &o) const {
    return !(*this == o);
}

// ---------------------------------------------------------------------------
// TIMDecoder Implementation
// ---------------------------------------------------------------------------
TIMColor TIMDecoder::WordToColor(uint16_t word) {
    uint8_t r = (word & 0x001F) * 255 / 31;
    uint8_t g = ((word & 0x03E0) >> 5) * 255 / 31;
    uint8_t b = ((word & 0x7C00) >> 10) * 255 / 31;
    uint8_t stp = (word >> 15) & 1;

    uint8_t a = 255;
    if (word == 0x0000) a = 0;
    else if (stp == 1) a = 255; 
    else a = 255;
    return {r, g, b, a};
}

bool TIMDecoder::Decode(const std::string& filepath, DecodedTIM& outTim) {
    outTim = DecodedTIM(); // Reset

    FILE* file = fopen(filepath.c_str(), "rb");
    if (!file) {
        printf("[TIMDecoder] fopen failed for %s (errno=%d)\n", filepath.c_str(), errno);
        return false;
    }

    TIM_FILE_HEADER hdr;
    if (fread(&hdr, sizeof(TIM_FILE_HEADER), 1, file) != 1) {
        printf("[TIMDecoder] fread hdr failed for %s\n", filepath.c_str());
        fclose(file);
        return false;
    }
    if (hdr.id != 0x10) {
        printf("[TIMDecoder] invalid magic 0x%X for %s\n", hdr.id, filepath.c_str());
        fclose(file);
        return false;
    }

    outTim.bpp = hdr.bpp_and_flags & 0x03;
    bool has_clut = (hdr.bpp_and_flags & 0x08) != 0;

    if (has_clut) {
        TIM_CLUT_HEADER clut_hdr;
        if (fread(&clut_hdr, sizeof(TIM_CLUT_HEADER), 1, file) != 1) {
            printf("[TIMDecoder] fread clut_hdr failed for %s\n", filepath.c_str());
            fclose(file);
            return false;
        }

        outTim.clutX = clut_hdr.x;
        outTim.clutY = clut_hdr.y;

        int clut_data_len = clut_hdr.clut_length - sizeof(TIM_CLUT_HEADER);
        std::vector<uint16_t> clut_data(clut_data_len / 2);
        fread(clut_data.data(), 1, clut_data_len, file);

        for (int row = 0; row < clut_hdr.height; ++row) {
            TIMPalette pal;
            for (int col = 0; col < clut_hdr.width; ++col) {
                uint16_t word = clut_data[row * clut_hdr.width + col];
                pal.colors.push_back(WordToColor(word));
            }
            outTim.palettes.push_back(pal);
        }
    }

    TIM_IMG_HEADER img_hdr;
    if (fread(&img_hdr, sizeof(TIM_IMG_HEADER), 1, file) != 1) {
        printf("[TIMDecoder] fread img_hdr failed for %s\n", filepath.c_str());
        fclose(file);
        return false;
    }

    outTim.imgX = img_hdr.x;
    outTim.imgY = img_hdr.y;

    int img_data_len = img_hdr.img_length - sizeof(TIM_IMG_HEADER);
    std::vector<uint8_t> img_data(img_data_len);
    fread(img_data.data(), 1, img_data_len, file);
    fclose(file);

    if (outTim.bpp == 0) { // 4-bit
        outTim.width = img_hdr.width * 4;
        outTim.height = img_hdr.height;
        outTim.rawIndices.resize(outTim.width * outTim.height);
        
        int stride = outTim.width / 2;
        for (int y = 0; y < outTim.height; ++y) {
            for (int x = 0; x < stride; ++x) {
                uint8_t byte_val = img_data[y * stride + x];
                outTim.rawIndices[y * outTim.width + x * 2 + 0] = byte_val & 0x0F;
                outTim.rawIndices[y * outTim.width + x * 2 + 1] = (byte_val >> 4) & 0x0F;
            }
        }
    } else if (outTim.bpp == 1) { // 8-bit
        outTim.width = img_hdr.width * 2;
        outTim.height = img_hdr.height;
        outTim.rawIndices = img_data;
    } else if (outTim.bpp == 2) { // 16-bit
        outTim.width = img_hdr.width;
        outTim.height = img_hdr.height;
        outTim.directPixels.resize(outTim.width * outTim.height);
        
        uint16_t* words = (uint16_t*)img_data.data();
        for (int i = 0; i < outTim.width * outTim.height; ++i) {
            outTim.directPixels[i] = WordToColor(words[i]);
        }
    } else {
        printf("[TIMDecoder] unsupported bpp %d for %s\n", outTim.bpp, filepath.c_str());
        return false;
    }

    return true;
}
