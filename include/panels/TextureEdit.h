#pragma once
#include "imgui.h"
#include "raylib.h"
#include "core/Textures.h"
#include "formats/Structs.h"
#include "formats/IPDParse.h"

class TextureEditPanel {
public:
    TextureEditPanel() = default;

    // Draws the texture and handles UV dragging.
    // Returns true if the UVs were modified by a completed drag operation.
    // When true is returned, outSnapBefore will contain the mesh state before dragging,
    // outMinUV and outMaxUV will contain the new UV bounds.
    bool Draw(Textures& testTexture, RenderFace* activeFace, RenderMesh* activeMesh, bool snapToGrid, float& textureScale, RenderMesh& outSnapBefore, ImVec2& outMinUV, ImVec2& outMaxUV);

private:
    bool m_isDraggingUV = false;
    ImVec2 m_dragStartUV;
    ImVec2 m_dragEndUV;
    RenderMesh m_dragStartMesh;
};

