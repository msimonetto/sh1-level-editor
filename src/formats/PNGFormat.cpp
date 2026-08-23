#include "formats/PNGFormat.h"
#include "raylib.h"
#include <vector>
#include <cstdio>

static Color ToRaylibColor(const TIMColor& c) {
    return {c.r, c.g, c.b, c.a};
}

static TIMColor FromRaylibColor(const Color& c) {
    return {c.r, c.g, c.b, c.a};
}

bool PNGFormat::Save(const DecodedTIM& tim, const std::string& outPathStem) {
    if (tim.bpp == 0 || tim.bpp == 1) { // 4-bit or 8-bit indexed
        if (tim.rawIndices.empty()) return false;
        
        Image idxImg = {0};
        idxImg.width = tim.width;
        idxImg.height = tim.height;
        idxImg.mipmaps = 1;
        idxImg.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
        idxImg.data = (void*)tim.rawIndices.data();
        
        ExportImage(idxImg, (outPathStem + ".png").c_str());

        // Save CLUTs
        if (!tim.palettes.empty()) {
            int clutWidth = tim.palettes[0].colors.size();
            int clutHeight = tim.palettes.size();
            
            std::vector<Color> clutPixels(clutWidth * clutHeight);
            for (int y = 0; y < clutHeight; ++y) {
                for (int x = 0; x < clutWidth; ++x) {
                    clutPixels[y * clutWidth + x] = ToRaylibColor(tim.palettes[y].colors[x]);
                }
            }
            
            Image clutImg = {0};
            clutImg.width = clutWidth;
            clutImg.height = clutHeight;
            clutImg.mipmaps = 1;
            clutImg.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            clutImg.data = clutPixels.data();
            
            ExportImage(clutImg, (outPathStem + "_cluts.png").c_str());
        }
    } else {
        if (tim.directPixels.empty()) return false;
        
        std::vector<Color> pixels(tim.width * tim.height);
        for (int i = 0; i < tim.width * tim.height; ++i) {
            pixels[i] = ToRaylibColor(tim.directPixels[i]);
        }
        
        Image img = {0};
        img.width = tim.width;
        img.height = tim.height;
        img.mipmaps = 1;
        img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        img.data = pixels.data();
        ExportImage(img, (outPathStem + ".png").c_str());
    }
    return true;
}

bool PNGFormat::Load(const std::string& inPathStem, DecodedTIM& outTim) {
    outTim = DecodedTIM(); // clear

    std::string mainFile = inPathStem + ".png";
    std::string clutFile = inPathStem + "_cluts.png";
    
    if (!FileExists(mainFile.c_str())) {
        printf("File not found: %s\n", mainFile.c_str());
        return false;
    }

    if (FileExists(clutFile.c_str())) {
        // Indexed Mode
        Image idxImg = LoadImage(mainFile.c_str());
        Image clutImg = LoadImage(clutFile.c_str());
        
        if (idxImg.data == nullptr || clutImg.data == nullptr) {
            if (idxImg.data) UnloadImage(idxImg);
            if (clutImg.data) UnloadImage(clutImg);
            return false;
        }

        outTim.width = idxImg.width;
        outTim.height = idxImg.height;
        
        ImageFormat(&idxImg, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
        ImageFormat(&clutImg, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        // Load Palettes
        int clutWidth = clutImg.width;
        int clutHeight = clutImg.height;
        outTim.bpp = (clutWidth <= 16) ? 0 : 1;
        
        Color* clutPixels = (Color*)clutImg.data;
        for (int y = 0; y < clutHeight; ++y) {
            TIMPalette pal;
            for (int x = 0; x < clutWidth; ++x) {
                pal.colors.push_back(FromRaylibColor(clutPixels[y * clutWidth + x]));
            }
            outTim.palettes.push_back(pal);
        }

        // Load Indices
        outTim.rawIndices.resize(outTim.width * outTim.height);
        uint8_t* rawData = (uint8_t*)idxImg.data;
        for (int i = 0; i < outTim.width * outTim.height; ++i) {
            outTim.rawIndices[i] = rawData[i];
        }

        UnloadImage(idxImg);
        UnloadImage(clutImg);
        
    } else {
        // Direct Color
        Image mainImg = LoadImage(mainFile.c_str());
        if (mainImg.data == nullptr) return false;
        
        outTim.width = mainImg.width;
        outTim.height = mainImg.height;
        outTim.bpp = 2; // Assume 16-bit direct color for now
        
        ImageFormat(&mainImg, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        // Populate decoded struct
        outTim.directPixels.resize(outTim.width * outTim.height);
        Color* rawData = (Color*)mainImg.data;
        for (int i = 0; i < outTim.width * outTim.height; ++i) {
            outTim.directPixels[i] = FromRaylibColor(rawData[i]);
        }
        UnloadImage(mainImg);
    }

    return true;
}
