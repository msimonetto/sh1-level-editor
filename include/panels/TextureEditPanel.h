#pragma once
#include "imgui.h"
#include "raylib.h"
#include "core/Textures.h"
#include "formats/Structs.h"
#include "formats/IPDParse.h"

class FileManager;
class Viewport;
class LocalGeometryOverlay;
class History;

class TextureEditPanel {
public:
    TextureEditPanel() = default;

    // Draws the "From file:" text box and "Browse..." file dialog button.
    void DrawFromFileControls(Textures &activeTexture, int &currentPalette,
                              RenderFace *activeFace, RenderMesh *activeMesh,
                              FileManager &fileManager, Viewport &sceneViewport,
                              LocalGeometryOverlay &localGeometryOverlay, History &history);
};