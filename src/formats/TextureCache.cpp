#include "formats/TextureCache.h"
#include "core/Textures.h"
#include <cstdio>

// ---------------------------------------------------------------------------
// TextureCache Implementation
// ---------------------------------------------------------------------------

TextureCache& TextureCache::Get() {
    static TextureCache instance;
    return instance;
}

Texture2D TextureCache::Fetch(const std::string& texName, int paletteRow,
                              const std::string& workspaceDir) {
    Texture2D empty = { 0 };
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
        std::string timPath = workspaceDir + "/TIM/" + texName + ".TIM";
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
    std::string timPath = workspaceDir + "/TIM/" + texName + ".TIM";
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
        std::string timPath = workspaceDir + "/TIM/" + texName + ".TIM";
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
            if (tex.id != 0) {
                UnloadTexture(tex);
            }
        }
        entry.paletteTextures.clear();
        entry.image = nullptr;
    }
    m_entries.clear();
}
