#include "panels/LocalGeometryPanel.h"
#include "viewport/LocalGeometry.h"
#include "core/History.h"
#include "geometry/ChunkValidator.h"
#include "geometry/SubdivideFace.h"
#include "geometry/MeshOperations.h"
#include "geometry/GlobalObjectOperations.h"
#include "geometry/FaceOperations.h"
#include "geometry/VertexOperations.h"
#include "panels/TextureMapPanel.h"
#include "imgui.h"
#include "extras/IconsFontAwesome6.h"
#include <cfloat>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

void LocalGeometryPanel::DrawContent(LocalGeometryOverlay &overlay, History *history) {
  DrawSelectionHeader(overlay);
  ImGui::Separator();

  // Mode-specific sections
  switch (overlay.m_editMode) {
    case EditMode::GlobalObject:
      DrawGlobalObjectsSection(overlay, history);
      break;
    case EditMode::Mesh:
      DrawMeshesSection(overlay, history);
      break;
    case EditMode::Face:
      DrawFacesSection(overlay, history);
      break;
    case EditMode::Vertex:
      DrawVerticesSection(overlay, history);
      break;
  }

  DrawValidationSection(overlay);
}

void LocalGeometryPanel::DrawSelectionHeader(LocalGeometryOverlay &overlay) {
  // 1. 4-Mode Switcher Bar
  ImGui::Text("Mode:");
  ImGui::SameLine();
  
  bool isGlob = (overlay.m_editMode == EditMode::GlobalObject);
  bool isMesh = (overlay.m_editMode == EditMode::Mesh);
  bool isFace = (overlay.m_editMode == EditMode::Face);
  bool isVert = (overlay.m_editMode == EditMode::Vertex);

  if (isGlob) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.6f, 0.7f, 1.0f));
  if (ImGui::Button(ICON_FA_GLOBE " Global")) overlay.m_editMode = EditMode::GlobalObject;
  if (isGlob) ImGui::PopStyleColor();

  ImGui::SameLine();
  if (isMesh) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
  if (ImGui::Button(ICON_FA_CUBES " Mesh")) overlay.m_editMode = EditMode::Mesh;
  if (isMesh) ImGui::PopStyleColor();

  ImGui::SameLine();
  if (isFace) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
  if (ImGui::Button(ICON_FA_VECTOR_SQUARE " Face")) overlay.m_editMode = EditMode::Face;
  if (isFace) ImGui::PopStyleColor();

  ImGui::SameLine();
  if (isVert) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
  if (ImGui::Button(ICON_FA_CIRCLE_DOT " Vertex")) overlay.m_editMode = EditMode::Vertex;
  if (isVert) ImGui::PopStyleColor();

  // 2. Dynamic Context Badge
  ImGui::Spacing();
  if (!overlay.m_selectedChunk.empty()) {
    std::string statsStr = "Chunk: " + overlay.m_selectedChunk;

    const RenderObject *selectedObj = nullptr;
    for (const auto &lc : overlay.GetChunks()) {
      if (lc.data->chunkName == overlay.m_selectedChunk &&
          overlay.m_selectedObjectIdx >= 0 &&
          overlay.m_selectedObjectIdx < (int)lc.data->objects.size()) {
        selectedObj = &lc.data->objects[overlay.m_selectedObjectIdx];
        break;
      }
    }

    if (overlay.m_editMode == EditMode::GlobalObject) {
      if (selectedObj && selectedObj->isGlobal) {
        statsStr += " | Prop: " + selectedObj->name + " (Obj #" + std::to_string(overlay.m_selectedObjectIdx) + ")";
        float px = (float)selectedObj->rawTx / 256.0f;
        float py = -(float)selectedObj->rawTy / 256.0f;
        float pz = -(float)selectedObj->rawTz / 256.0f;
        char posBuf[64];
        sprintf(posBuf, " | Pos: (%.1f, %.1f, %.1f)", px, py, pz);
        statsStr += posBuf;
      } else {
        statsStr += " | No global prop selected";
      }
      ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.9f, 1.0f), "%s", statsStr.c_str());
    } else if (overlay.m_editMode == EditMode::Mesh) {
      if (selectedObj && !selectedObj->isGlobal) {
        statsStr += " | Mesh Obj #" + std::to_string(overlay.m_selectedObjectIdx);
        if (overlay.m_selectedMeshIdx >= 0 && overlay.m_selectedMeshIdx < (int)selectedObj->meshes.size()) {
          const auto &m = selectedObj->meshes[overlay.m_selectedMeshIdx];
          statsStr += " (" + std::to_string(m.vx.size()) + " Verts, " + std::to_string(m.faces.size()) + " Faces)";
        }
      } else {
        statsStr += " | No local mesh selected";
      }
      ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", statsStr.c_str());
    } else if (overlay.m_editMode == EditMode::Face) {
      size_t faceCount = overlay.m_selectedFaces.size();
      if (faceCount == 0 && overlay.m_selectedFaceIdx >= 0) faceCount = 1;
      statsStr += " | Selected: " + std::to_string(faceCount) + " Face(s)";
      ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "%s", statsStr.c_str());
    } else if (overlay.m_editMode == EditMode::Vertex) {
      size_t vertCount = overlay.m_selectedVertices.size();
      if (vertCount == 0 && overlay.m_selectedVertexIdx >= 0) vertCount = 1;
      statsStr += " | Selected: " + std::to_string(vertCount) + " Vertex(es)";
      ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", statsStr.c_str());
    }
  } else {
    ImGui::TextDisabled("No chunk selected.");
  }

  // 3. Move Step (Power of 2 grid snap)
  ImGui::Spacing();
  char stepBuf[32];
  int rawUnits = 1 << overlay.m_moveStepPower;
  float meterUnits = (float)rawUnits / 256.0f;
  if (meterUnits >= 1.0f) {
    sprintf(stepBuf, "%.1f m (%d raw)", meterUnits, rawUnits);
  } else {
    sprintf(stepBuf, "1/%d m (%d raw)", (int)round(1.0f / meterUnits), rawUnits);
  }

  ImGui::Text("Grid Snap:");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 160.0f);
  ImGui::SliderInt("##MoveStep", &overlay.m_moveStepPower, 0, 10, stepBuf);

  // Quick preset snap buttons
  ImGui::SameLine();
  if (ImGui::Button("1/16")) overlay.m_moveStepPower = 4; // 16 raw = 1/16m
  ImGui::SameLine();
  if (ImGui::Button("1/4"))  overlay.m_moveStepPower = 6; // 64 raw = 1/4m
  ImGui::SameLine();
  if (ImGui::Button("1.0"))  overlay.m_moveStepPower = 8; // 256 raw = 1m
}

void LocalGeometryPanel::DrawGlobalObjectsSection(LocalGeometryOverlay &overlay, History *history) {
  float availW = ImGui::GetContentRegionAvail().x;
  float halfW = (availW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
  float thirdW = (availW - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;

  if (ImGui::CollapsingHeader(ICON_FA_GLOBE " Global Prop Transforms", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("Yaw Rotation (90°):");
    if (ImGui::Button(ICON_FA_ROTATE_LEFT " -90° (CCW)", ImVec2(thirdW, 0))) {
      Geometry::RotateGlobalObject(overlay, -1, history);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ROTATE_RIGHT " +90° (CW)", ImVec2(thirdW, 0))) {
      Geometry::RotateGlobalObject(overlay, +1, history);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ARROWS_ROTATE " 180°", ImVec2(thirdW, 0))) {
      Geometry::RotateGlobalObject(overlay, +2, history);
    }

    ImGui::Spacing();
    ImGui::Text("Mirror Prop:");
    if (ImGui::Button("Mirror X", ImVec2(thirdW, 0))) {
      Geometry::MirrorGlobalObject(overlay, 0, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("Mirror Y", ImVec2(thirdW, 0))) {
      Geometry::MirrorGlobalObject(overlay, 1, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("Mirror Z", ImVec2(thirdW, 0))) {
      Geometry::MirrorGlobalObject(overlay, 2, history);
    }

    ImGui::Spacing();
    ImGui::Text("Snapping:");
    if (ImGui::Button(ICON_FA_MAGNET " Snap to Grid", ImVec2(halfW, 0))) {
      Geometry::SnapGlobalObjectToGrid(overlay, history);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ARROW_DOWN " Snap to Floor (Y=0)", ImVec2(halfW, 0))) {
      Geometry::SnapGlobalObjectToFloor(overlay, history);
    }

    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader(ICON_FA_FOLDER_PLUS " Global Prop Library & Instancing", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::Button(ICON_FA_COPY " Duplicate Prop Instance", ImVec2(availW, 0))) {
      Geometry::DuplicateGlobalObject(overlay, history);
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button(ICON_FA_TRASH " Delete Prop Instance", ImVec2(availW, 0))) {
      Geometry::DeleteGlobalObject(overlay, history);
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
  }
}

void LocalGeometryPanel::DrawMeshesSection(LocalGeometryOverlay &overlay, History *history) {
  float availW = ImGui::GetContentRegionAvail().x;
  float halfW = (availW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
  float thirdW = (availW - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;
  float sixthW = (availW - ImGui::GetStyle().ItemSpacing.x * 5.0f) / 6.0f;

  if (ImGui::CollapsingHeader(ICON_FA_CUBES " Mesh Management & Containers", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::Button(ICON_FA_CLONE " Duplicate Mesh", ImVec2(halfW, 0))) {
      Geometry::DuplicateMesh(overlay, history);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_OBJECT_UNGROUP " Separate to New Object", ImVec2(halfW, 0))) {
      Geometry::SeparateMeshToNewObject(overlay, history);
    }

    if (ImGui::Button(ICON_FA_OBJECT_GROUP " Merge Selected Meshes", ImVec2(halfW, 0))) {
      Geometry::MergeMeshes(overlay, history);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_VECTOR_SQUARE " Recalculate AABB", ImVec2(halfW, 0))) {
      Geometry::RecalculateBounds(overlay);
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button(ICON_FA_TRASH " Delete Mesh", ImVec2(availW, 0))) {
      Geometry::DeleteMesh(overlay, history);
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Mesh Transforms & Alignment", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("Mirror / Flip Mesh:");
    if (ImGui::Button("Flip X", ImVec2(thirdW, 0))) {
      Geometry::MirrorMesh(overlay, 0, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("Flip Y", ImVec2(thirdW, 0))) {
      Geometry::MirrorMesh(overlay, 1, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("Flip Z", ImVec2(thirdW, 0))) {
      Geometry::MirrorMesh(overlay, 2, history);
    }

    ImGui::Spacing();
    ImGui::Text("Rotate Mesh 90°:");
    if (ImGui::Button("+X", ImVec2(sixthW, 0))) {
      Geometry::RotateMesh(overlay, 0, 90.0f, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("-X", ImVec2(sixthW, 0))) {
      Geometry::RotateMesh(overlay, 0, -90.0f, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("+Y", ImVec2(sixthW, 0))) {
      Geometry::RotateMesh(overlay, 1, 90.0f, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("-Y", ImVec2(sixthW, 0))) {
      Geometry::RotateMesh(overlay, 1, -90.0f, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("+Z", ImVec2(sixthW, 0))) {
      Geometry::RotateMesh(overlay, 2, 90.0f, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("-Z", ImVec2(sixthW, 0))) {
      Geometry::RotateMesh(overlay, 2, -90.0f, history);
    }

    ImGui::Spacing();
    if (ImGui::Button(ICON_FA_ARROW_DOWN " Snap Mesh to Floor", ImVec2(halfW, 0))) {
      Geometry::SnapMeshToFloor(overlay, history);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_CROSSHAIRS " Center Pivot", ImVec2(halfW, 0))) {
      Geometry::CenterMeshPivot(overlay, history);
    }

    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader(ICON_FA_CUBES_STACKED " Add Low-Poly Primitives", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("Primitive Dimensions (meters):");
    ImGui::SetNextItemWidth(thirdW);
    ImGui::InputFloat("##PrimW", &m_primWidth, 0.25f, 1.0f, "W: %.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(thirdW);
    ImGui::InputFloat("##PrimH", &m_primHeight, 0.25f, 1.0f, "H: %.2f");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(thirdW);
    ImGui::InputFloat("##PrimL", &m_primLength, 0.25f, 1.0f, "L: %.2f");

    m_primWidth = std::max(0.05f, m_primWidth);
    m_primHeight = std::max(0.05f, m_primHeight);
    m_primLength = std::max(0.05f, m_primLength);

    ImGui::Spacing();
    if (ImGui::Button(ICON_FA_SQUARE " + Floor Tile (Plane)", ImVec2(halfW, 0))) {
      Geometry::AddPrimitive(overlay, 0, m_primWidth, m_primHeight, m_primLength, history);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_SQUARE " + Wall Tile (Plane)", ImVec2(halfW, 0))) {
      Geometry::AddPrimitive(overlay, 1, m_primWidth, m_primHeight, m_primLength, history);
    }

    if (ImGui::Button(ICON_FA_CUBE " + Cube Block", ImVec2(halfW, 0))) {
      Geometry::AddPrimitive(overlay, 2, m_primWidth, m_primHeight, m_primLength, history);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_CHEVRON_UP " + Ramp / Slope", ImVec2(halfW, 0))) {
      Geometry::AddPrimitive(overlay, 3, m_primWidth, m_primHeight, m_primLength, history);
    }

    ImGui::Spacing();
  }
}

void LocalGeometryPanel::DrawFacesSection(LocalGeometryOverlay &overlay, History *history) {
  float availW = ImGui::GetContentRegionAvail().x;
  float halfW = (availW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
  float thirdW = (availW - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;

  if (ImGui::CollapsingHeader(ICON_FA_SITEMAP " Face Topology & Operations", ImGuiTreeNodeFlags_DefaultOpen)) {
    // Subdivide (Tile 1.0)
    if (ImGui::Button(ICON_FA_BORDER_ALL " Subdivide (Tile 1.0m)", ImVec2(availW, 0))) {
      Geometry::SubdivideSelectedFaces(overlay);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Subdivides selected planar quad faces into 1.0x1.0m PS1 grid tiles while preserving UVs.");
    }

    if (ImGui::Button(ICON_FA_PLAY " Triangulate Quads", ImVec2(halfW, 0))) {
      Geometry::TriangulateFaces(overlay, history);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_LINK " Connect / Bridge", ImVec2(halfW, 0))) {
      Geometry::ConnectBridgeFaces(overlay, history);
    }

    if (ImGui::Button(ICON_FA_UP_RIGHT_FROM_SQUARE " Extrude Faces", ImVec2(halfW, 0))) {
      int rawUnits = 1 << overlay.m_moveStepPower;
      float step = (float)rawUnits / 256.0f;
      Geometry::ExtrudeFaces(overlay, step, m_faceMoveMode, history);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ARROWS_ROTATE " Invert Normals", ImVec2(halfW, 0))) {
      Geometry::InvertNormals(overlay, history);
    }

    ImGui::Spacing();
    ImGui::Text("Extrusion Mode:");
    ImGui::SameLine();
    ImGui::RadioButton("Extrude & Connect", &m_faceMoveMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Separate", &m_faceMoveMode, 1);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button(ICON_FA_TRASH " Delete Faces", ImVec2(halfW, 0))) {
      Geometry::DeleteFaces(overlay, false, history);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_TRASH_CAN " Delete + Isolated Verts", ImVec2(halfW, 0))) {
      Geometry::DeleteFaces(overlay, true, history);
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader(ICON_FA_IMAGE " UV & Texturing", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (overlay.m_texManager && overlay.m_texManager->IsTilePaintModeActive()) {
      const auto &tile = overlay.m_texManager->GetCurrentTile();
      ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.5f, 1.0f), ICON_FA_BRUSH " Tile Paint: %s (Pal %d)", tile.texName.c_str(), tile.palette);
    } else {
      ImGui::TextDisabled("Select tiles in 'Texture Map' to enable rapid paint.");
    }

    if (ImGui::Button(ICON_FA_PAINT_ROLLER " Paint Selected Faces", ImVec2(halfW, 0))) {
      Geometry::PaintFaces(overlay, history);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_BAN " Clear Texture", ImVec2(halfW, 0))) {
      Geometry::ClearTexture(overlay, history);
    }

    ImGui::Spacing();
    ImGui::Text("Rotate UV:");
    if (ImGui::Button("UV 90°", ImVec2(thirdW, 0))) {
      Geometry::RotateUV(overlay, 1, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("UV 180°", ImVec2(thirdW, 0))) {
      Geometry::RotateUV(overlay, 2, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("UV 270°", ImVec2(thirdW, 0))) {
      Geometry::RotateUV(overlay, 3, history);
    }

    ImGui::Spacing();
    ImGui::Text("Mirror UV:");
    if (ImGui::Button("Flip UV (H)", ImVec2(halfW, 0))) {
      Geometry::FlipUV(overlay, true, false, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("Flip UV (V)", ImVec2(halfW, 0))) {
      Geometry::FlipUV(overlay, false, true, history);
    }

    ImGui::Spacing();
    if (ImGui::Button("Fit UV to Tile Bounds", ImVec2(halfW, 0))) {
      Geometry::FitUVToTileBounds(overlay, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Default UV", ImVec2(halfW, 0))) {
      Geometry::ResetDefaultUV(overlay, history);
    }

    ImGui::Spacing();
  }
}

void LocalGeometryPanel::DrawVerticesSection(LocalGeometryOverlay &overlay, History *history) {
  float availW = ImGui::GetContentRegionAvail().x;
  float halfW = (availW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
  float thirdW = (availW - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;

  if (ImGui::CollapsingHeader(ICON_FA_ARROWS_TO_DOT " Vertex Transforms & Snapping", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::Button(ICON_FA_MAGNET " Snap to Grid", ImVec2(halfW, 0))) {
      Geometry::SnapVerticesToGrid(overlay, history);
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_ARROW_DOWN " Snap to Floor (Y=0)", ImVec2(halfW, 0))) {
      Geometry::SnapVerticesToFloor(overlay, history);
    }

    ImGui::Spacing();
    ImGui::Text("Planarize / Flatten:");
    if (ImGui::Button("Flatten X", ImVec2(thirdW, 0))) {
      Geometry::PlanarizeVertices(overlay, 0, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("Flatten Y", ImVec2(thirdW, 0))) {
      Geometry::PlanarizeVertices(overlay, 1, history);
    }
    ImGui::SameLine();
    if (ImGui::Button("Flatten Z", ImVec2(thirdW, 0))) {
      Geometry::PlanarizeVertices(overlay, 2, history);
    }

    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader(ICON_FA_DRAW_POLYGON " Topology from Vertices", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::Button(ICON_FA_PLUS " Add Face from Vertices (3-4)", ImVec2(availW, 0))) {
      Geometry::AddFaceFromSelectedVertices(overlay, history);
    }
    if (ImGui::Button(ICON_FA_UP_RIGHT_FROM_SQUARE " Extrude Edge / Vertices", ImVec2(availW, 0))) {
      int rawUnits = 1 << overlay.m_moveStepPower;
      float step = (float)rawUnits / 256.0f;
      Geometry::ExtrudeSelectedVertices(overlay, { 0.0f, step, 0.0f }, history);
    }

    ImGui::Spacing();
  }

  if (ImGui::CollapsingHeader(ICON_FA_WAND_MAGIC_SPARKLES " Vertex Weld & Cleanup", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::SetNextItemWidth(availW - 120.0f);
    ImGui::SliderFloat("##WeldTol", &m_weldTolerance, 0.001f, 0.5f, "Tol: %.3f m");
    ImGui::SameLine();
    if (ImGui::Button("Weld Vertices", ImVec2(110.0f, 0))) {
      Geometry::WeldVertices(overlay, m_weldTolerance, history);
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button(ICON_FA_TRASH " Delete Selected Vertices", ImVec2(availW, 0))) {
      Geometry::DeleteSelectedVertices(overlay, history);
    }
    ImGui::PopStyleColor();

    ImGui::Spacing();
  }
}

void LocalGeometryPanel::DrawValidationSection(LocalGeometryOverlay &overlay) {
  if (ImGui::CollapsingHeader(ICON_FA_SHIELD_HALVED " PS1 Limits & Validation", ImGuiTreeNodeFlags_DefaultOpen)) {
    float availW = ImGui::GetContentRegionAvail().x;
    float halfW = (availW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    ImGui::Checkbox("Auto-Validate Edits", &overlay.m_autoValidate);
    ImGui::SameLine();
    if (ImGui::Button("Validate Chunk", ImVec2(halfW, 0))) {
      if (!overlay.GetChunks().empty()) {
        m_hasRunValidation = false;
        for (const auto &lc : overlay.GetChunks()) {
          if (lc.data->chunkName == overlay.m_selectedChunk) {
            m_lastValidationResult = ChunkValidator::ValidateChunk(*lc.data);
            m_hasRunValidation = true;
            break;
          }
        }
        if (!m_hasRunValidation && !overlay.GetChunks().empty()) {
          m_lastValidationResult = ChunkValidator::ValidateChunk(
              *overlay.GetChunks()[0].data);
          m_hasRunValidation = true;
        }
      }
    }

    if (m_hasRunValidation) {
      if (m_lastValidationResult.IsClean()) {
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
                           "[OK] No height, overhang, or vertex capacity issues.");
      } else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Errors: %d",
                           m_lastValidationResult.errorCount);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f), "Warnings: %d",
                           m_lastValidationResult.warningCount);

        ImGui::BeginChild("ValidationIssuesList", ImVec2(0, 120), true);
        for (size_t ii = 0; ii < m_lastValidationResult.issues.size(); ++ii) {
          const auto &issue = m_lastValidationResult.issues[ii];
          ImVec4 color = (issue.severity == ValidationSeverity::Error)
                             ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
                             : ImVec4(1.0f, 0.7f, 0.2f, 1.0f);

          ImGui::PushID((int)ii);
          ImGui::PushStyleColor(ImGuiCol_Text, color);
          if (ImGui::Selectable(issue.message.c_str(), false)) {
            overlay.m_selectedChunk = issue.chunkName;
            if (issue.objectIdx >= 0)
              overlay.m_selectedObjectIdx = issue.objectIdx;
            if (issue.meshIdx >= 0)
              overlay.m_selectedMeshIdx = issue.meshIdx;
            if (issue.vertexIdx >= 0) {
              overlay.m_editMode = EditMode::Vertex;
              overlay.m_selectedVertices.clear();
              overlay.m_selectedVertices.insert(
                  {issue.chunkName, issue.objectIdx, issue.meshIdx,
                   issue.vertexIdx});
            }
          }
          ImGui::PopStyleColor();
          ImGui::PopID();
        }
        ImGui::EndChild();
      }
    } else {
      ImGui::TextDisabled("Click 'Validate Chunk' to check height (16.0f), "
                          "cell overhang (+7 raw), and vertex limits.");
    }
  }
}
