#include "panels/OutlinerPanel.h"
#include "imgui.h"
#include "viewport/Viewport.h"
#include "viewport/LocalGeometry.h"
#include "extras/IconsFontAwesome6.h"
#include <cstdio>

void OutlinerPanel::Draw(Viewport &viewport,
                         LocalGeometryOverlay *localGeometryOverlay) {
  ImGui::Begin(ICON_FA_LIST_UL " Outliner");

  auto &chunks = viewport.GetChunks();

  if (chunks.empty()) {
    ImGui::TextDisabled("No chunks loaded.");
    ImGui::TextDisabled("Use the Chunk Manager to load a chunk.");
    ImGui::End();
    return;
  }

  for (int ci = 0; ci < (int)chunks.size(); ++ci) {
    auto &lc = chunks[ci];
    const auto &chunk = *lc.data;

    // Eye icon toggle
    ImGui::PushID(ci);
    bool vis = lc.visible;
    if (ImGui::Checkbox("##vis", &vis)) {
      lc.visible = vis;
    }
    ImGui::SameLine();
    ImGui::PopID();

    // Chunk root node
    bool chunkOpen =
        ImGui::TreeNodeEx(chunk.chunkName.c_str(),
                          ImGuiTreeNodeFlags_DefaultOpen |
                              (lc.hasError ? ImGuiTreeNodeFlags_Leaf : 0));
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("Expand All / Collapse All")) {}
      if (ImGui::MenuItem("View Properties")) {}
      if (ImGui::MenuItem("Focus Camera on Chunk")) {}
      if (ImGui::MenuItem("Unload from Workspace")) {}
      ImGui::EndPopup();
    }

    // Error indicator
    if (lc.hasError) {
      ImGui::SameLine();
      ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "  [ERROR]");
    } else {
      // Stats on same line
      ImGui::SameLine();
      ImGui::TextDisabled("  %zu obj  %zu batch", chunk.objects.size(),
                          lc.batches.size());
    }

    if (chunkOpen) {
      if (lc.hasError) {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%s", lc.errorMsg.c_str());
      } else {
        // Chunk position info
        ImGui::TextDisabled("Map pos: x=%d  y=%d", (int)chunk.xPos,
                            (int)chunk.yPos);
        ImGui::TextDisabled("Local textures:  %zu", chunk.localTexNames.size());
        ImGui::TextDisabled("Global textures: %zu",
                            chunk.globalTexNames.size());

        // Local geometry
        if (ImGui::TreeNode("Local Geometry")) {
          int localCount = 0;
          for (size_t oi = 0; oi < chunk.objects.size(); ++oi) {
            const auto &obj = chunk.objects[oi];
            if (!obj.isGlobal) {
              bool isSelectedObj =
                  (viewport.m_selectedChunk == chunk.chunkName &&
                   viewport.m_selectedObjectIdx == (int)oi);

              int maxVertsPerMesh = 0;
              int totalVerts = 0;
              for (const auto &m : obj.meshes) {
                totalVerts += (int)m.vx.size();
                if ((int)m.vx.size() > maxVertsPerMesh)
                  maxVertsPerMesh = (int)m.vx.size();
              }

              ImGuiTreeNodeFlags objFlags =
                  ImGuiTreeNodeFlags_OpenOnArrow |
                  ImGuiTreeNodeFlags_OpenOnDoubleClick;
              if (isSelectedObj &&
                  (localGeometryOverlay
                       ? localGeometryOverlay->m_selectedMeshIdx == -1
                       : true)) {
                objFlags |= ImGuiTreeNodeFlags_Selected;
              }
              if (obj.meshes.empty()) {
                objFlags |= ImGuiTreeNodeFlags_Leaf |
                            ImGuiTreeNodeFlags_NoTreePushOnOpen;
              }

              char objBuf[128];
              snprintf(objBuf, sizeof(objBuf),
                       "%s (Meshes: %zu, Tot: %d, Max/Mesh: %d/255)",
                       obj.name.c_str(), obj.meshes.size(), totalVerts,
                       maxVertsPerMesh);

              ImGui::PushID((int)oi);
              bool objOpen = ImGui::TreeNodeEx((void *)(intptr_t)oi, objFlags,
                                               "%s", objBuf);
              if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Select Object")) {}
                if (ImGui::MenuItem("Focus Camera on Object")) {}
                if (ImGui::MenuItem("Duplicate Object")) {}
                if (ImGui::MenuItem("Move Object to Chunk...")) {}
                if (ImGui::MenuItem("Delete Object")) {}
                ImGui::EndPopup();
              }

              if (maxVertsPerMesh >= 255) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                                   "[MAX LIMIT]");
              } else if (maxVertsPerMesh >= 240) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f),
                                   "[NEAR LIMIT]");
              }

              if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                viewport.m_selectedChunk = chunk.chunkName;
                viewport.m_selectedObjectIdx = (int)oi;
                if (localGeometryOverlay) {
                  localGeometryOverlay->m_selectedChunk = chunk.chunkName;
                  localGeometryOverlay->m_selectedObjectIdx = (int)oi;
                  localGeometryOverlay->m_selectedMeshIdx = -1;
                  localGeometryOverlay->m_selectedFaceIdx = -1;
                  localGeometryOverlay->m_selectedVertices.clear();
                  localGeometryOverlay->m_selectedFaces.clear();
                  for (size_t mi = 0; mi < obj.meshes.size(); ++mi) {
                    const auto &mesh = obj.meshes[mi];
                    for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
                      localGeometryOverlay->m_selectedFaces.insert(
                          {chunk.chunkName, (int)oi, (int)mi, (int)fi});
                    }
                  }
                }
              }

              if (objOpen && !obj.meshes.empty()) {
                for (size_t mi = 0; mi < obj.meshes.size(); ++mi) {
                  const auto &mesh = obj.meshes[mi];
                  bool isSelectedMesh =
                      isSelectedObj &&
                      (localGeometryOverlay &&
                       localGeometryOverlay->m_selectedMeshIdx == (int)mi);

                  ImGuiTreeNodeFlags meshFlags =
                      ImGuiTreeNodeFlags_Leaf |
                      ImGuiTreeNodeFlags_NoTreePushOnOpen;
                  if (isSelectedMesh)
                    meshFlags |= ImGuiTreeNodeFlags_Selected;

                  char meshBuf[128];
                  snprintf(meshBuf, sizeof(meshBuf),
                           "Mesh %zu: %zu / 255 verts (%zu faces)", mi,
                           mesh.vx.size(), mesh.faces.size());

                  ImGui::PushID((int)mi);
                  ImGui::TreeNodeEx((void *)(intptr_t)mi, meshFlags, "%s",
                                    meshBuf);
                  if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Select Mesh")) {}
                    if (ImGui::MenuItem("Duplicate Mesh")) {}
                    if (ImGui::MenuItem("Extract as New Object")) {}
                    if (ImGui::MenuItem("Delete Mesh")) {}
                    ImGui::EndPopup();
                  }

                  if (mesh.vx.size() >= 255) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                                       "[LIMIT 255]");
                  } else if (mesh.vx.size() >= 240) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f),
                                       "[NEAR 255]");
                  }

                  if (ImGui::IsItemClicked()) {
                    viewport.m_selectedChunk = chunk.chunkName;
                    viewport.m_selectedObjectIdx = (int)oi;
                    if (localGeometryOverlay) {
                      localGeometryOverlay->m_selectedChunk = chunk.chunkName;
                      localGeometryOverlay->m_selectedObjectIdx = (int)oi;
                      localGeometryOverlay->m_selectedMeshIdx = (int)mi;
                      localGeometryOverlay->m_selectedFaceIdx = -1;
                      localGeometryOverlay->m_selectedVertices.clear();
                      localGeometryOverlay->m_selectedFaces.clear();
                      for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
                        localGeometryOverlay->m_selectedFaces.insert(
                            {chunk.chunkName, (int)oi, (int)mi, (int)fi});
                      }
                    }
                  }
                  ImGui::PopID();
                }
                ImGui::TreePop();
              }
              ImGui::PopID();
              localCount++;
            }
          }
          if (localCount == 0)
            ImGui::TextDisabled("None");
          ImGui::TreePop();
        }

        // Global geometry
        if (ImGui::TreeNode("Global Geometry")) {
          int globalCount = 0;
          for (size_t oi = 0; oi < chunk.objects.size(); ++oi) {
            const auto &obj = chunk.objects[oi];
            if (obj.isGlobal) {
              bool isSelectedObj =
                  (viewport.m_selectedChunk == chunk.chunkName &&
                   viewport.m_selectedObjectIdx == (int)oi);

              int maxVertsPerMesh = 0;
              int totalVerts = 0;
              for (const auto &m : obj.meshes) {
                totalVerts += (int)m.vx.size();
                if ((int)m.vx.size() > maxVertsPerMesh)
                  maxVertsPerMesh = (int)m.vx.size();
              }

              ImGuiTreeNodeFlags objFlags =
                  ImGuiTreeNodeFlags_OpenOnArrow |
                  ImGuiTreeNodeFlags_OpenOnDoubleClick;
              if (isSelectedObj &&
                  (localGeometryOverlay
                       ? localGeometryOverlay->m_selectedMeshIdx == -1
                       : true)) {
                objFlags |= ImGuiTreeNodeFlags_Selected;
              }
              if (obj.meshes.empty()) {
                objFlags |= ImGuiTreeNodeFlags_Leaf |
                            ImGuiTreeNodeFlags_NoTreePushOnOpen;
              }

              char objBuf[128];
              snprintf(objBuf, sizeof(objBuf),
                       "%s (Meshes: %zu, Tot: %d, Max/Mesh: %d/255)",
                       obj.name.c_str(), obj.meshes.size(), totalVerts,
                       maxVertsPerMesh);

              ImGui::PushID((int)oi);
              bool objOpen = ImGui::TreeNodeEx((void *)(intptr_t)oi, objFlags,
                                               "%s", objBuf);
              if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Select Object")) {}
                if (ImGui::MenuItem("Focus Camera on Object")) {}
                if (ImGui::MenuItem("Duplicate Object")) {}
                if (ImGui::MenuItem("Move Object to Chunk...")) {}
                if (ImGui::MenuItem("Delete Object")) {}
                ImGui::EndPopup();
              }

              if (maxVertsPerMesh >= 255) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                                   "[MAX LIMIT]");
              } else if (maxVertsPerMesh >= 240) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f),
                                   "[NEAR LIMIT]");
              }

              if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                viewport.m_selectedChunk = chunk.chunkName;
                viewport.m_selectedObjectIdx = (int)oi;
                if (localGeometryOverlay) {
                  localGeometryOverlay->m_selectedChunk = chunk.chunkName;
                  localGeometryOverlay->m_selectedObjectIdx = (int)oi;
                  localGeometryOverlay->m_selectedMeshIdx = -1;
                  localGeometryOverlay->m_selectedFaceIdx = -1;
                  localGeometryOverlay->m_selectedVertices.clear();
                  localGeometryOverlay->m_selectedFaces.clear();
                  for (size_t mi = 0; mi < obj.meshes.size(); ++mi) {
                    const auto &mesh = obj.meshes[mi];
                    for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
                      localGeometryOverlay->m_selectedFaces.insert(
                          {chunk.chunkName, (int)oi, (int)mi, (int)fi});
                    }
                  }
                }
              }

              if (objOpen && !obj.meshes.empty()) {
                for (size_t mi = 0; mi < obj.meshes.size(); ++mi) {
                  const auto &mesh = obj.meshes[mi];
                  bool isSelectedMesh =
                      isSelectedObj &&
                      (localGeometryOverlay &&
                       localGeometryOverlay->m_selectedMeshIdx == (int)mi);

                  ImGuiTreeNodeFlags meshFlags =
                      ImGuiTreeNodeFlags_Leaf |
                      ImGuiTreeNodeFlags_NoTreePushOnOpen;
                  if (isSelectedMesh)
                    meshFlags |= ImGuiTreeNodeFlags_Selected;

                  char meshBuf[128];
                  snprintf(meshBuf, sizeof(meshBuf),
                           "Mesh %zu: %zu / 255 verts (%zu faces)", mi,
                           mesh.vx.size(), mesh.faces.size());

                  ImGui::PushID((int)mi);
                  ImGui::TreeNodeEx((void *)(intptr_t)mi, meshFlags, "%s",
                                    meshBuf);
                  if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Select Mesh")) {}
                    if (ImGui::MenuItem("Duplicate Mesh")) {}
                    if (ImGui::MenuItem("Extract as New Object")) {}
                    if (ImGui::MenuItem("Delete Mesh")) {}
                    ImGui::EndPopup();
                  }

                  if (mesh.vx.size() >= 255) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f),
                                       "[LIMIT 255]");
                  } else if (mesh.vx.size() >= 240) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f),
                                       "[NEAR 255]");
                  }

                  if (ImGui::IsItemClicked()) {
                    viewport.m_selectedChunk = chunk.chunkName;
                    viewport.m_selectedObjectIdx = (int)oi;
                    if (localGeometryOverlay) {
                      localGeometryOverlay->m_selectedChunk = chunk.chunkName;
                      localGeometryOverlay->m_selectedObjectIdx = (int)oi;
                      localGeometryOverlay->m_selectedMeshIdx = (int)mi;
                      localGeometryOverlay->m_selectedFaceIdx = -1;
                      localGeometryOverlay->m_selectedVertices.clear();
                      localGeometryOverlay->m_selectedFaces.clear();
                      for (size_t fi = 0; fi < mesh.faces.size(); ++fi) {
                        localGeometryOverlay->m_selectedFaces.insert(
                            {chunk.chunkName, (int)oi, (int)mi, (int)fi});
                      }
                    }
                  }
                  ImGui::PopID();
                }
                ImGui::TreePop();
              }
              ImGui::PopID();
              globalCount++;
            }
          }
          if (globalCount == 0)
            ImGui::TextDisabled("None");
          ImGui::TreePop();
        }

        // GPU batches summary
        if (!lc.batches.empty()) {
          if (ImGui::TreeNode("Draw Batches")) {
            for (int bi = 0; bi < (int)lc.batches.size(); ++bi) {
              const auto &b = lc.batches[bi];
              char label[64];
              if (b.texName.empty()) {
                snprintf(label, sizeof(label), "[%d] NO-TEX  P%d  (%d verts)",
                         bi, b.paletteRow, b.mesh.vertexCount);
              } else {
                snprintf(label, sizeof(label), "[%d] %s  P%d  (%d verts)", bi,
                         b.texName.c_str(), b.paletteRow, b.mesh.vertexCount);
              }
              ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_Leaf |
                                           ImGuiTreeNodeFlags_NoTreePushOnOpen);
              if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Highlight Batch")) {}
                ImGui::EndPopup();
              }
            }
            ImGui::TreePop();
          }
        }
      }

      ImGui::TreePop();
    }
  }

  ImGui::End();
}
