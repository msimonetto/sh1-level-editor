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
    void DrawFromFileControls(Textures &testTexture, int &currentPalette,
                              RenderFace *activeFace, RenderMesh *activeMesh,
                              FileManager &fileManager, Viewport &sceneViewport,
                              LocalGeometryOverlay &localGeometryOverlay, History &history);

    // Draws the texture and handles UV dragging.
    // Returns true if the UVs were modified by a completed drag operation.
    // When true is returned, outSnapBefore will contain the mesh state before dragging,
    // outMinUV and outMaxUV will contain the new UV bounds.
    bool Draw(Textures& testTexture, RenderFace* activeFace, RenderMesh* activeMesh, bool snapToGrid, RenderMesh& outSnapBefore, ImVec2& outMinUV, ImVec2& outMaxUV);

private:
    bool m_isDraggingUV = false;
    ImVec2 m_dragStartUV;
    ImVec2 m_dragEndUV;
    RenderMesh m_dragStartMesh;
};

