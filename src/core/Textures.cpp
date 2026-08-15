#include "core/Textures.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Textures::Textures() : m_width(0), m_height(0), m_bpp(0) {
    m_texture.id = 0;
    m_image.data = nullptr;
}

Textures::~Textures() {
    Unload();
}

void Textures::Unload() {
    if (m_texture.id != 0) {
        UnloadTexture(m_texture);
        m_texture.id = 0;
    }
    if (m_image.data != nullptr) {
        UnloadImage(m_image);
        m_image.data = nullptr;
    }
    m_palettes.clear();
    m_rawIndices.clear();
}

Color Textures::WordToColor(uint16_t word) {
    uint8_t r = (word & 0x001F) * 255 / 31;
    uint8_t g = ((word & 0x03E0) >> 5) * 255 / 31;
    uint8_t b = ((word & 0x7C00) >> 10) * 255 / 31;
    uint8_t stp = (word >> 15) & 1;

    uint8_t a = 255;
    if (word == 0x0000) a = 0;
    else if (stp == 1) a = 255;
    else a = 127;
    return {r, g, b, a};
}

bool Textures::Load(const std::string& filepath) {
    Unload();

    FILE* file = fopen(filepath.c_str(), "rb");
    if (!file) {
        printf("Failed to open TIM file: %s\n", filepath.c_str());
        return false;
    }

    TIM_FILE_HEADER hdr;
    if (fread(&hdr, sizeof(TIM_FILE_HEADER), 1, file) != 1 || hdr.id != 0x10) {
        fclose(file);
        return false;
    }

    m_bpp = hdr.bpp_and_flags & 0x03;
    bool has_clut = (hdr.bpp_and_flags & 0x08) != 0;

    if (has_clut) {
        TIM_CLUT_HEADER clut_hdr;
        if (fread(&clut_hdr, sizeof(TIM_CLUT_HEADER), 1, file) != 1) {
            fclose(file);
            return false;
        }

        int clut_data_len = clut_hdr.clut_length - sizeof(TIM_CLUT_HEADER);
        std::vector<uint16_t> clut_data(clut_data_len / 2);
        fread(clut_data.data(), 1, clut_data_len, file);

        for (int row = 0; row < clut_hdr.height; ++row) {
            TIMPalette pal;
            for (int col = 0; col < clut_hdr.width; ++col) {
                uint16_t word = clut_data[row * clut_hdr.width + col];
                pal.colors.push_back(WordToColor(word));
            }
            m_palettes.push_back(pal);
        }
    }

    TIM_IMG_HEADER img_hdr;
    if (fread(&img_hdr, sizeof(TIM_IMG_HEADER), 1, file) != 1) {
        fclose(file);
        return false;
    }

    int img_data_len = img_hdr.img_length - sizeof(TIM_IMG_HEADER);
    std::vector<uint8_t> img_data(img_data_len);
    fread(img_data.data(), 1, img_data_len, file);
    fclose(file);

    if (m_bpp == 0) { // 4-bit
        m_width = img_hdr.width * 4;
        m_height = img_hdr.height;
        m_rawIndices.resize(m_width * m_height);
        
        int stride = m_width / 2;
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < stride; ++x) {
                uint8_t byte_val = img_data[y * stride + x];
                m_rawIndices[y * m_width + x * 2 + 0] = byte_val & 0x0F;
                m_rawIndices[y * m_width + x * 2 + 1] = (byte_val >> 4) & 0x0F;
            }
        }
    } else if (m_bpp == 1) { // 8-bit
        m_width = img_hdr.width * 2;
        m_height = img_hdr.height;
        m_rawIndices = img_data;
    } else if (m_bpp == 2) { // 16-bit
        m_width = img_hdr.width;
        m_height = img_hdr.height;
        
        m_image = GenImageColor(m_width, m_height, BLANK);
        Color* pixels = (Color*)m_image.data;
        
        uint16_t* words = (uint16_t*)img_data.data();
        for (int i = 0; i < m_width * m_height; ++i) {
            pixels[i] = WordToColor(words[i]);
        }
        
        m_texture = LoadTextureFromImage(m_image);
        return true; // 16-bit has no CLUT, we are done
    } else {
        return false; // Unsupported
    }

    // Apply default palette 0 for 4-bit/8-bit
    m_image = GenImageColor(m_width, m_height, BLANK);
    if (!m_palettes.empty()) {
        ApplyPalette(0);
    }
    
    return true;
}

void Textures::ApplyPalette(int paletteIndex) {
    if (paletteIndex < 0 || paletteIndex >= (int)m_palettes.size() || m_image.data == nullptr) {
        return;
    }
    
    const auto& pal = m_palettes[paletteIndex];
    Color* pixels = (Color*)m_image.data;
    
    for (size_t i = 0; i < m_rawIndices.size(); ++i) {
        uint8_t idx = m_rawIndices[i];
        if (idx < pal.colors.size()) {
            pixels[i] = pal.colors[idx];
        } else {
            pixels[i] = BLANK;
        }
    }

    if (m_texture.id != 0) {
        UnloadTexture(m_texture);
    }
    m_texture = LoadTextureFromImage(m_image);
}

bool Textures::SaveToPNG(const std::string& outPathStem) {
    if (m_rawIndices.empty() && m_image.data == nullptr) return false;

    if (m_bpp == 0 || m_bpp == 1) { // 4-bit or 8-bit indexed
        // Save indices as grayscale PNG
        Image idxImg = {0};
        idxImg.width = m_width;
        idxImg.height = m_height;
        idxImg.mipmaps = 1;
        idxImg.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
        idxImg.data = m_rawIndices.data();
        
        ExportImage(idxImg, (outPathStem + ".png").c_str());

        // Save CLUTs
        if (!m_palettes.empty()) {
            int clutWidth = m_palettes[0].colors.size();
            int clutHeight = m_palettes.size();
            
            std::vector<Color> clutPixels(clutWidth * clutHeight);
            for (int y = 0; y < clutHeight; ++y) {
                for (int x = 0; x < clutWidth; ++x) {
                    clutPixels[y * clutWidth + x] = m_palettes[y].colors[x];
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
        // Direct color, just save m_image
        if (m_image.data) {
            ExportImage(m_image, (outPathStem + ".png").c_str());
        }
    }
    return true;
}

bool Textures::LoadFromPNG(const std::string& inPathStem) {
    Unload();
    
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

        m_width = idxImg.width;
        m_height = idxImg.height;
        
        ImageFormat(&idxImg, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
        ImageFormat(&clutImg, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        // Load Palettes
        int clutWidth = clutImg.width;
        int clutHeight = clutImg.height;
        m_bpp = (clutWidth <= 16) ? 0 : 1;
        
        Color* clutPixels = (Color*)clutImg.data;
        for (int y = 0; y < clutHeight; ++y) {
            TIMPalette pal;
            for (int x = 0; x < clutWidth; ++x) {
                pal.colors.push_back(clutPixels[y * clutWidth + x]);
            }
            m_palettes.push_back(pal);
        }

        // Load Indices
        m_rawIndices.resize(m_width * m_height);
        uint8_t* rawData = (uint8_t*)idxImg.data;
        for (int i = 0; i < m_width * m_height; ++i) {
            m_rawIndices[i] = rawData[i];
        }

        UnloadImage(idxImg);
        UnloadImage(clutImg);
        
        // Generate m_image (apply first palette)
        m_image = GenImageColor(m_width, m_height, BLANK);
        ApplyPalette(0);
        
    } else {
        // Direct Color
        m_image = LoadImage(mainFile.c_str());
        if (m_image.data == nullptr) return false;
        
        m_width = m_image.width;
        m_height = m_image.height;
        m_bpp = 2; // Assume 16-bit direct color for now
        
        ImageFormat(&m_image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        m_texture = LoadTextureFromImage(m_image);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Textures::BuildPaletteTexture
// Returns a new Texture2D for the given palette row without modifying
// m_texture.  The caller is responsible for calling UnloadTexture().
// ---------------------------------------------------------------------------
Texture2D Textures::BuildPaletteTexture(int paletteIndex) const {
    Texture2D empty = {0};
    if (m_rawIndices.empty() || paletteIndex < 0 ||
        paletteIndex >= (int)m_palettes.size()) {
        // For 16-bit (no CLUT), return m_texture directly (caller should not unload it)
        if (m_bpp == 2 && m_texture.id != 0) {
            return m_texture;
        }
        return empty;
    }

    const auto& pal = m_palettes[paletteIndex];
    Image tmp = GenImageColor(m_width, m_height, BLANK);
    Color* pixels = (Color*)tmp.data;
    for (size_t i = 0; i < m_rawIndices.size(); ++i) {
        uint8_t idx = m_rawIndices[i];
        pixels[i] = (idx < pal.colors.size()) ? pal.colors[idx] : BLANK;
    }
    Texture2D tex = LoadTextureFromImage(tmp);
    UnloadImage(tmp);
    return tex;
}

// ---------------------------------------------------------------------------
// TextureCache implementation
// ---------------------------------------------------------------------------

TextureCache& TextureCache::Get() {
    static TextureCache instance;
    return instance;
}

Texture2D TextureCache::Fetch(const std::string& texName, int paletteRow,
                               const std::string& workspaceDir) {
    Texture2D empty = {0};

    std::shared_ptr<Textures> img;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_entries.find(texName) != m_entries.end()) {
            if (m_entries[texName].loadFailed) return empty;
            img = m_entries[texName].image;
        }
    }
    
    if (!img) {
        // Fallback File IO outside lock
        auto newImg = std::make_shared<Textures>();
        std::string timPath = workspaceDir + "/textures/" + texName + ".TIM";
        if (!newImg->Load(timPath)) {
            printf("[TextureCache] Failed to load TIM: %s\n", timPath.c_str());
            std::lock_guard<std::mutex> lock(m_mutex);
            m_entries[texName].loadFailed = true;
            return empty;
        }
        
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& entry = m_entries[texName];
        if (!entry.image) {
            entry.image = newImg;
            entry.loadFailed = false;
        }
        img = entry.image;
    }
    
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& entry = m_entries[texName];
        auto it = entry.paletteTextures.find(paletteRow);
        if (it != entry.paletteTextures.end()) {
            return it->second;
        }
    }

    // Build and cache the palette texture (calling OpenGL MUST be done outside the mutex if we can, 
    // but we need to insert it back into the map safely)
    Texture2D tex = img->BuildPaletteTexture(paletteRow);
    
    if (tex.id != 0) {
        SetTextureFilter(tex, TEXTURE_FILTER_POINT); // nearest-neighbour for pixel art
        
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries[texName].paletteTextures[paletteRow] = tex;
    }
    return tex;
}

void TextureCache::Preload(const std::string& texName, const std::string& workspaceDir) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_entries.find(texName) != m_entries.end()) {
            if (m_entries[texName].image || m_entries[texName].loadFailed) {
                return;
            }
        }
    }
    
    // File IO outside lock
    auto img = std::make_shared<Textures>();
    std::string timPath = workspaceDir + "/textures/" + texName + ".TIM";
    if (img->Load(timPath)) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& entry = m_entries[texName];
        if (!entry.image) {
            entry.image = img;
            entry.loadFailed = false;
        }
    } else {
        printf("[TextureCache] Failed to preload TIM: %s\n", timPath.c_str());
        std::lock_guard<std::mutex> lock(m_mutex);
        m_entries[texName].loadFailed = true;
    }
}


void TextureCache::GetDimensions(const std::string& texName,
                                   const std::string& workspaceDir,
                                   int& w, int& h) {
    w = 256; h = 256; // safe fallback

    std::lock_guard<std::mutex> lock(m_mutex);
    auto& entry = m_entries[texName];
    if (entry.loadFailed) return;

    if (!entry.image) {
        entry.image = std::make_shared<Textures>();
        std::string timPath = workspaceDir + "/textures/" + texName + ".TIM";
        if (!entry.image->Load(timPath)) {
            entry.loadFailed = true;
            entry.image = nullptr;
            return;
        }
        entry.loadFailed = false;
    }
    w = entry.image->GetWidth();
    h = entry.image->GetHeight();
}

void TextureCache::UnloadAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [name, entry] : m_entries) {
        for (auto& [row, tex] : entry.paletteTextures) {
            // Don't unload textures that are actually m_texture (16-bit case)
            if (entry.image && tex.id != entry.image->GetTexture().id) {
                UnloadTexture(tex);
            }
        }
        entry.paletteTextures.clear();
        entry.image.reset();
    }
    m_entries.clear();
}
