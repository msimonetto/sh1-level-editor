#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "raylib.h"
#include "core/structs.h"

struct TIMPalette {
    std::vector<Color> colors;
};

class Textures {
public:
    Textures();
    ~Textures();

    bool Load(const std::string& filepath);
    void Unload();

    // PNG Conversion
    bool SaveToPNG(const std::string& outPathStem);
    bool LoadFromPNG(const std::string& inPathStem);

    // For ImGui rendering
    Texture2D GetTexture() const { return m_texture; }
    
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetBpp() const { return m_bpp; }
    
    const std::vector<uint8_t>& GetRawIndices() const { return m_rawIndices; }
    const std::vector<TIMPalette>& GetPalettes() const { return m_palettes; }

    // Apply a different CLUT to the texture and upload to GPU
    void ApplyPalette(int paletteIndex);

    // Build a standalone GPU texture for a specific palette row without
    // modifying the current m_texture.  Returns a Texture2D the caller owns
    // (must UnloadTexture when done).
    Texture2D BuildPaletteTexture(int paletteIndex) const;

private:
    Texture2D m_texture; // Raylib GPU texture
    Image m_image;       // Raylib CPU image (RGBA8888)

    int m_width;
    int m_height;
    int m_bpp; // 0=4-bit, 1=8-bit, 2=16-bit, 3=24-bit
    
    std::vector<uint8_t> m_rawIndices; // Stored indices for 4-bit / 8-bit
    std::vector<TIMPalette> m_palettes;
    
    Color WordToColor(uint16_t word);
};

// ---------------------------------------------------------------------------
// TextureCache — shared GPU texture registry for the 3D viewport.
//
// Key  : (texName, paletteRow)
// Value: a Raylib Texture2D uploaded once and reused across all chunks.
//
// Usage:
//   Texture2D tex = TextureCache::Get().Fetch("THRFF01", 3, workspaceDir);
//   // ... render ...
//   TextureCache::Get().UnloadAll();  // call on shutdown
// ---------------------------------------------------------------------------
class TextureCache {
public:
    static TextureCache& Get();

    // Return (or load+cache) the GPU texture for the given texture name and
    // palette row.  workspaceDir is used to find the TIM file under
    // {workspaceDir}/textures/{texName}.TIM.
    // Returns a Texture2D with id==0 if the file cannot be found/loaded.
    Texture2D Fetch(const std::string& texName, int paletteRow,
                    const std::string& workspaceDir);

    // Preloads the TIM image from disk into RAM. Thread-safe and safe to call
    // from a background worker thread (does NOT interact with OpenGL).
    void Preload(const std::string& texName, const std::string& workspaceDir);

    // Return the pixel dimensions of a texture (loads it if needed).
    // Returns {256,256} as a safe fallback if loading fails.
    void GetDimensions(const std::string& texName, const std::string& workspaceDir,
                       int& w, int& h);

    // Release all cached GPU textures.  Call before CloseWindow().
    void UnloadAll();

private:
    TextureCache() = default;

    struct CacheEntry {
        std::shared_ptr<Textures> image;  // owns the raw data
        bool loadFailed = false;
        // Per-palette GPU texture: index = paletteRow
        std::map<int, Texture2D> paletteTextures;
    };

    // texName → cache entry
    std::map<std::string, CacheEntry> m_entries;
    std::mutex m_mutex;
};

