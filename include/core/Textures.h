#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "raylib.h"
#include "formats/Structs.h"
#include "formats/TIMDecoder.h"
class Textures {
public:
    Textures();
    ~Textures();

    bool Load(const std::string& filepath);
    void Unload();


    // For ImGui rendering
    Texture2D GetTexture() const { return m_texture; }
    
    int GetWidth() const { return m_decoded.width; }
    int GetHeight() const { return m_decoded.height; }
    int GetBpp() const { return m_decoded.bpp; }
    
    const std::vector<uint8_t>& GetRawIndices() const { return m_decoded.rawIndices; }
    const std::vector<TIMPalette>& GetPalettes() const { return m_decoded.palettes; }

    // Apply a different CLUT to the texture and upload to GPU
    void ApplyPalette(int paletteIndex);

    // Build a standalone GPU texture for a specific palette row without
    // modifying the current m_texture.  Returns a Texture2D the caller owns
    // (must UnloadTexture when done).
    Texture2D BuildPaletteTexture(int paletteIndex) const;

private:
    Texture2D m_texture; // Raylib GPU texture
    Image m_image;       // Raylib CPU image (RGBA8888)

    DecodedTIM m_decoded;
};

#include "formats/TextureCache.h"


