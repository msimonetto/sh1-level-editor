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

Shader TextureCache::GetAlphaCutoutShader() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_alphaShaderLoaded) {
        return m_alphaCutoutShader;
    }

    const char* appDir = GetApplicationDirectory();
    std::string shaderPath = "";
    if (FileExists(TextFormat("%s../res/shaders/alpha_discard.fs", appDir))) {
        shaderPath = TextFormat("%s../res/shaders/alpha_discard.fs", appDir);
    } else if (FileExists(TextFormat("%sres/shaders/alpha_discard.fs", appDir))) {
        shaderPath = TextFormat("%sres/shaders/alpha_discard.fs", appDir);
    } else if (FileExists("res/shaders/alpha_discard.fs")) {
        shaderPath = "res/shaders/alpha_discard.fs";
    }

    if (!shaderPath.empty()) {
        m_alphaCutoutShader = LoadShader(0, shaderPath.c_str());
        m_alphaShaderLoaded = (m_alphaCutoutShader.id != 0);
    }

    if (!m_alphaShaderLoaded) {
        // Embedded GLSL fallback
        static const char* fsSource =
            "#version 330\n"
            "in vec2 fragTexCoord;\n"
            "in vec4 fragColor;\n"
            "uniform sampler2D texture0;\n"
            "uniform vec4 colDiffuse;\n"
            "out vec4 finalColor;\n"
            "void main()\n"
            "{\n"
            "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
            "    vec4 color = texelColor * colDiffuse * fragColor;\n"
            "    if (color.a < 0.1) {\n"
            "        discard;\n"
            "    }\n"
            "    finalColor = color;\n"
            "}\n";

        m_alphaCutoutShader = LoadShaderFromMemory(0, fsSource);
        m_alphaShaderLoaded = (m_alphaCutoutShader.id != 0);
    }

    return m_alphaCutoutShader;
}

Material TextureCache::CreateMeshMaterial(const std::string& texName, int paletteRow,
                                          const std::string& workspaceDir) {
    Material mat = LoadMaterialDefault();
    mat.shader = GetAlphaCutoutShader();
    if (!texName.empty()) {
        Texture2D tex = Fetch(texName, paletteRow, workspaceDir);
        if (tex.id != 0) {
            mat.maps[MATERIAL_MAP_DIFFUSE].texture = tex;
        }
    }
    return mat;
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

    if (m_alphaShaderLoaded) {
        UnloadShader(m_alphaCutoutShader);
        m_alphaCutoutShader = {0};
        m_alphaShaderLoaded = false;
    }
}
