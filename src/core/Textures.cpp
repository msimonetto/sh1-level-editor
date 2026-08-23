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
