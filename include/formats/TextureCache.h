#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "raylib.h"

class Textures;

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
    // {workspaceDir}/TIM/{texName}.TIM.
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

    // Return the shared alpha cutout shader for 3D mesh rendering
    Shader GetAlphaCutoutShader();

    // Create a Raylib Material initialized with the alpha cutout shader and diffuse texture
    Material CreateMeshMaterial(const std::string& texName, int paletteRow,
                                const std::string& workspaceDir);

    // Invalidate and release cached GPU textures for a specific texture name so subsequent Fetch calls reload from disk
    void Invalidate(const std::string& texName);

    // Release all cached GPU textures and shader. Call before CloseWindow().
    void UnloadAll();

private:
    TextureCache() = default;

    struct CacheEntry {
        std::shared_ptr<Textures> image;  // owns the raw data
        bool loadFailed = false;
        // Per-palette GPU texture: index = paletteRow
        std::map<int, Texture2D> paletteTextures;
    };

    // Shared alpha-discard shader for 3D viewport materials
    Shader m_alphaCutoutShader = {0};
    bool m_alphaShaderLoaded = false;

    // texName → cache entry
    std::map<std::string, CacheEntry> m_entries;
    std::mutex m_mutex;
};
