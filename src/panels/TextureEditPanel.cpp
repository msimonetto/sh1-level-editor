#include "panels/TextureEditPanel.h"
#include "core/Config.h"
#include "core/FileManager.h"
#include "extras/IconsFontAwesome6.h"
#include "formats/TextureCache.h"
#include "imgui_internal.h"
#include "viewport/LocalGeometryOverlay.h"
#include "viewport/Viewport.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>

TextureEditPanel::TextureEditPanel() = default;

TextureEditPanel::~TextureEditPanel() {
  m_workingTexture.Unload();
}

void TextureEditPanel::Open(const std::string &timPath, int initialPaletteRow) {
  if (timPath.empty())
    return;

  m_timPath = timPath;
  m_texName = std::filesystem::path(timPath).stem().string();

  if (!m_workingTexture.Load(m_timPath)) {
    printf("[TextureEditPanel] Failed to load TIM: %s\n", timPath.c_str());
    return;
  }

  m_originalTim = m_workingTexture.GetDecoded();
  int numPalettes = (int)m_workingTexture.GetPalettes().size();
  m_currentPalette = (numPalettes > 0)
                         ? std::clamp(initialPaletteRow, 0, numPalettes - 1)
                         : 0;
  m_workingTexture.ApplyPalette(m_currentPalette);

  m_isOpen = true;
  m_isDirty = false;
  m_panOffset = ImVec2(0.0f, 0.0f);
  m_zoom = 2.0f;
  m_editingColorIdx = -1;
}

void TextureEditPanel::Close() { m_isOpen = false; }

void TextureEditPanel::Save(FileManager &fileManager,
                            Textures &activeMapTexture,
                            int currentMapPalette,
                            LocalGeometryOverlay &localGeometryOverlay,
                            Viewport &sceneViewport) {
  if (m_timPath.empty())
    return;

  if (TIMEncoder::Encode(m_workingTexture.GetDecoded(), m_timPath)) {
    m_originalTim = m_workingTexture.GetDecoded();
    m_isDirty = false;

    // Invalidate global texture cache
    TextureCache::Get().Invalidate(m_texName);

    // If currently selected in Texture Map panel, reload it
    if (Config::Get().LastTexturePath == m_timPath) {
      activeMapTexture.Load(m_timPath);
      activeMapTexture.ApplyPalette(currentMapPalette);
    }

    // Rebuild viewports for any chunks that use this texture
    std::string wsDir = fileManager.GetWorkspaceDir();
    for (const auto &lc : localGeometryOverlay.GetChunks()) {
      if (lc.data) {
        bool usesTexture = false;
        for (const auto &name : lc.data->localTexNames) {
          if (name == m_texName) {
            usesTexture = true;
            break;
          }
        }
        if (!usesTexture) {
          for (const auto &name : lc.data->globalTexNames) {
            if (name == m_texName) {
              usesTexture = true;
              break;
            }
          }
        }
        if (usesTexture) {
          sceneViewport.RebuildChunkBatches(lc.data->chunkName, wsDir);
          localGeometryOverlay.RebuildChunkBatches(lc.data->chunkName, wsDir);
        }
      }
    }
  }
}

void TextureEditPanel::Revert() {
  m_workingTexture.GetDecoded() = m_originalTim;
  m_workingTexture.ApplyPalette(m_currentPalette);
  m_isDirty = false;
}

void TextureEditPanel::DrawHeader() {
  const char *bppStr = "Unknown";
  int bpp = m_workingTexture.GetBpp();
  if (bpp == 0)
    bppStr = "4-bit Indexed (16 colors / row)";
  else if (bpp == 1)
    bppStr = "8-bit Indexed (256 colors / row)";
  else if (bpp == 2)
    bppStr = "16-bit Direct Color (RGB555)";

  ImGui::Text("File: %s%s", m_texName.c_str(), m_isDirty ? " *" : "");
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 220.0f);
  ImGui::Text("Size: %dx%d px | %s", m_workingTexture.GetWidth(),
              m_workingTexture.GetHeight(), bppStr);
}

void TextureEditPanel::DrawToolbar(FileManager &fileManager,
                                  Textures &activeMapTexture,
                                  int currentMapPalette,
                                  LocalGeometryOverlay &localGeometryOverlay,
                                  Viewport &sceneViewport) {
  if (!m_isDirty)
    ImGui::BeginDisabled();
  if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
    Save(fileManager, activeMapTexture, currentMapPalette,
         localGeometryOverlay, sceneViewport);
  }
  if (!m_isDirty)
    ImGui::EndDisabled();

  ImGui::SameLine();
  if (!m_isDirty)
    ImGui::BeginDisabled();
  if (ImGui::Button(ICON_FA_ROTATE_LEFT " Revert")) {
    Revert();
  }
  if (!m_isDirty)
    ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  ImGui::Text("Zoom:");
  ImGui::SameLine();
  if (ImGui::Button("-##ZoomOut") && m_zoom > 1.0f) {
    m_zoom = std::max(1.0f, m_zoom / 1.5f);
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(70.0f);
  if (ImGui::SliderFloat("##Zoom", &m_zoom, 1.0f, 16.0f, "%.1fx")) {
    m_zoom = std::clamp(m_zoom, 1.0f, 16.0f);
  }
  ImGui::SameLine();
  if (ImGui::Button("+##ZoomIn") && m_zoom < 16.0f) {
    m_zoom = std::min(16.0f, m_zoom * 1.5f);
  }

  ImGui::SameLine();
  if (ImGui::Button("1:1")) {
    m_zoom = 1.0f;
    m_panOffset = ImVec2(0.0f, 0.0f);
  }

  ImGui::SameLine();
  ImGui::Checkbox("Pixel Grid", &m_showPixelGrid);

  ImGui::SameLine();
  if (ImGui::Button("Reset Pan")) {
    m_panOffset = ImVec2(0.0f, 0.0f);
  }
}

void TextureEditPanel::DrawCanvas() {
  ImVec2 avail = ImGui::GetContentRegionAvail();
  float canvasW = avail.x;
  float canvasH = std::max(150.0f, avail.y - 70.0f);

  ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(28, 28, 32, 255));
  ImGui::BeginChild("TextureEditCanvasChild", ImVec2(canvasW, canvasH), true,
                    ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);

  ImVec2 childPos = ImGui::GetCursorScreenPos();
  ImVec2 childSize = ImGui::GetContentRegionAvail();
  ImDrawList *drawList = ImGui::GetWindowDrawList();

  // Draw dark checkered background for transparency
  float checkSize = 16.0f;
  for (float y = childPos.y; y < childPos.y + childSize.y; y += checkSize) {
    for (float x = childPos.x; x < childPos.x + childSize.x; x += checkSize) {
      int ix = (int)((x - childPos.x) / checkSize);
      int iy = (int)((y - childPos.y) / checkSize);
      ImU32 col = ((ix + iy) % 2 == 0) ? IM_COL32(35, 35, 40, 255)
                                       : IM_COL32(25, 25, 30, 255);
      drawList->AddRectFilled(
          ImVec2(x, y),
          ImVec2(std::min(x + checkSize, childPos.x + childSize.x),
                 std::min(y + checkSize, childPos.y + childSize.y)),
          col);
    }
  }

  // Handle canvas navigation (pan with middle mouse, zoom with mouse wheel)
  bool isHovered = ImGui::IsWindowHovered();
  ImGuiIO &io = ImGui::GetIO();

  if (isHovered && io.MouseWheel != 0.0f) {
    m_zoom = std::clamp(m_zoom + io.MouseWheel * 0.5f, 1.0f, 16.0f);
  }

  if (isHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
    m_panOffset.x += io.MouseDelta.x;
    m_panOffset.y += io.MouseDelta.y;
  }

  int w = m_workingTexture.GetWidth() > 0 ? m_workingTexture.GetWidth() : 256;
  int h = m_workingTexture.GetHeight() > 0 ? m_workingTexture.GetHeight() : 256;

  float texW = (float)w * m_zoom;
  float texH = (float)h * m_zoom;

  // Center image + apply pan offset
  float imgX = childPos.x + (childSize.x - texW) * 0.5f + m_panOffset.x;
  float imgY = childPos.y + (childSize.y - texH) * 0.5f + m_panOffset.y;

  ImVec2 p0(imgX, imgY);
  ImVec2 p1(imgX + texW, imgY + texH);

  // Render texture image
  Texture2D tex = m_workingTexture.GetTexture();
  if (tex.id != 0) {
    drawList->AddImage((ImTextureID)(intptr_t)tex.id, p0, p1);
  }

  // Border around texture
  drawList->AddRect(p0, p1, IM_COL32(90, 90, 100, 255), 0.0f, 0, 1.0f);

  // Draw pixel grid if zoomed in
  if (m_showPixelGrid && m_zoom >= 4.0f && w > 0 && h > 0) {
    ImU32 gridCol = IM_COL32(255, 255, 255, 30);
    for (int x = 0; x <= w; ++x) {
      float lx = p0.x + (float)x * m_zoom;
      drawList->AddLine(ImVec2(lx, p0.y), ImVec2(lx, p1.y), gridCol);
    }
    for (int y = 0; y <= h; ++y) {
      float ly = p0.y + (float)y * m_zoom;
      drawList->AddLine(ImVec2(p0.x, ly), ImVec2(p1.x, ly), gridCol);
    }
  }

  // Pixel hover inspection tooltip
  ImVec2 mousePos = ImGui::GetMousePos();
  if (isHovered && mousePos.x >= p0.x && mousePos.x < p1.x &&
      mousePos.y >= p0.y && mousePos.y < p1.y) {
    int px = (int)((mousePos.x - p0.x) / m_zoom);
    int py = (int)((mousePos.y - p0.y) / m_zoom);

    if (px >= 0 && px < w && py >= 0 && py < h) {
      // Highlight hovered pixel
      ImVec2 hp0(p0.x + (float)px * m_zoom, p0.y + (float)py * m_zoom);
      ImVec2 hp1(hp0.x + m_zoom, hp0.y + m_zoom);
      drawList->AddRect(hp0, hp1, IM_COL32(255, 255, 0, 200), 0.0f, 0, 1.0f);

      ImGui::BeginTooltip();
      ImGui::Text("Pixel (%d, %d)", px, py);
      int pIdx = py * w + px;

      const auto &raw = m_workingTexture.GetRawIndices();
      const auto &pals = m_workingTexture.GetPalettes();

      if (m_workingTexture.GetBpp() == 0 || m_workingTexture.GetBpp() == 1) {
        if (pIdx < (int)raw.size()) {
          uint8_t cIdx = raw[pIdx];
          ImGui::Text("Color Index: #%d", cIdx);
          if (!pals.empty()) {
            int palIdx = std::clamp(m_currentPalette, 0, (int)pals.size() - 1);
            if (cIdx < pals[palIdx].colors.size()) {
              const auto &c = pals[palIdx].colors[cIdx];
              ImGui::Text("RGB: (%d, %d, %d)", c.r, c.g, c.b);
              ImGui::Text("Hex: #%02X%02X%02X", c.r, c.g, c.b);
              if (c.a == 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f),
                                   "(Transparent STP)");
              }
            }
          }
        }
      }
      ImGui::EndTooltip();
    }
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void TextureEditPanel::DrawClutEditor() {
  if (m_workingTexture.GetPalettes().empty())
    return;

  ImGui::Separator();
  PaletteInspectorWidget::Draw(
      m_workingTexture, m_currentPalette,
      [&](int newPal) {
        m_currentPalette = newPal;
      },
      [&](int colorIdx, TIMColor color) {
        m_editingColorIdx = colorIdx;
        m_editingColor = color;
        m_editingR5 = color.r * 31 / 255;
        m_editingG5 = color.g * 31 / 255;
        m_editingB5 = color.b * 31 / 255;
        m_editingStp = (color.a == 0)
                           ? false
                           : (color.a < 255 || (color.r == 0 && color.g == 0 &&
                                                color.b == 0));
        ImGui::OpenPopup("EditClutColorPopup");
      },
      false /* showDimensions */);
}

void TextureEditPanel::DrawColorPickerPopup() {
  if (ImGui::BeginPopup("EditClutColorPopup")) {
    if (m_editingColorIdx >= 0 && !m_workingTexture.GetPalettes().empty()) {
      int palIdx = std::clamp(m_currentPalette, 0,
                              (int)m_workingTexture.GetPalettes().size() - 1);
      auto &pal = m_workingTexture.GetPalettes()[palIdx];

      ImGui::Text("Edit Color #%d (CLUT Row %d)", m_editingColorIdx,
                  m_currentPalette);
      ImGui::Separator();

      bool colorChanged = false;

      // 5-bit PS1 quantized sliders (0–31)
      ImGui::Text("PS1 15-bit Quantized (0-31):");
      if (ImGui::SliderInt("Red (R5)", &m_editingR5, 0, 31)) {
        colorChanged = true;
      }
      if (ImGui::SliderInt("Green (G5)", &m_editingG5, 0, 31)) {
        colorChanged = true;
      }
      if (ImGui::SliderInt("Blue (B5)", &m_editingB5, 0, 31)) {
        colorChanged = true;
      }

      if (ImGui::Checkbox("Semi-Transparent (STP Bit)", &m_editingStp)) {
        colorChanged = true;
      }

      if (ImGui::Button("Set as Transparent (0x0000)")) {
        m_editingR5 = 0;
        m_editingG5 = 0;
        m_editingB5 = 0;
        m_editingStp = false;
        m_editingColor = {0, 0, 0, 0};
        colorChanged = true;
      }

      if (colorChanged) {
        uint8_t r8 = (uint8_t)(m_editingR5 * 255 / 31);
        uint8_t g8 = (uint8_t)(m_editingG5 * 255 / 31);
        uint8_t b8 = (uint8_t)(m_editingB5 * 255 / 31);
        uint8_t a8 = 255;
        if (m_editingR5 == 0 && m_editingG5 == 0 && m_editingB5 == 0 &&
            !m_editingStp) {
          a8 = 0; // Pure 0x0000 is transparent
        } else if (m_editingStp &&
                   (m_editingR5 != 0 || m_editingG5 != 0 || m_editingB5 != 0)) {
          a8 = 180; // Semi-transparent
        }

        m_editingColor = {r8, g8, b8, a8};

        if (m_editingColorIdx < (int)pal.colors.size()) {
          pal.colors[m_editingColorIdx] = m_editingColor;
          m_workingTexture.ApplyPalette(m_currentPalette);
          m_isDirty = true;
        }
      }

      // Color preview swatch
      ImGui::Spacing();
      ImGui::Text("Preview: #%02X%02X%02X (R:%d G:%d B:%d)", m_editingColor.r,
                  m_editingColor.g, m_editingColor.b, m_editingColor.r,
                  m_editingColor.g, m_editingColor.b);
      ImVec2 previewPos = ImGui::GetCursorScreenPos();
      float pSize = 30.0f;
      ImDrawList *drawList = ImGui::GetWindowDrawList();
      drawList->AddRectFilled(
          previewPos, ImVec2(previewPos.x + 120.0f, previewPos.y + pSize),
          IM_COL32(m_editingColor.r, m_editingColor.g, m_editingColor.b, 255));
      drawList->AddRect(
          previewPos, ImVec2(previewPos.x + 120.0f, previewPos.y + pSize),
          IM_COL32(255, 255, 255, 255));
      ImGui::Dummy(ImVec2(120.0f, pSize + 4.0f));

      if (ImGui::Button("Close", ImVec2(80.0f, 0))) {
        ImGui::CloseCurrentPopup();
      }
    }
    ImGui::EndPopup();
  }
}

void TextureEditPanel::Draw(FileManager &fileManager, Textures &activeMapTexture,
                            int currentMapPalette,
                            LocalGeometryOverlay &localGeometryOverlay,
                            Viewport &sceneViewport) {
  if (!m_isOpen)
    return;

  ImGui::SetNextWindowSize(ImVec2(680.0f, 620.0f), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSizeConstraints(ImVec2(400.0f, 350.0f),
                                      ImVec2(FLT_MAX, FLT_MAX));

  std::string winTitle =
      ICON_FA_IMAGE " Texture Editor: " + m_texName + (m_isDirty ? " *" : "") +
      "###TextureEditorPopout";

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking;

  if (ImGui::Begin(winTitle.c_str(), &m_isOpen, flags)) {
    // Handle Ctrl+S inside Texture Editor window
    ImGuiIO &io = ImGui::GetIO();
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))) {
      Save(fileManager, activeMapTexture, currentMapPalette,
           localGeometryOverlay, sceneViewport);
    }

    DrawHeader();
    ImGui::Separator();
    DrawToolbar(fileManager, activeMapTexture, currentMapPalette,
                localGeometryOverlay, sceneViewport);
    DrawCanvas();
    DrawClutEditor();
    DrawColorPickerPopup();
  }
  ImGui::End();
}
