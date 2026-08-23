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
        return true;
    }

    // Apply default palette 0 for 4-bit/8-bit
    m_image = GenImageColor(m_decoded.width, m_decoded.height, BLANK);
    
    // We defer actual texture generation (ApplyPalette / LoadTextureFromImage)
    // to the main thread via BuildPaletteTexture, since Load() can run on background threads.
    
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
        if (m_decoded.bpp == 2) {
            if (m_texture.id == 0 && m_image.data != nullptr) {
                // Lazy-load OpenGL texture on main thread
                const_cast<Textures*>(this)->m_texture = LoadTextureFromImage(m_image);
            }
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

