#include "panels/TextureWidgets.h"
#include "core/Config.h"
#include "core/DependencyManager.h"
#include "core/FileDialog.h"
#include "core/ResourceFilter.h"
#include "extras/IconsFontAwesome6.h"
#include "imgui_internal.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

void TextureSelectorWidget::RefreshAvailable(
    FileManager &fileManager, DependencyManager &dependencyManager,
    const std::string &currentSelChunk, int currentSelObj, int currentSelMesh,
    const std::vector<const ParsedChunk *> &chunks) {
  std::string currentWorkspaceDir = fileManager.GetWorkspaceDir();
  std::string currentAssetsDir = fileManager.GetAssetsDir();
  std::string currentPrefix = fileManager.GetSelectedPrefix();
  std::vector<std::string> currentSelChunks = fileManager.GetSelectedChunks();

  bool needRefresh = (m_filterScope != lastFilterScope) ||
                     (currentWorkspaceDir != lastWorkspaceDirForTex) ||
                     (currentAssetsDir != lastAssetsDirForTex) ||
                     (currentPrefix != lastSelectedPrefixForTex) ||
                     (currentSelChunks != lastSelectedChunksForTex) ||
                     (currentSelChunk != lastSelChunk) ||
                     (currentSelObj != lastSelObj) ||
                     (currentSelMesh != lastSelMesh) ||
                     (ImGui::GetTime() - lastTexRefreshTime > 1.0);

  if (needRefresh) {
    lastFilterScope = m_filterScope;
    lastWorkspaceDirForTex = currentWorkspaceDir;
    lastAssetsDirForTex = currentAssetsDir;
    lastSelectedPrefixForTex = currentPrefix;
    lastSelectedChunksForTex = currentSelChunks;
    lastSelChunk = currentSelChunk;
    lastSelObj = currentSelObj;
    lastSelMesh = currentSelMesh;
    lastTexRefreshTime = ImGui::GetTime();

    cachedTextures.clear();

    switch (m_filterScope) {
    case TextureFilterScope::Assets:
      cachedTextures =
          ResourceFilter::GetAssetTextures(fileManager, currentPrefix);
      break;
    case TextureFilterScope::Workspace:
      cachedTextures =
          ResourceFilter::GetWorkspaceTextures(fileManager, currentPrefix);
      break;
    case TextureFilterScope::SelectedChunks:
      cachedTextures = ResourceFilter::GetSelectedChunksTextures(
          fileManager, dependencyManager, currentPrefix);
      break;
    case TextureFilterScope::CurrentChunk: {
      const ParsedChunk *chunkData = nullptr;
      for (const auto *cd : chunks) {
        if (cd && cd->chunkName == currentSelChunk) {
          chunkData = cd;
          break;
        }
      }
      cachedTextures =
          ResourceFilter::GetChunkTextures(chunkData, currentPrefix);
      break;
    }
    case TextureFilterScope::CurrentMesh: {
      const ParsedChunk *chunkData = nullptr;
      for (const auto *cd : chunks) {
        if (cd && cd->chunkName == currentSelChunk) {
          chunkData = cd;
          break;
        }
      }
      const RenderMesh *mesh = nullptr;
      const RenderObject *obj = nullptr;
      if (chunkData && currentSelObj >= 0 &&
          currentSelObj < (int)chunkData->objects.size()) {
        obj = &chunkData->objects[currentSelObj];
        if (currentSelMesh >= 0 && currentSelMesh < (int)obj->meshes.size()) {
          mesh = &obj->meshes[currentSelMesh];
        }
      }
      cachedTextures =
          ResourceFilter::GetMeshTextures(mesh, chunkData, obj, currentPrefix);
      break;
    }
    default:
      break;
    }
  }
}

bool TextureSelectorWidget::DrawCombo(
    FileManager &fileManager, const std::string &currentTexPath,
    const std::function<void(const std::string &path, const std::string &texName)>
        &onSelect) {
  bool selected = false;
  const char *filterScopeNames[] = {
      "Assets", "Workspace", "Selected Chunks", "Current Chunk", "Current Mesh"};

  float filterBtnWidth = ImGui::GetFrameHeight();
  ImGui::SetNextItemWidth(filterBtnWidth);
  if (ImGui::BeginCombo("##TexFilterScope", ICON_FA_FILTER,
                        ImGuiComboFlags_NoArrowButton)) {
    for (int i = 0; i < static_cast<int>(TextureFilterScope::Count); ++i) {
      bool isSelected = (m_filterScope == static_cast<TextureFilterScope>(i));
      if (ImGui::Selectable(filterScopeNames[i], isSelected)) {
        m_filterScope = static_cast<TextureFilterScope>(i);
      }
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Filter Textures: %s",
                      filterScopeNames[static_cast<int>(m_filterScope)]);
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
  std::string currentTexName = "Select a face or texture...";
  if (!currentTexPath.empty()) {
    currentTexName =
        std::filesystem::path(currentTexPath).stem().string();
  }

  if (cachedTextures.empty()) {
    std::string placeholder;
    std::string currentPrefix = fileManager.GetSelectedPrefix();
    switch (m_filterScope) {
    case TextureFilterScope::Assets:
      placeholder = currentPrefix.empty()
                        ? "No textures found in assets"
                        : "No " + currentPrefix + " textures in assets";
      break;
    case TextureFilterScope::Workspace:
      placeholder = currentPrefix.empty()
                        ? "No textures found in workspace"
                        : "No " + currentPrefix + " textures in workspace";
      break;
    case TextureFilterScope::SelectedChunks:
      placeholder = "No textures in selected chunks";
      break;
    case TextureFilterScope::CurrentChunk:
      placeholder = lastSelChunk.empty()
                        ? "No chunk selected in viewport"
                        : "No textures in chunk " + lastSelChunk;
      break;
    case TextureFilterScope::CurrentMesh:
      placeholder = (lastSelMesh < 0)
                        ? "No mesh selected in viewport"
                        : "No textures in current mesh";
      break;
    default:
      placeholder = "No textures found";
      break;
    }
    if (ImGui::BeginCombo("##TIM_Sel", placeholder.c_str())) {
      ImGui::EndCombo();
    }
  } else {
    if (ImGui::BeginCombo("##TIM_Sel", currentTexName.c_str())) {
      for (const auto &tex : cachedTextures) {
        if (ImGui::Selectable(tex.c_str())) {
          std::string path =
              ResourceFilter::ResolveTexturePath(fileManager, tex);
          if (onSelect) {
            onSelect(path, tex);
          }
          selected = true;
        }
      }
      ImGui::EndCombo();
    }
  }

  return selected;
}

bool TextureSelectorWidget::DrawFromFile(
    const std::function<void(const std::string &path, const std::string &texName)>
        &onSelect) {
  float labelWidth = 110.0f;
  float browseWidth = 80.0f;
  bool loaded = false;

  ImGui::AlignTextToFramePadding();
  ImGui::Text("From file:");
  ImGui::SameLine(labelWidth);

  char timPathBuf[256];
  strncpy(timPathBuf, Config::Get().LastTexturePath.c_str(), sizeof(timPathBuf));
  timPathBuf[sizeof(timPathBuf) - 1] = '\0';

  auto applyPath = [&](const std::string &path) {
    if (path.empty()) return;
    std::string stem = std::filesystem::path(path).stem().string();
    if (onSelect) {
      onSelect(path, stem);
    }
    loaded = true;
  };

  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - browseWidth -
                          ImGui::GetStyle().ItemSpacing.x);
  if (ImGui::InputText("##TIM_Path", timPathBuf, sizeof(timPathBuf),
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    applyPath(timPathBuf);
  }
  ImGui::SameLine();
  if (ImGui::Button("Browse...", ImVec2(browseWidth, 0))) {
    std::string path =
        FileDialog::OpenFile("TIM Files\0*.TIM;*.tim\0All Files\0*.*\0");
    if (!path.empty()) {
      applyPath(path);
    }
  }

  return loaded;
}

bool PaletteInspectorWidget::Draw(
    Textures &activeTexture, int &currentPalette,
    const std::function<void(int newPalette)> &onPaletteChanged,
    const std::function<void(int colorIdx, TIMColor color)> &onColorSelected,
    bool showDimensions, PaletteWidgetLayout layout, const char *label) {
  bool changed = false;

  if (showDimensions) {
    ImGui::Text("Size: %d x %d", activeTexture.GetWidth(),
                activeTexture.GetHeight());
  }

  int numPalettes = (int)activeTexture.GetPalettes().size();
  if (numPalettes >= 1) {
    auto applyNewPalette = [&](int newPal) {
      if (numPalettes <= 0) return;
      newPal = (newPal % numPalettes + numPalettes) % numPalettes;
      if (newPal == currentPalette) return;
      currentPalette = newPal;
      activeTexture.ApplyPalette(currentPalette);
      if (onPaletteChanged) {
        onPaletteChanged(currentPalette);
      }
      changed = true;
    };

    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", label ? label : "Palette:");
    ImGui::SameLine();

    float btnW = (layout == PaletteWidgetLayout::Inline) ? 20.0f : 22.0f;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float comboW = (layout == PaletteWidgetLayout::Inline)
                       ? 65.0f
                       : (ImGui::GetContentRegionAvail().x -
                          (btnW * 2.0f + spacing * 2.0f));
    if (comboW < 50.0f) comboW = 50.0f;

    bool canStep = (numPalettes > 1);
    if (!canStep) ImGui::BeginDisabled();
    if (ImGui::ArrowButton("##PrevPal", ImGuiDir_Left)) {
      applyNewPalette(currentPalette - 1);
    }
    if (!canStep) ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::SetNextItemWidth(comboW);
    std::string comboPreview = std::to_string(currentPalette);
    if (!canStep) ImGui::BeginDisabled();
    if (ImGui::BeginCombo("##PaletteCombo", comboPreview.c_str(),
                          ImGuiComboFlags_HeightLarge)) {
      for (int i = 0; i < numPalettes; ++i) {
        ImGui::PushID(i);
        bool isSelected = (currentPalette == i);
        ImVec2 itemPos = ImGui::GetCursorScreenPos();
        float itemH = 18.0f;
        float itemW = ImGui::GetContentRegionAvail().x;

        if (ImGui::Selectable("##PalItem", isSelected, 0, ImVec2(0, itemH))) {
          applyNewPalette(i);
        }
        if (isSelected) {
          ImGui::SetItemDefaultFocus();
        }

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        std::string numLabel = std::to_string(i);
        float labelW = 24.0f;
        drawList->AddText(
            ImVec2(itemPos.x + 4.0f, itemPos.y + 1.0f),
            isSelected ? IM_COL32(255, 255, 255, 255)
                       : IM_COL32(200, 200, 200, 255),
            numLabel.c_str());

        const auto &rowPal = activeTexture.GetPalettes()[i];
        if (!rowPal.colors.empty()) {
          float barX = itemPos.x + labelW;
          float barW = itemW - labelW - 4.0f;
          float barH = itemH - 4.0f;
          float barY = itemPos.y + 2.0f;
          int numCols = (int)rowPal.colors.size();
          float sW = barW / (float)numCols;

          drawList->AddRectFilled(ImVec2(barX, barY),
                                  ImVec2(barX + barW, barY + barH),
                                  IM_COL32(20, 20, 20, 255));
          for (int c = 0; c < numCols; ++c) {
            const auto &col = rowPal.colors[c];
            ImVec2 cp0(barX + c * sW, barY);
            ImVec2 cp1(barX + (c + 1) * sW, barY + barH);
            if (col.a == 0 && col.r == 0 && col.g == 0 && col.b == 0) {
              drawList->AddRectFilled(cp0, cp1, IM_COL32(25, 25, 30, 255));
              drawList->AddLine(cp0, cp1, IM_COL32(70, 70, 80, 255));
            } else {
              drawList->AddRectFilled(cp0, cp1,
                                      IM_COL32(col.r, col.g, col.b, 255));
            }
          }
          drawList->AddRect(ImVec2(barX, barY),
                            ImVec2(barX + barW, barY + barH),
                            IM_COL32(60, 60, 60, 255));
        }

        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
    if (!canStep) ImGui::EndDisabled();

    ImGui::SameLine();

    if (!canStep) ImGui::BeginDisabled();
    if (ImGui::ArrowButton("##NextPal", ImGuiDir_Right)) {
      applyNewPalette(currentPalette + 1);
    }
    if (!canStep) ImGui::EndDisabled();

    if (layout == PaletteWidgetLayout::Inline) {
      ImGui::SameLine();
    }
  }

  if (!activeTexture.GetPalettes().empty()) {
    int palIdx = std::clamp(currentPalette, 0,
                            (int)activeTexture.GetPalettes().size() - 1);
    const auto &pal = activeTexture.GetPalettes()[palIdx];
    if (!pal.colors.empty()) {
      float availWidth = ImGui::GetContentRegionAvail().x;
      float barHeight = (layout == PaletteWidgetLayout::Inline)
                            ? ImGui::GetFrameHeight()
                            : 16.0f;
      ImVec2 barPos = ImGui::GetCursorScreenPos();
      ImDrawList *drawList = ImGui::GetWindowDrawList();

      ImGui::InvisibleButton("##PaletteColorPreview",
                             ImVec2(availWidth, barHeight));
      bool isHovered = ImGui::IsItemHovered();
      ImVec2 mousePos = ImGui::GetMousePos();

      int numColors = (int)pal.colors.size();
      float swatchW = availWidth / (float)numColors;

      int hoveredColorIdx = -1;
      if (isHovered && mousePos.x >= barPos.x &&
          mousePos.x < barPos.x + availWidth) {
        hoveredColorIdx = std::clamp(
            (int)((mousePos.x - barPos.x) / swatchW), 0, numColors - 1);
      }

      drawList->AddRectFilled(barPos,
                              ImVec2(barPos.x + availWidth, barPos.y + barHeight),
                              IM_COL32(20, 20, 20, 255));

      for (int i = 0; i < numColors; ++i) {
        const auto &c = pal.colors[i];
        ImVec2 p0(barPos.x + i * swatchW, barPos.y);
        ImVec2 p1(barPos.x + (i + 1) * swatchW, barPos.y + barHeight);

        if (c.a == 0 && c.r == 0 && c.g == 0 && c.b == 0) {
          drawList->AddRectFilled(p0, p1, IM_COL32(25, 25, 30, 255));
          drawList->AddLine(p0, p1, IM_COL32(70, 70, 80, 255));
        } else {
          drawList->AddRectFilled(p0, p1, IM_COL32(c.r, c.g, c.b, 255));
        }
      }

      drawList->AddRect(barPos,
                        ImVec2(barPos.x + availWidth, barPos.y + barHeight),
                        IM_COL32(80, 80, 80, 255));

      if (hoveredColorIdx >= 0 && hoveredColorIdx < numColors) {
        const auto &hc = pal.colors[hoveredColorIdx];
        ImVec2 hp0(barPos.x + hoveredColorIdx * swatchW, barPos.y);
        ImVec2 hp1(barPos.x + (hoveredColorIdx + 1) * swatchW,
                   barPos.y + barHeight);
        drawList->AddRect(hp0, hp1, IM_COL32(255, 255, 255, 255), 0.0f, 0,
                          2.0f);

        if (isHovered && ImGui::IsMouseClicked(0) && onColorSelected) {
          onColorSelected(hoveredColorIdx, hc);
        }

        ImGui::BeginTooltip();
        ImGui::Text("Color #%d / %d", hoveredColorIdx, numColors);
        ImGui::Text("RGB: (%d, %d, %d)", hc.r, hc.g, hc.b);
        ImGui::Text("Hex: #%02X%02X%02X", hc.r, hc.g, hc.b);
        if (hc.a == 0)
          ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                             "(Transparent STP)");
        ImGui::EndTooltip();
      }
    }
  }

  return changed;
}

// -----------------------------------------------------------------------------
// TextureCanvasWidget Implementation
// -----------------------------------------------------------------------------

void TextureCanvasWidget::DrawCheckeredBackground(ImDrawList *drawList,
                                                  ImVec2 pos, ImVec2 size,
                                                  float checkSize) {
  if (!drawList || size.x <= 0.0f || size.y <= 0.0f || checkSize <= 0.0f)
    return;

  for (float y = pos.y; y < pos.y + size.y; y += checkSize) {
    for (float x = pos.x; x < pos.x + size.x; x += checkSize) {
      int ix = (int)((x - pos.x) / checkSize);
      int iy = (int)((y - pos.y) / checkSize);
      ImU32 col = ((ix + iy) % 2 == 0) ? IM_COL32(35, 35, 40, 255)
                                       : IM_COL32(25, 25, 30, 255);
      drawList->AddRectFilled(
          ImVec2(x, y),
          ImVec2(std::min(x + checkSize, pos.x + size.x),
                 std::min(y + checkSize, pos.y + size.y)),
          col);
    }
  }
}

bool TextureCanvasWidget::ScreenToPixelCoords(ImVec2 mousePos, ImVec2 imgPos,
                                              float scale, int texWidth,
                                              int texHeight, int &outPx,
                                              int &outPy) {
  if (scale <= 0.0f || texWidth <= 0 || texHeight <= 0)
    return false;

  float scaledW = (float)texWidth * scale;
  float scaledH = (float)texHeight * scale;

  if (mousePos.x < imgPos.x || mousePos.x >= imgPos.x + scaledW ||
      mousePos.y < imgPos.y || mousePos.y >= imgPos.y + scaledH) {
    return false;
  }

  outPx = std::clamp((int)((mousePos.x - imgPos.x) / scale), 0, texWidth - 1);
  outPy = std::clamp((int)((mousePos.y - imgPos.y) / scale), 0, texHeight - 1);
  return true;
}

void TextureCanvasWidget::PixelToTileCoords(int px, int py, int &outTileX,
                                           int &outTileY, int &outTileIdx,
                                           int texWidth, int tileSize) {
  if (tileSize <= 0) tileSize = 32;
  outTileX = px / tileSize;
  outTileY = py / tileSize;
  int tilesX = std::max(1, texWidth / tileSize);
  outTileIdx = outTileY * tilesX + outTileX;
}

float TextureCanvasWidget::SnapCoord32(float val) {
  return std::floor(val / 32.0f) * 32.0f;
}

// -----------------------------------------------------------------------------
// TextureGridWidget Implementation
// -----------------------------------------------------------------------------

void TextureGridWidget::DrawPixelGrid(ImDrawList *drawList, ImVec2 p0,
                                      ImVec2 p1, int texW, int texH,
                                      float zoom, bool skipTileLines,
                                      ImU32 color) {
  if (!drawList || zoom < 4.0f || texW <= 0 || texH <= 0)
    return;

  for (int x = 0; x <= texW; ++x) {
    if (skipTileLines && (x % 32 == 0))
      continue;
    float lx = p0.x + (float)x * zoom;
    drawList->AddLine(ImVec2(lx, p0.y), ImVec2(lx, p1.y), color);
  }
  for (int y = 0; y <= texH; ++y) {
    if (skipTileLines && (y % 32 == 0))
      continue;
    float ly = p0.y + (float)y * zoom;
    drawList->AddLine(ImVec2(p0.x, ly), ImVec2(p1.x, ly), color);
  }
}

void TextureGridWidget::DrawTileGrid(ImDrawList *drawList, ImVec2 p0, ImVec2 p1,
                                     int texW, int texH, float zoom,
                                     int tileSize, ImU32 color,
                                     float thickness) {
  if (!drawList || texW <= 0 || texH <= 0 || tileSize <= 0)
    return;

  for (int x = tileSize; x < texW; x += tileSize) {
    float lx = p0.x + (float)x * zoom;
    drawList->AddLine(ImVec2(lx, p0.y), ImVec2(lx, p1.y), color, thickness);
  }
  for (int y = tileSize; y < texH; y += tileSize) {
    float ly = p0.y + (float)y * zoom;
    drawList->AddLine(ImVec2(p0.x, ly), ImVec2(p1.x, ly), color, thickness);
  }
}

// -----------------------------------------------------------------------------
// TextureInspectorWidget Implementation
// -----------------------------------------------------------------------------

void TextureInspectorWidget::DrawPixelTileTooltip(int px, int py, int texWidth,
                                                  int tileSize) {
  ImGui::BeginTooltip();
  ImGui::Text("Pixel: (%d, %d)", px, py);
  int tileX = 0, tileY = 0, tileIdx = 0;
  TextureCanvasWidget::PixelToTileCoords(px, py, tileX, tileY, tileIdx,
                                        texWidth, tileSize);
  ImGui::Text("Tile (%dx%d): [%d, %d] (#%d)", tileSize, tileSize, tileX, tileY,
              tileIdx);
  ImGui::EndTooltip();
}
