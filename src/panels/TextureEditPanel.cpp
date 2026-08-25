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

static bool IsWorkspacePath(const std::string &filePath,
                            const std::string &workspaceDir) {
  if (filePath.empty() || workspaceDir.empty())
    return false;
  try {
    std::filesystem::path f =
        std::filesystem::weakly_canonical(std::filesystem::path(filePath));
    std::filesystem::path w =
        std::filesystem::weakly_canonical(std::filesystem::path(workspaceDir));
    auto rel = f.lexically_relative(w);
    std::string relStr = rel.string();
    if (relStr.empty() || relStr.rfind("..", 0) == 0 ||
        relStr.find(':') != std::string::npos) {
      return false;
    }
    return true;
  } catch (...) {
    return false;
  }
}

TextureEditPanel::TextureEditPanel() = default;

TextureEditPanel::~TextureEditPanel() {
  m_workingTexture.Unload();
}

void TextureEditPanel::Open(const std::string &timPath, FileManager &fileManager,
                            int initialPaletteRow) {
  bool isReadOnly = !IsWorkspacePath(timPath, fileManager.GetWorkspaceDir());
  Open(timPath, isReadOnly, initialPaletteRow);
}

void TextureEditPanel::Open(const std::string &timPath, bool isReadOnly,
                            int initialPaletteRow) {
  if (timPath.empty())
    return;

  m_timPath = timPath;
  m_texName = std::filesystem::path(timPath).stem().string();
  m_isReadOnly = isReadOnly;

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
  m_shouldAutoFit = true;
  m_focusRequested = true;
}

void TextureEditPanel::SetPalette(int paletteRow) {
  int numPalettes = (int)m_workingTexture.GetPalettes().size();
  if (numPalettes > 0) {
    m_currentPalette = std::clamp(paletteRow, 0, numPalettes - 1);
    m_workingTexture.ApplyPalette(m_currentPalette);
  }
}

void TextureEditPanel::Close() { m_isOpen = false; }

void TextureEditPanel::SetZoom(float newZoom) {
  newZoom = std::clamp(newZoom, 0.5f, 32.0f);
  if (std::abs(newZoom - m_zoom) < 0.0001f)
    return;

  float ratio = newZoom / m_zoom;
  m_panOffset.x *= ratio;
  m_panOffset.y *= ratio;
  m_zoom = newZoom;
}

void TextureEditPanel::Save(FileManager &fileManager,
                            Textures &activeMapTexture,
                            int currentMapPalette,
                            LocalGeometryOverlay &localGeometryOverlay,
                            Viewport &sceneViewport) {
  if (m_timPath.empty() || m_isReadOnly)
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
    localGeometryOverlay.RebuildChunksUsingTexture(
        m_texName, fileManager.GetWorkspaceDir(), sceneViewport);
  }
}

void TextureEditPanel::Revert() {
  if (m_isReadOnly)
    return;
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
  if (m_isReadOnly) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f),
                       ICON_FA_LOCK " [Read-Only: Asset Archive]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("This texture is located in the read-only game assets archive.\n"
                        "Extract it to the workspace directory to enable editing and saving.");
    }
  }

  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 220.0f);
  ImGui::Text("Size: %dx%d px | %s", m_workingTexture.GetWidth(),
              m_workingTexture.GetHeight(), bppStr);
}

void TextureEditPanel::DrawToolbar(FileManager &fileManager,
                                  Textures &activeMapTexture,
                                  int currentMapPalette,
                                  LocalGeometryOverlay &localGeometryOverlay,
                                  Viewport &sceneViewport) {
  if (m_isReadOnly) {
    ImGui::BeginDisabled();
    ImGui::Button(ICON_FA_LOCK " Read-Only");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Texture is in the read-only asset archive and cannot be overwritten.\n"
                        "Extract it to the workspace to enable saving.");
    }
  } else {
    if (!m_isDirty)
      ImGui::BeginDisabled();
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
      Save(fileManager, activeMapTexture, currentMapPalette,
           localGeometryOverlay, sceneViewport);
    }
    if (!m_isDirty)
      ImGui::EndDisabled();
  }

  ImGui::SameLine();
  if (m_isReadOnly || !m_isDirty)
    ImGui::BeginDisabled();
  if (ImGui::Button(ICON_FA_ROTATE_LEFT " Revert")) {
    Revert();
  }
  if (m_isReadOnly || !m_isDirty)
    ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  ImGui::Text("Zoom:");
  ImGui::SameLine();
  if (ImGui::Button("-##ZoomOut")) {
    SetZoom(m_zoom / 1.25f);
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(90.0f);
  float tempZoom = m_zoom;
  if (ImGui::SliderFloat("##ZoomSlider", &tempZoom, 0.5f, 32.0f, "%.2fx",
                         ImGuiSliderFlags_Logarithmic)) {
    SetZoom(tempZoom);
  }
  ImGui::SameLine();
  if (ImGui::Button("+##ZoomIn")) {
    SetZoom(m_zoom * 1.25f);
  }

  ImGui::SameLine();
  if (ImGui::Button("1:1")) {
    SetZoom(1.0f);
    m_panOffset = ImVec2(0.0f, 0.0f);
  }

  ImGui::SameLine();
  ImGui::Checkbox("Pixel Grid", &m_showPixelGrid);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Show individual 1x1 pixel grid (visible at zoom >= 4x)");
  }

  ImGui::SameLine();
  ImGui::Checkbox("Tile Grid", &m_showTileGrid);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Show 32x32 pixel tile grid boundaries");
  }

  ImGui::SameLine();
  if (ImGui::Button("Reset Pan")) {
    m_panOffset = ImVec2(0.0f, 0.0f);
  }
}

void TextureEditPanel::DrawSidePanel(float canvasH) {
  float sidePanelW = 46.0f;
  ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(24, 24, 28, 255));
  ImGui::BeginChild("TextureEditToolsSidePanel", ImVec2(sidePanelW, canvasH), true,
                    ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);

  auto DrawToolBtn = [&](TextureEditTool tool, const char *icon,
                         const char *name, const char *desc) {
    bool isActive = (m_activeTool == tool);
    if (isActive) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.45f, 0.75f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.86f, 1.0f, 1.0f));
    }
    if (ImGui::Button(icon, ImVec2(34.0f, 34.0f))) {
      m_activeTool = tool;
    }
    if (isActive) {
      ImGui::PopStyleColor(2);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("%s", name);
      ImGui::Separator();
      ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", desc);
      ImGui::EndTooltip();
    }
  };

  DrawToolBtn(TextureEditTool::Pencil, ICON_FA_PENCIL, "Pencil / Pixel Edit",
              "Draw directly onto texture with selected palette color.");

  DrawToolBtn(TextureEditTool::RectSelect, ICON_FA_VECTOR_SQUARE,
              "Rectangular Select",
              "Select a rectangular pixel region on the texture.");

  DrawToolBtn(TextureEditTool::FillBucket, ICON_FA_FILL_DRIP, "Fill Bucket",
              "Flood-fill contiguous pixels with selected palette color.");

  DrawToolBtn(TextureEditTool::Eyedropper, ICON_FA_EYE_DROPPER,
              "Color Eyedropper",
              "Click on texture pixels to select that palette color index.");

  DrawToolBtn(TextureEditTool::Eraser, ICON_FA_ERASER, "Eraser",
              "Paint with palette index 0 (transparent).");

  ImGui::Separator();

  DrawToolBtn(TextureEditTool::ImportExport, ICON_FA_FILE_IMPORT,
              "Import / Export",
              "Import or export texture to external image (PNG).");

  ImGui::Separator();

  // Active Color Swatch preview & quick color picker summon
  TIMColor activeCol = {0, 0, 0, 255};
  const auto &pals = m_workingTexture.GetPalettes();
  if (!pals.empty()) {
    int palIdx = std::clamp(m_currentPalette, 0, (int)pals.size() - 1);
    if (m_selectedColorIdx >= 0 &&
        m_selectedColorIdx < (int)pals[palIdx].colors.size()) {
      activeCol = pals[palIdx].colors[m_selectedColorIdx];
    }
  }

  ImVec2 curPos = ImGui::GetCursorScreenPos();
  ImDrawList *drawList = ImGui::GetWindowDrawList();
  if (activeCol.a == 0 && activeCol.r == 0 && activeCol.g == 0 &&
      activeCol.b == 0) {
    drawList->AddRectFilled(curPos, ImVec2(curPos.x + 34.0f, curPos.y + 34.0f),
                            IM_COL32(25, 25, 30, 255));
    drawList->AddLine(curPos, ImVec2(curPos.x + 34.0f, curPos.y + 34.0f),
                      IM_COL32(70, 70, 80, 255));
  } else {
    drawList->AddRectFilled(curPos, ImVec2(curPos.x + 34.0f, curPos.y + 34.0f),
                            IM_COL32(activeCol.r, activeCol.g, activeCol.b, 255));
  }
  drawList->AddRect(curPos, ImVec2(curPos.x + 34.0f, curPos.y + 34.0f),
                    IM_COL32(0, 220, 255, 255), 0.0f, 0, 2.0f);

  if (ImGui::InvisibleButton("##ActiveColorBox", ImVec2(34.0f, 34.0f))) {
    m_editingColorIdx = m_selectedColorIdx;
    m_editingColor = activeCol;
    m_editingR5 = activeCol.r * 31 / 255;
    m_editingG5 = activeCol.g * 31 / 255;
    m_editingB5 = activeCol.b * 31 / 255;
    m_editingStp = (activeCol.a == 0)
                       ? false
                       : (activeCol.a < 255 || (activeCol.r == 0 &&
                                                activeCol.g == 0 &&
                                                activeCol.b == 0));
    ImGui::OpenPopup("EditClutColorPopup");
  }
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Selected Color: #%d (Palette %d)", m_selectedColorIdx,
                m_currentPalette);
    ImGui::Text("RGB: (%d, %d, %d)", activeCol.r, activeCol.g, activeCol.b);
    ImGui::Text("Hex: #%02X%02X%02X", activeCol.r, activeCol.g, activeCol.b);
    if (activeCol.a == 0) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "(Transparent STP)");
    }
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
                       "Click to edit 15-bit PS1 color.");
    ImGui::EndTooltip();
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void TextureEditPanel::DrawCanvas(float canvasW, float canvasH) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(28, 28, 32, 255));
  ImGui::BeginChild("TextureEditCanvasChild", ImVec2(canvasW, canvasH), true,
                    ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);

  ImVec2 childPos = ImGui::GetCursorScreenPos();
  ImVec2 childSize = ImGui::GetContentRegionAvail();
  ImDrawList *drawList = ImGui::GetWindowDrawList();

  // Draw dark checkered background for transparency
  TextureCanvasWidget::DrawCheckeredBackground(drawList, childPos, childSize, 16.0f);

  // Handle canvas navigation (pan with middle mouse, exponential zoom with mouse wheel)
  bool isHovered = ImGui::IsWindowHovered();
  ImGuiIO &io = ImGui::GetIO();

  if (isHovered && io.MouseWheel != 0.0f) {
    float factor = (io.MouseWheel > 0.0f) ? 1.25f : (1.0f / 1.25f);
    SetZoom(m_zoom * factor);
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

  // Draw 1x1 pixel grid if zoomed in
  if (m_showPixelGrid) {
    TextureGridWidget::DrawPixelGrid(drawList, p0, p1, w, h, m_zoom, m_showTileGrid);
  }

  // Draw 32x32 tile gridlines if enabled
  if (m_showTileGrid) {
    TextureGridWidget::DrawTileGrid(drawList, p0, p1, w, h, m_zoom, 32);
  }

  // Pixel hover inspection tooltip
  ImVec2 mousePos = ImGui::GetMousePos();
  int px = 0, py = 0;
  if (isHovered &&
      TextureCanvasWidget::ScreenToPixelCoords(mousePos, p0, m_zoom, w, h, px, py)) {
    // Highlight hovered pixel
    ImVec2 hp0(p0.x + (float)px * m_zoom, p0.y + (float)py * m_zoom);
    ImVec2 hp1(hp0.x + m_zoom, hp0.y + m_zoom);
    drawList->AddRect(hp0, hp1, IM_COL32(255, 255, 0, 200), 0.0f, 0, 1.0f);

    int pIdx = py * w + px;
    const auto &raw = m_workingTexture.GetRawIndices();
    const auto &pals = m_workingTexture.GetPalettes();

    if (m_workingTexture.GetBpp() == 0 || m_workingTexture.GetBpp() == 1) {
      if (pIdx < (int)raw.size()) {
        uint8_t cIdx = raw[pIdx];

        // If Eyedropper tool is active and user clicks on a pixel, pick that palette color
        if (m_activeTool == TextureEditTool::Eyedropper &&
            ImGui::IsMouseClicked(0)) {
          m_selectedColorIdx = cIdx;
        }

        ImGui::BeginTooltip();
        ImGui::Text("Pixel: (%d, %d)", px, py);
        int tileX = 0, tileY = 0, tileIdx = 0;
        TextureCanvasWidget::PixelToTileCoords(px, py, tileX, tileY, tileIdx, w, 32);
        ImGui::Text("Tile (32x32): [%d, %d] (#%d)", tileX, tileY, tileIdx);
        ImGui::Text("Color Index: #%d%s", cIdx,
                    (cIdx == m_selectedColorIdx) ? " (Selected)" : "");

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
        if (m_activeTool == TextureEditTool::Eyedropper) {
          ImGui::Separator();
          ImGui::TextColored(ImVec4(0.0f, 0.86f, 1.0f, 1.0f),
                             "Click to pick color #%d", cIdx);
        }
        ImGui::EndTooltip();
      }
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
        m_selectedColorIdx = colorIdx;
        m_editingColorIdx = colorIdx;
        m_editingColor = color;
        m_editingR5 = color.r * 31 / 255;
        m_editingG5 = color.g * 31 / 255;
        m_editingB5 = color.b * 31 / 255;
        m_editingStp = (color.a == 0)
                           ? false
                           : (color.a < 255 || (color.r == 0 && color.g == 0 &&
                                                color.b == 0));
        if (ImGui::IsMouseDoubleClicked(0)) {
          ImGui::OpenPopup("EditClutColorPopup");
        }
      },
      false /* showDimensions */,
      PaletteWidgetLayout::Inline,
      "Palette:",
      m_selectedColorIdx);
}

void TextureEditPanel::DrawColorPickerPopup() {
  if (ImGui::BeginPopup("EditClutColorPopup")) {
    if (m_editingColorIdx >= 0 && !m_workingTexture.GetPalettes().empty()) {
      int palIdx = std::clamp(m_currentPalette, 0,
                              (int)m_workingTexture.GetPalettes().size() - 1);
      auto &pal = m_workingTexture.GetPalettes()[palIdx];

      TIMColor origColor = {0, 0, 0, 0};
      bool hasOrig = (palIdx < (int)m_originalTim.palettes.size() &&
                      m_editingColorIdx < (int)m_originalTim.palettes[palIdx].colors.size());
      if (hasOrig) {
        origColor = m_originalTim.palettes[palIdx].colors[m_editingColorIdx];
      }

      bool isColorDifferentFromOrig = hasOrig &&
          (origColor.r != m_editingColor.r || origColor.g != m_editingColor.g ||
           origColor.b != m_editingColor.b || origColor.a != m_editingColor.a);

      ImGui::Text("Edit Color #%d (Palette %d)", m_editingColorIdx,
                  m_currentPalette);
      if (m_isReadOnly) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f),
                           ICON_FA_LOCK " [Read-Only]");
      }
      ImGui::Separator();

      if (m_isReadOnly) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.2f, 1.0f),
                           "Asset archive file is read-only. Color modifications are locked.");
        ImGui::Spacing();
      }

      bool colorChanged = false;

      // 5-bit PS1 quantized sliders (0–31)
      if (m_isReadOnly)
        ImGui::BeginDisabled();

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

      if (m_isReadOnly)
        ImGui::EndDisabled();

      if (colorChanged && !m_isReadOnly) {
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

      ImGui::Spacing();
      ImGui::Separator();

      // Current vs Original comparison swatches
      ImGui::Text("Comparison:");
      float pWidth = 80.0f;
      float pHeight = 24.0f;
      ImDrawList *drawList = ImGui::GetWindowDrawList();

      // Current preview
      ImGui::Text("Current: #%02X%02X%02X", m_editingColor.r,
                  m_editingColor.g, m_editingColor.b);
      ImVec2 curPos = ImGui::GetCursorScreenPos();
      drawList->AddRectFilled(
          curPos, ImVec2(curPos.x + pWidth, curPos.y + pHeight),
          IM_COL32(m_editingColor.r, m_editingColor.g, m_editingColor.b, 255));
      drawList->AddRect(
          curPos, ImVec2(curPos.x + pWidth, curPos.y + pHeight),
          IM_COL32(255, 255, 255, 255));
      ImGui::Dummy(ImVec2(pWidth, pHeight + 4.0f));

      if (hasOrig) {
        ImGui::SameLine(pWidth + 24.0f);
        ImGui::BeginGroup();
        ImGui::Text("Original: #%02X%02X%02X", origColor.r, origColor.g,
                    origColor.b);
        ImVec2 origPos = ImGui::GetCursorScreenPos();
        drawList->AddRectFilled(
            origPos, ImVec2(origPos.x + pWidth, origPos.y + pHeight),
            IM_COL32(origColor.r, origColor.g, origColor.b, 255));
        drawList->AddRect(
            origPos, ImVec2(origPos.x + pWidth, origPos.y + pHeight),
            IM_COL32(180, 180, 180, 255));
        ImGui::Dummy(ImVec2(pWidth, pHeight + 4.0f));
        ImGui::EndGroup();
      }

      // Revert individual color button
      if (m_isReadOnly || !isColorDifferentFromOrig)
        ImGui::BeginDisabled();
      if (ImGui::Button(ICON_FA_ROTATE_LEFT " Revert Color to Original")) {
        m_editingColor = origColor;
        m_editingR5 = origColor.r * 31 / 255;
        m_editingG5 = origColor.g * 31 / 255;
        m_editingB5 = origColor.b * 31 / 255;
        m_editingStp = (origColor.a == 0)
                           ? false
                           : (origColor.a < 255 || (origColor.r == 0 &&
                                                    origColor.g == 0 &&
                                                    origColor.b == 0));
        if (m_editingColorIdx < (int)pal.colors.size()) {
          pal.colors[m_editingColorIdx] = m_editingColor;
          m_workingTexture.ApplyPalette(m_currentPalette);
          m_isDirty = true;
        }
      }
      if (m_isReadOnly || !isColorDifferentFromOrig)
        ImGui::EndDisabled();

      ImGui::Spacing();
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

  // Dynamically recheck workspace status in case workspace contents changed
  m_isReadOnly = !IsWorkspacePath(m_timPath, fileManager.GetWorkspaceDir());

  if (m_shouldAutoFit) {
    int w = m_workingTexture.GetWidth() > 0 ? m_workingTexture.GetWidth() : 256;
    int h = m_workingTexture.GetHeight() > 0 ? m_workingTexture.GetHeight() : 256;

    // Minimum width required so that all toolbar buttons and controls fit comfortably without wrapping
    float minToolbarWidth = 720.0f;
    float desiredWidth = std::max(minToolbarWidth, 46.0f + (float)w * m_zoom + 50.0f);

    // Calculate vertical height adapting to all internal components:
    // Window titlebar + Header + Separator + Toolbar + Canvas (texture at initial zoom + padding) + CLUT (if any) + window padding
    float titlebarH = ImGui::GetFrameHeight();
    float headerH = ImGui::GetTextLineHeightWithSpacing() + 8.0f;
    float toolbarH = ImGui::GetFrameHeightWithSpacing() + 8.0f;
    float canvasH = (float)h * m_zoom + 24.0f;
    float clutH = (!m_workingTexture.GetPalettes().empty())
                      ? (ImGui::GetFrameHeightWithSpacing() + 16.0f)
                      : 0.0f;
    float windowPaddingY = ImGui::GetStyle().WindowPadding.y * 2.0f + 16.0f;

    float desiredHeight =
        titlebarH + headerH + toolbarH + canvasH + clutH + windowPaddingY;

    // Clamp to reasonable screen viewport bounds
    ImGuiViewport *mainViewport = ImGui::GetMainViewport();
    if (mainViewport) {
      desiredWidth = std::min(desiredWidth, mainViewport->WorkSize.x * 0.95f);
      desiredHeight = std::min(desiredHeight, mainViewport->WorkSize.y * 0.90f);
    }
    desiredWidth = std::max(desiredWidth, 500.0f);
    desiredHeight = std::max(desiredHeight, 350.0f);

    ImGui::SetNextWindowSize(ImVec2(desiredWidth, desiredHeight),
                             ImGuiCond_Always);
    m_shouldAutoFit = false;
  }

  ImGui::SetNextWindowSizeConstraints(ImVec2(450.0f, 300.0f),
                                      ImVec2(FLT_MAX, FLT_MAX));

  if (m_focusRequested) {
    ImGui::SetNextWindowFocus();
    m_focusRequested = false;
  }

  std::string winTitle =
      ICON_FA_IMAGE " Texture Editor: " + m_texName +
      (m_isReadOnly ? " [Read-Only]" : (m_isDirty ? " *" : "")) +
      "###TextureEditorPopout_" + m_timPath;

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking;

  if (ImGui::Begin(winTitle.c_str(), &m_isOpen, flags)) {
    // Handle Ctrl+S inside Texture Editor window (if not read-only)
    ImGuiIO &io = ImGui::GetIO();
    if (!m_isReadOnly &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))) {
      Save(fileManager, activeMapTexture, currentMapPalette,
           localGeometryOverlay, sceneViewport);
    }

    DrawHeader();
    ImGui::Separator();
    DrawToolbar(fileManager, activeMapTexture, currentMapPalette,
                localGeometryOverlay, sceneViewport);

    float sidePanelW = 46.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float clutReserve = m_workingTexture.GetPalettes().empty()
                            ? 0.0f
                            : (ImGui::GetFrameHeightWithSpacing() + 10.0f);
    float canvasH = std::max(120.0f, avail.y - clutReserve);
    float canvasW = std::max(120.0f, avail.x - sidePanelW - ImGui::GetStyle().ItemSpacing.x);

    DrawSidePanel(canvasH);
    ImGui::SameLine();
    DrawCanvas(canvasW, canvasH);
    DrawClutEditor();
    DrawColorPickerPopup();
  }
  ImGui::End();
}
