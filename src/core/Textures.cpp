#include "core/Textures.h"
#include "formats/TIMDecoder.h"
#include "formats/TIMEncoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Color ToRaylibColor(const TIMColor& c) {
    return {c.r, c.g, c.b, c.a};
}

static TIMColor FromRaylibColor(const Color& c) {
    return {c.r, c.g, c.b, c.a};
}

Textures::Textures() {
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
    m_decoded = DecodedTIM();
}

bool Textures::Load(const std::string& filepath) {
    Unload();

    if (!TIMDecoder::Decode(filepath, m_decoded)) {
        printf("Failed to decode TIM file: %s\n", filepath.c_str());
        return false;
    }

    if (m_decoded.bpp == 2) { // 16-bit direct color
        m_image = GenImageColor(m_decoded.width, m_decoded.height, BLANK);
        Color* pixels = (Color*)m_image.data;
        for (int i = 0; i < m_decoded.width * m_decoded.height; ++i) {
            pixels[i] = ToRaylibColor(m_decoded.directPixels[i]);
        }
        m_texture = LoadTextureFromImage(m_image);
        return true;
    }

    // Apply default palette 0 for 4-bit/8-bit
    m_image = GenImageColor(m_decoded.width, m_decoded.height, BLANK);
    if (!m_decoded.palettes.empty()) {
        ApplyPalette(0);
    }
    
    return true;
}

void Textures::ApplyPalette(int paletteIndex) {
    if (paletteIndex < 0 || paletteIndex >= (int)m_decoded.palettes.size() || m_image.data == nullptr) {
        return;
    }
    
    const auto& pal = m_decoded.palettes[paletteIndex];
    Color* pixels = (Color*)m_image.data;
    
    for (size_t i = 0; i < m_decoded.rawIndices.size(); ++i) {
        uint8_t idx = m_decoded.rawIndices[i];
        if (idx < pal.colors.size()) {
            pixels[i] = ToRaylibColor(pal.colors[idx]);
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
    if (m_decoded.rawIndices.empty() && m_image.data == nullptr) return false;

    if (m_decoded.bpp == 0 || m_decoded.bpp == 1) { // 4-bit or 8-bit indexed
        // Save indices as grayscale PNG
        Image idxImg = {0};
        idxImg.width = m_decoded.width;
        idxImg.height = m_decoded.height;
        idxImg.mipmaps = 1;
        idxImg.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
        idxImg.data = m_decoded.rawIndices.data();
        
        ExportImage(idxImg, (outPathStem + ".png").c_str());

        // Save CLUTs
        if (!m_decoded.palettes.empty()) {
            int clutWidth = m_decoded.palettes[0].colors.size();
            int clutHeight = m_decoded.palettes.size();
            
            std::vector<Color> clutPixels(clutWidth * clutHeight);
            for (int y = 0; y < clutHeight; ++y) {
                for (int x = 0; x < clutWidth; ++x) {
                    clutPixels[y * clutWidth + x] = ToRaylibColor(m_decoded.palettes[y].colors[x]);
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

        m_decoded.width = idxImg.width;
        m_decoded.height = idxImg.height;
        
        ImageFormat(&idxImg, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);
        ImageFormat(&clutImg, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);

        // Load Palettes
        int clutWidth = clutImg.width;
        int clutHeight = clutImg.height;
        m_decoded.bpp = (clutWidth <= 16) ? 0 : 1;
        
        Color* clutPixels = (Color*)clutImg.data;
        for (int y = 0; y < clutHeight; ++y) {
            TIMPalette pal;
            for (int x = 0; x < clutWidth; ++x) {
                pal.colors.push_back(FromRaylibColor(clutPixels[y * clutWidth + x]));
            }
            m_decoded.palettes.push_back(pal);
        }

        // Load Indices
        m_decoded.rawIndices.resize(m_decoded.width * m_decoded.height);
        uint8_t* rawData = (uint8_t*)idxImg.data;
        for (int i = 0; i < m_decoded.width * m_decoded.height; ++i) {
            m_decoded.rawIndices[i] = rawData[i];
        }

        UnloadImage(idxImg);
        UnloadImage(clutImg);
        
        // Generate m_image (apply first palette)
        m_image = GenImageColor(m_decoded.width, m_decoded.height, BLANK);
        ApplyPalette(0);
        
    } else {
        // Direct Color
        m_image = LoadImage(mainFile.c_str());
        if (m_image.data == nullptr) return false;
        
        m_decoded.width = m_image.width;
        m_decoded.height = m_image.height;
        m_decoded.bpp = 2; // Assume 16-bit direct color for now
        
        ImageFormat(&m_image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        m_texture = LoadTextureFromImage(m_image);

        // Populate decoded struct
        m_decoded.directPixels.resize(m_decoded.width * m_decoded.height);
        Color* rawData = (Color*)m_image.data;
        for (int i = 0; i < m_decoded.width * m_decoded.height; ++i) {
            m_decoded.directPixels[i] = FromRaylibColor(rawData[i]);
        }
    }

    return true;
}

Texture2D Textures::BuildPaletteTexture(int paletteIndex) const {
    Texture2D empty = {0};
    if (m_decoded.rawIndices.empty() || paletteIndex < 0 ||
        paletteIndex >= (int)m_decoded.palettes.size()) {
        if (m_decoded.bpp == 2 && m_texture.id != 0) {
            return m_texture;
        }
        return empty;
    }

    const auto& pal = m_decoded.palettes[paletteIndex];
    Image tmp = GenImageColor(m_decoded.width, m_decoded.height, BLANK);
    Color* pixels = (Color*)tmp.data;
    for (size_t i = 0; i < m_decoded.rawIndices.size(); ++i) {
        uint8_t idx = m_decoded.rawIndices[i];
        pixels[i] = (idx < pal.colors.size()) ? ToRaylibColor(pal.colors[idx]) : BLANK;
    }
    Texture2D tex = LoadTextureFromImage(tmp);
    UnloadImage(tmp);
    return tex;
}

// ---------------------------------------------------------------------------
// TextureCache implementation (unchanged logic)
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

    Texture2D tex = img->BuildPaletteTexture(paletteRow);
    
    if (tex.id != 0) {
        SetTextureFilter(tex, TEXTURE_FILTER_POINT);
        
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
    w = 256; h = 256; 

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
            if (entry.image && tex.id != entry.image->GetTexture().id) {
                UnloadTexture(tex);
            }
        }
        entry.paletteTextures.clear();
        entry.image.reset();
    }
    m_entries.clear();
}
