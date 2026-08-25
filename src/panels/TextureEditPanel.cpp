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

static std::string FindAssetTimPath(const std::string &texName,
                                    const std::string &assetsDir) {
  if (texName.empty() || assetsDir.empty())
    return "";

  namespace fs = std::filesystem;
  std::vector<fs::path> candidates = {
      fs::path(assetsDir) / "BG" / (texName + ".TIM"),
      fs::path(assetsDir) / "BG" / (texName + ".tim"),
      fs::path(assetsDir) / "TIM" / (texName + ".TIM"),
      fs::path(assetsDir) / "TIM" / (texName + ".tim"),
      fs::path(assetsDir) / (texName + ".TIM"),
      fs::path(assetsDir) / (texName + ".tim"),
  };

  for (const auto &p : candidates) {
    if (fs::exists(p) && fs::is_regular_file(p)) {
      return p.string();
    }
  }
  return "";
}

struct TextureClipboard {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rawIndices;
  std::vector<TIMColor> directPixels;
  bool isDirect = false;
};

static TextureClipboard s_textureClipboard;

static int GetSelectionGridStep(bool isAltDown, float zoom) {
  // When Alt is held down, enable per-pixel (1x1) precision at any zoom level
  if (isAltDown) {
    return 1;
  }
  // Default staggered snapping based on zoom level (32x32 at default 2.0x zoom):
  if (zoom <= 2.0f)
    return 32; // 32x32 tile snapping at 2.0x or lower
  if (zoom < 4.0f)
    return 16; // 16x16 pixel snapping
  if (zoom < 6.0f)
    return 8;  // 8x8 pixel snapping
  if (zoom < 8.0f)
    return 4;  // 4x4 pixel snapping
  return 1;    // 1x1 per-pixel snapping at 8.0x+ zoom
}

TextureEditPanel::TextureEditPanel() = default;

TextureEditPanel::~TextureEditPanel() {
  m_workingTexture.Unload();
}

void TextureEditPanel::Open(const std::string &timPath, FileManager &fileManager,
                            int initialPaletteRow) {
  bool isReadOnly = !IsWorkspacePath(timPath, fileManager.GetWorkspaceDir());
  std::string texName = std::filesystem::path(timPath).stem().string();
  std::string assetTimPath = FindAssetTimPath(texName, fileManager.GetAssetsDir());
  Open(timPath, isReadOnly, initialPaletteRow, assetTimPath);
}

void TextureEditPanel::Open(const std::string &timPath, bool isReadOnly,
                            int initialPaletteRow,
                            const std::string &assetTimPath) {
  if (timPath.empty())
    return;

  m_timPath = timPath;
  m_texName = std::filesystem::path(timPath).stem().string();
  m_isReadOnly = isReadOnly;

  if (!m_workingTexture.Load(m_timPath)) {
    printf("[TextureEditPanel] Failed to load TIM: %s\n", timPath.c_str());
    return;
  }

  // Look up pristine original asset from disk
  std::string resolvedAssetPath = assetTimPath;
  if (resolvedAssetPath.empty()) {
    std::string defaultAssetsDir =
        (std::filesystem::path(Config::Get().ProjectDirectory) / "assets").string();
    resolvedAssetPath = FindAssetTimPath(m_texName, defaultAssetsDir);
  }

  m_hasOriginalAsset = false;
  if (!resolvedAssetPath.empty() && std::filesystem::exists(resolvedAssetPath)) {
    if (TIMDecoder::Decode(resolvedAssetPath, m_originalTim)) {
      m_hasOriginalAsset = true;
    }
  }
  if (!m_hasOriginalAsset) {
    m_originalTim = m_workingTexture.GetDecoded();
  }

  int numPalettes = (int)m_workingTexture.GetPalettes().size();
  m_currentPalette = (numPalettes > 0)
                         ? std::clamp(initialPaletteRow, 0, numPalettes - 1)
                         : 0;
  m_workingTexture.ApplyPalette(m_currentPalette);

  m_undoBuffer.Clear();
  m_isPaintingStroke = false;
  m_strokeModified = false;
  m_isFocused = false;

  m_isOpen = true;
  m_isDirty = false;
  m_panOffset = ImVec2(0.0f, 0.0f);
  m_zoom = 2.0f;
  m_editingColorIdx = -1;
  m_selectedColorIdx = 0;
  SetEditingColor({0, 0, 0, 255});
  m_activeTool = TextureEditTool::Pencil;
  m_hasSelection = false;
  m_isSelecting = false;
  m_shouldAutoFit = true;
  m_focusRequested = true;
  m_lastCheckedWorkspaceDir.clear();
}

void TextureEditPanel::SetEditingColor(const TIMColor &c) {
  m_editingColor = c;
  c.ToR5G5B5(m_editingR5, m_editingG5, m_editingB5, m_editingStp);
}

bool TextureEditPanel::IsDifferentFromOriginal() const {
  return m_workingTexture.GetDecoded() != m_originalTim;
}

void TextureEditPanel::SetPalette(int paletteRow) {
  int numPalettes = (int)m_workingTexture.GetPalettes().size();
  if (numPalettes > 0) {
    m_currentPalette = std::clamp(paletteRow, 0, numPalettes - 1);
    m_workingTexture.ApplyPalette(m_currentPalette);
  }
}

void TextureEditPanel::Close() {
  m_isOpen = false;
  m_isFocused = false;
  m_undoBuffer.Clear();
}

bool TextureEditPanel::Undo() {
  if (m_isReadOnly || !m_undoBuffer.CanUndo())
    return false;
  std::string desc;
  if (m_undoBuffer.Undo(m_workingTexture.GetDecoded(), m_currentPalette, desc)) {
    m_workingTexture.ApplyPalette(m_currentPalette);
    m_hasSelection = false;
    m_isDirty = true;
    return true;
  }
  return false;
}

bool TextureEditPanel::Redo() {
  if (m_isReadOnly || !m_undoBuffer.CanRedo())
    return false;
  std::string desc;
  if (m_undoBuffer.Redo(m_workingTexture.GetDecoded(), m_currentPalette, desc)) {
    m_workingTexture.ApplyPalette(m_currentPalette);
    m_hasSelection = false;
    m_isDirty = true;
    return true;
  }
  return false;
}

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
    // Note: m_originalTim is deliberately NOT overwritten here,
    // so Revert can always restore back to the original game asset file!
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
  m_undoBuffer.Push(m_workingTexture.GetDecoded(), m_currentPalette, "Revert to Original");
  m_workingTexture.GetDecoded() = m_originalTim;
  m_workingTexture.ApplyPalette(m_currentPalette);
  m_hasSelection = false;
  m_isDirty = true;
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
  } else if (m_hasOriginalAsset) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                       ICON_FA_CIRCLE_CHECK " [Original Asset Linked]");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Original game asset is linked. You can restore original pixels and palettes at any time.");
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
    ImGui::BeginDisabled(true);
    ImGui::Button(ICON_FA_LOCK " Read-Only");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Texture is in the read-only asset archive and cannot be overwritten.\n"
                        "Extract it to the workspace to enable saving.");
    }
  } else {
    bool canSave = m_isDirty;
    ImGui::BeginDisabled(!canSave);
    if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save")) {
      Save(fileManager, activeMapTexture, currentMapPalette,
           localGeometryOverlay, sceneViewport);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Save modified texture to workspace and update 3D viewport (Ctrl+S).");
    }
  }

  ImGui::SameLine();
  bool canUndo = !m_isReadOnly && m_undoBuffer.CanUndo();
  ImGui::BeginDisabled(!canUndo);
  if (ImGui::Button(ICON_FA_ARROW_ROTATE_LEFT " Undo")) {
    Undo();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (canUndo) {
      ImGui::SetTooltip("Undo: %s (Ctrl+Z)", m_undoBuffer.PeekUndoDesc().c_str());
    } else {
      ImGui::SetTooltip("Undo (Ctrl+Z)");
    }
  }

  ImGui::SameLine();
  bool canRedo = !m_isReadOnly && m_undoBuffer.CanRedo();
  ImGui::BeginDisabled(!canRedo);
  if (ImGui::Button(ICON_FA_ARROW_ROTATE_RIGHT " Redo")) {
    Redo();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (canRedo) {
      ImGui::SetTooltip("Redo: %s (Ctrl+Y)", m_undoBuffer.PeekRedoDesc().c_str());
    } else {
      ImGui::SetTooltip("Redo (Ctrl+Y / Ctrl+Shift+Z)");
    }
  }

  ImGui::SameLine();
  bool canRevert = !m_isReadOnly && (m_isDirty || IsDifferentFromOriginal());
  ImGui::BeginDisabled(!canRevert);
  if (ImGui::Button(ICON_FA_CLOCK_ROTATE_LEFT " Revert")) {
    Revert();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Revert all pixel data and color palettes back to the original game asset file.");
  }

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

void TextureEditPanel::DrawCanvas(float canvasW, float canvasH) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(28, 28, 32, 255));
  ImGui::BeginChild("TextureEditCanvasChild", ImVec2(canvasW, canvasH), true,
                    ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);

  ImVec2 childPos = ImGui::GetCursorScreenPos();
  ImVec2 childSize = ImGui::GetContentRegionAvail();
  ImDrawList *drawList = ImGui::GetWindowDrawList();

  // Draw dark checkered background for transparency
  TextureCanvasWidget::DrawCheckeredBackground(drawList, childPos, childSize,
                                              16.0f);

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
    TextureGridWidget::DrawPixelGrid(drawList, p0, p1, w, h, m_zoom,
                                     m_showTileGrid);
  }

  // Draw 32x32 tile gridlines if enabled
  if (m_showTileGrid) {
    TextureGridWidget::DrawTileGrid(drawList, p0, p1, w, h, m_zoom, 32);
  }

  // Draw selection marquee overlay if active
  if (m_hasSelection && w > 0 && h > 0) {
    TextureEditTools::DrawSelectionMarquee(drawList, p0, m_zoom, m_selMinX,
                                          m_selMinY, m_selMaxX, m_selMaxY);
  }

  // Pixel hover inspection & tool interaction
  ImVec2 mousePos = ImGui::GetMousePos();
  int px = 0, py = 0;
  bool isInsidePixel =
      TextureCanvasWidget::ScreenToPixelCoords(mousePos, p0, m_zoom, w, h, px, py);

  if (isHovered && isInsidePixel) {
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

        ImGui::BeginTooltip();
        ImGui::Text("Pixel: (%d, %d)", px, py);
        int tileX = 0, tileY = 0, tileIdx = 0;
        TextureCanvasWidget::PixelToTileCoords(px, py, tileX, tileY, tileIdx, w,
                                              32);
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
        switch (m_activeTool) {
        case TextureEditTool::RectSelect: {
          int gridStep = GetSelectionGridStep(io.KeyAlt, m_zoom);
          ImGui::Separator();
          ImGui::TextColored(ImVec4(0.0f, 0.86f, 1.0f, 1.0f),
                             "Grid Snap: %dx%d (%s)", gridStep, gridStep,
                             io.KeyAlt ? "Per-Pixel (Alt)" : (gridStep == 32 ? "32x32 Tile" : "Zoom Scaled"));
          if (!io.KeyAlt && gridStep > 1) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                               "Hold [Alt] for 1x1 per-pixel snap");
          }
          break;
        }
        case TextureEditTool::Eyedropper:
          ImGui::Separator();
          ImGui::TextColored(ImVec4(0.0f, 0.86f, 1.0f, 1.0f),
                             "Click to pick color #%d", cIdx);
          break;
        case TextureEditTool::FillBucket:
          ImGui::Separator();
          ImGui::TextColored(ImVec4(0.0f, 0.86f, 1.0f, 1.0f),
                             "Click to flood-fill with color #%d",
                             m_selectedColorIdx);
          break;
        default:
          break;
        }
        ImGui::EndTooltip();
      }
    }
  }

  // Handle interactive tool actions
  if (isHovered && !m_isReadOnly) {
    switch (m_activeTool) {
    case TextureEditTool::RectSelect:
      HandleToolRectSelect(io, mousePos, p0, w, h, px, py, isInsidePixel);
      break;
    case TextureEditTool::Pencil:
    case TextureEditTool::Eraser:
      HandleToolPencilEraser(w, h, px, py, isInsidePixel);
      break;
    case TextureEditTool::FillBucket:
      HandleToolFillBucket(w, h, px, py, isInsidePixel);
      break;
    case TextureEditTool::Eyedropper:
      HandleToolEyedropper(w, h, px, py, isInsidePixel);
      break;
    }
  }

  // Finalize painting stroke when mouse is released
  if (m_isPaintingStroke && !ImGui::IsMouseDown(0)) {
    if (m_strokeModified) {
      m_undoBuffer.Push(m_strokeStartSnapshot, m_currentPalette, m_strokeDesc);
    }
    m_isPaintingStroke = false;
    m_strokeModified = false;
  }

  // Handle selection keyboard operations
  // Right-click context menu trigger
  if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    ImGui::OpenPopup("TextureSelectionContextMenu");
  }

  // Draw Context Menu
  DrawSelectionContextMenu();

  // Handle selection keyboard operations
  if (m_hasSelection && !m_isReadOnly) {
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) ||
        ImGui::IsKeyPressed(ImGuiKey_Backspace)) {
      FillSelectionWithColor(0);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      Deselect();
    }
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void TextureEditPanel::CopySelection() {
  if (!m_hasSelection)
    return;
  int w = m_workingTexture.GetWidth();
  int h = m_workingTexture.GetHeight();
  int selW = m_selMaxX - m_selMinX + 1;
  int selH = m_selMaxY - m_selMinY + 1;
  if (selW <= 0 || selH <= 0 || w <= 0 || h <= 0)
    return;

  s_textureClipboard.width = selW;
  s_textureClipboard.height = selH;
  s_textureClipboard.isDirect = (m_workingTexture.GetBpp() == 2);
  s_textureClipboard.rawIndices.resize(selW * selH);

  const auto &raw = m_workingTexture.GetRawIndices();
  for (int y = 0; y < selH; ++y) {
    for (int x = 0; x < selW; ++x) {
      int srcIdx = (m_selMinY + y) * w + (m_selMinX + x);
      int dstIdx = y * selW + x;
      s_textureClipboard.rawIndices[dstIdx] =
          (srcIdx >= 0 && srcIdx < (int)raw.size()) ? raw[srcIdx] : 0;
    }
  }

  if (s_textureClipboard.isDirect) {
    const auto &direct = m_workingTexture.GetDecoded().directPixels;
    s_textureClipboard.directPixels.resize(selW * selH);
    for (int y = 0; y < selH; ++y) {
      for (int x = 0; x < selW; ++x) {
        int srcIdx = (m_selMinY + y) * w + (m_selMinX + x);
        int dstIdx = y * selW + x;
        s_textureClipboard.directPixels[dstIdx] =
            (srcIdx >= 0 && srcIdx < (int)direct.size()) ? direct[srcIdx]
                                                         : TIMColor{0, 0, 0, 255};
      }
    }
  }
}

void TextureEditPanel::CutSelection() {
  if (m_isReadOnly || !m_hasSelection)
    return;
  CopySelection();
  m_undoBuffer.Push(m_workingTexture.GetDecoded(), m_currentPalette, "Cut Selection");
  TextureEditTools::FillSelection(m_workingTexture, m_currentPalette, 0,
                                 m_selMinX, m_selMinY, m_selMaxX, m_selMaxY,
                                 m_isReadOnly);
  m_isDirty = true;
}

void TextureEditPanel::PasteClipboard() {
  if (m_isReadOnly || s_textureClipboard.width <= 0 ||
      s_textureClipboard.height <= 0)
    return;

  int w = m_workingTexture.GetWidth();
  int h = m_workingTexture.GetHeight();
  if (w <= 0 || h <= 0)
    return;

  int startX = m_hasSelection ? m_selMinX : 0;
  int startY = m_hasSelection ? m_selMinY : 0;

  m_undoBuffer.Push(m_workingTexture.GetDecoded(), m_currentPalette, "Paste");

  auto &raw = m_workingTexture.GetRawIndices();
  for (int y = 0; y < s_textureClipboard.height; ++y) {
    int dstY = startY + y;
    if (dstY < 0 || dstY >= h)
      continue;
    for (int x = 0; x < s_textureClipboard.width; ++x) {
      int dstX = startX + x;
      if (dstX < 0 || dstX >= w)
        continue;

      int srcIdx = y * s_textureClipboard.width + x;
      int dstIdx = dstY * w + dstX;
      if (srcIdx < (int)s_textureClipboard.rawIndices.size() &&
          dstIdx < (int)raw.size()) {
        raw[dstIdx] = s_textureClipboard.rawIndices[srcIdx];
      }
    }
  }

  if (s_textureClipboard.isDirect && m_workingTexture.GetBpp() == 2) {
    auto &direct = m_workingTexture.GetDecoded().directPixels;
    for (int y = 0; y < s_textureClipboard.height; ++y) {
      int dstY = startY + y;
      if (dstY < 0 || dstY >= h)
        continue;
      for (int x = 0; x < s_textureClipboard.width; ++x) {
        int dstX = startX + x;
        if (dstX < 0 || dstX >= w)
          continue;

        int srcIdx = y * s_textureClipboard.width + x;
        int dstIdx = dstY * w + dstX;
        if (srcIdx < (int)s_textureClipboard.directPixels.size() &&
            dstIdx < (int)direct.size()) {
          direct[dstIdx] = s_textureClipboard.directPixels[srcIdx];
        }
      }
    }
  }

  // Set selection marquee to cover pasted region
  m_hasSelection = true;
  m_selMinX = startX;
  m_selMinY = startY;
  m_selMaxX = std::clamp(startX + s_textureClipboard.width - 1, 0, w - 1);
  m_selMaxY = std::clamp(startY + s_textureClipboard.height - 1, 0, h - 1);

  m_workingTexture.ApplyPalette(m_currentPalette);
  m_isDirty = true;
}

void TextureEditPanel::RevertSelection() {
  if (m_isReadOnly || !m_hasSelection)
    return;
  int w = m_workingTexture.GetWidth();
  int h = m_workingTexture.GetHeight();
  if (w <= 0 || h <= 0)
    return;

  m_undoBuffer.Push(m_workingTexture.GetDecoded(), m_currentPalette,
                    "Revert Selection");

  auto &raw = m_workingTexture.GetRawIndices();
  const auto &origRaw = m_originalTim.rawIndices;
  for (int y = m_selMinY; y <= m_selMaxY; ++y) {
    for (int x = m_selMinX; x <= m_selMaxX; ++x) {
      int idx = y * w + x;
      if (idx >= 0 && idx < (int)raw.size() && idx < (int)origRaw.size()) {
        raw[idx] = origRaw[idx];
      }
    }
  }

  if (m_workingTexture.GetBpp() == 2) {
    auto &direct = m_workingTexture.GetDecoded().directPixels;
    const auto &origDirect = m_originalTim.directPixels;
    for (int y = m_selMinY; y <= m_selMaxY; ++y) {
      for (int x = m_selMinX; x <= m_selMaxX; ++x) {
        int idx = y * w + x;
        if (idx >= 0 && idx < (int)direct.size() &&
            idx < (int)origDirect.size()) {
          direct[idx] = origDirect[idx];
        }
      }
    }
  }

  m_workingTexture.ApplyPalette(m_currentPalette);
  m_isDirty = true;
}

void TextureEditPanel::FillSelectionWithColor(uint8_t colorIdx) {
  if (m_isReadOnly || !m_hasSelection)
    return;
  m_undoBuffer.Push(m_workingTexture.GetDecoded(), m_currentPalette,
                    (colorIdx == 0) ? "Clear Selection" : "Fill Selection");
  TextureEditTools::FillSelection(m_workingTexture, m_currentPalette, colorIdx,
                                 m_selMinX, m_selMinY, m_selMaxX, m_selMaxY,
                                 m_isReadOnly);
  m_isDirty = true;
}

void TextureEditPanel::FlipSelectionH() {
  if (m_isReadOnly || !m_hasSelection)
    return;
  int w = m_workingTexture.GetWidth();
  int h = m_workingTexture.GetHeight();
  if (w <= 0 || h <= 0)
    return;

  m_undoBuffer.Push(m_workingTexture.GetDecoded(), m_currentPalette,
                    "Flip Horizontal");

  auto &raw = m_workingTexture.GetRawIndices();
  int selW = m_selMaxX - m_selMinX + 1;

  for (int y = m_selMinY; y <= m_selMaxY; ++y) {
    for (int x = 0; x < selW / 2; ++x) {
      int x1 = m_selMinX + x;
      int x2 = m_selMaxX - x;
      int idx1 = y * w + x1;
      int idx2 = y * w + x2;
      if (idx1 >= 0 && idx1 < (int)raw.size() && idx2 >= 0 &&
          idx2 < (int)raw.size()) {
        std::swap(raw[idx1], raw[idx2]);
      }
    }
  }

  if (m_workingTexture.GetBpp() == 2) {
    auto &direct = m_workingTexture.GetDecoded().directPixels;
    for (int y = m_selMinY; y <= m_selMaxY; ++y) {
      for (int x = 0; x < selW / 2; ++x) {
        int x1 = m_selMinX + x;
        int x2 = m_selMaxX - x;
        int idx1 = y * w + x1;
        int idx2 = y * w + x2;
        if (idx1 >= 0 && idx1 < (int)direct.size() && idx2 >= 0 &&
            idx2 < (int)direct.size()) {
          std::swap(direct[idx1], direct[idx2]);
        }
      }
    }
  }

  m_workingTexture.ApplyPalette(m_currentPalette);
  m_isDirty = true;
}

void TextureEditPanel::FlipSelectionV() {
  if (m_isReadOnly || !m_hasSelection)
    return;
  int w = m_workingTexture.GetWidth();
  int h = m_workingTexture.GetHeight();
  if (w <= 0 || h <= 0)
    return;

  m_undoBuffer.Push(m_workingTexture.GetDecoded(), m_currentPalette,
                    "Flip Vertical");

  auto &raw = m_workingTexture.GetRawIndices();
  int selH = m_selMaxY - m_selMinY + 1;

  for (int y = 0; y < selH / 2; ++y) {
    int y1 = m_selMinY + y;
    int y2 = m_selMaxY - y;
    for (int x = m_selMinX; x <= m_selMaxX; ++x) {
      int idx1 = y1 * w + x;
      int idx2 = y2 * w + x;
      if (idx1 >= 0 && idx1 < (int)raw.size() && idx2 >= 0 &&
          idx2 < (int)raw.size()) {
        std::swap(raw[idx1], raw[idx2]);
      }
    }
  }

  if (m_workingTexture.GetBpp() == 2) {
    auto &direct = m_workingTexture.GetDecoded().directPixels;
    for (int y = 0; y < selH / 2; ++y) {
      int y1 = m_selMinY + y;
      int y2 = m_selMaxY - y;
      for (int x = m_selMinX; x <= m_selMaxX; ++x) {
        int idx1 = y1 * w + x;
        int idx2 = y2 * w + x;
        if (idx1 >= 0 && idx1 < (int)direct.size() && idx2 >= 0 &&
            idx2 < (int)direct.size()) {
          std::swap(direct[idx1], direct[idx2]);
        }
      }
    }
  }

  m_workingTexture.ApplyPalette(m_currentPalette);
  m_isDirty = true;
}

void TextureEditPanel::SelectAll() {
  int w = m_workingTexture.GetWidth();
  int h = m_workingTexture.GetHeight();
  if (w <= 0 || h <= 0)
    return;
  m_hasSelection = true;
  m_selMinX = 0;
  m_selMinY = 0;
  m_selMaxX = w - 1;
  m_selMaxY = h - 1;
}

void TextureEditPanel::Deselect() {
  m_hasSelection = false;
}

void TextureEditPanel::DrawSelectionContextMenu() {
  if (ImGui::BeginPopup("TextureSelectionContextMenu")) {
    int selW = m_hasSelection ? (m_selMaxX - m_selMinX + 1) : 0;
    int selH = m_hasSelection ? (m_selMaxY - m_selMinY + 1) : 0;

    if (m_hasSelection) {
      ImGui::TextDisabled("Selection: %dx%d px (%d,%d to %d,%d)", selW, selH,
                          m_selMinX, m_selMinY, m_selMaxX, m_selMaxY);
      ImGui::Separator();
    }

    bool hasSel = m_hasSelection;
    bool canEdit = !m_isReadOnly;
    bool hasClip = (s_textureClipboard.width > 0 && s_textureClipboard.height > 0);

    if (ImGui::MenuItem(ICON_FA_SCISSORS " Cut", "Ctrl+X", false, hasSel && canEdit)) {
      CutSelection();
    }
    if (ImGui::MenuItem(ICON_FA_CLONE " Copy", "Ctrl+C", false, hasSel)) {
      CopySelection();
    }
    if (ImGui::MenuItem(ICON_FA_CLIPBOARD " Paste", "Ctrl+V", false, hasClip && canEdit)) {
      PasteClipboard();
    }

    ImGui::Separator();

    std::string fillLabel = "Fill with Color #" + std::to_string(m_selectedColorIdx);
    if (ImGui::MenuItem((ICON_FA_FILL_DRIP " " + fillLabel).c_str(), nullptr, false, hasSel && canEdit)) {
      FillSelectionWithColor((uint8_t)m_selectedColorIdx);
    }
    if (ImGui::MenuItem(ICON_FA_ERASER " Clear (Transparent)", "Del", false, hasSel && canEdit)) {
      FillSelectionWithColor(0);
    }
    if (ImGui::MenuItem(ICON_FA_CLOCK_ROTATE_LEFT " Revert Selection to Original", nullptr, false,
                        hasSel && canEdit && m_hasOriginalAsset)) {
      RevertSelection();
    }

    ImGui::Separator();

    if (ImGui::MenuItem(ICON_FA_ARROW_RIGHT_ARROW_LEFT " Flip Horizontal", nullptr, false, hasSel && canEdit)) {
      FlipSelectionH();
    }
    if (ImGui::MenuItem(ICON_FA_UP_DOWN " Flip Vertical", nullptr, false, hasSel && canEdit)) {
      FlipSelectionV();
    }

    ImGui::Separator();

    if (ImGui::MenuItem(ICON_FA_VECTOR_SQUARE " Select All", "Ctrl+A")) {
      SelectAll();
    }
    if (ImGui::MenuItem(ICON_FA_XMARK " Deselect", "Esc", false, hasSel)) {
      Deselect();
    }

    ImGui::EndPopup();
  }
}

void TextureEditPanel::HandleToolRectSelect(const ImGuiIO &io,
                                           const ImVec2 &mousePos,
                                           const ImVec2 &p0, int w, int h,
                                           int px, int py,
                                           bool isInsidePixel) {
  int gridStep = GetSelectionGridStep(io.KeyAlt, m_zoom);

  if (ImGui::IsMouseClicked(0)) {
    if (isInsidePixel) {
      m_isSelecting = true;
      m_selStartX = (px / gridStep) * gridStep;
      m_selStartY = (py / gridStep) * gridStep;
      m_selMinX = m_selStartX;
      m_selMaxX = std::clamp(m_selStartX + gridStep - 1, 0, w - 1);
      m_selMinY = m_selStartY;
      m_selMaxY = std::clamp(m_selStartY + gridStep - 1, 0, h - 1);
      m_hasSelection = true;
    } else {
      m_hasSelection = false;
    }
  }

  if (m_isSelecting && ImGui::IsMouseDown(0)) {
    float relX = (mousePos.x - p0.x) / m_zoom;
    float relY = (mousePos.y - p0.y) / m_zoom;
    int curPx = std::clamp((int)std::floor(relX), 0, w - 1);
    int curPy = std::clamp((int)std::floor(relY), 0, h - 1);

    int x0 = (std::min(m_selStartX, curPx) / gridStep) * gridStep;
    int x1 = ((std::max(m_selStartX, curPx) / gridStep) + 1) * gridStep - 1;
    int y0 = (std::min(m_selStartY, curPy) / gridStep) * gridStep;
    int y1 = ((std::max(m_selStartY, curPy) / gridStep) + 1) * gridStep - 1;

    m_selMinX = std::clamp(x0, 0, w - 1);
    m_selMaxX = std::clamp(x1, 0, w - 1);
    m_selMinY = std::clamp(y0, 0, h - 1);
    m_selMaxY = std::clamp(y1, 0, h - 1);
  }

  if (m_isSelecting && ImGui::IsMouseReleased(0)) {
    m_isSelecting = false;
  }
}

void TextureEditPanel::HandleToolPencilEraser(int w, int h, int px, int py,
                                             bool isInsidePixel) {
  if (ImGui::IsMouseClicked(0) && isInsidePixel) {
    m_isPaintingStroke = true;
    m_strokeModified = false;
    m_strokeStartSnapshot = m_workingTexture.GetDecoded();
    m_strokeDesc =
        (m_activeTool == TextureEditTool::Eraser) ? "Eraser" : "Pencil";
  }
  if (m_isPaintingStroke && ImGui::IsMouseDown(0) && isInsidePixel) {
    uint8_t col = (m_activeTool == TextureEditTool::Eraser)
                      ? 0
                      : (uint8_t)m_selectedColorIdx;
    const auto &raw = m_workingTexture.GetRawIndices();
    int idx = py * w + px;
    if (idx >= 0 && idx < (int)raw.size() && raw[idx] != col) {
      TextureEditTools::ApplyPencil(
          m_workingTexture, m_currentPalette, px, py, col, m_hasSelection,
          m_selMinX, m_selMinY, m_selMaxX, m_selMaxY, m_isReadOnly);
      m_isDirty = true;
      m_strokeModified = true;
    }
  }
}

void TextureEditPanel::HandleToolFillBucket(int w, int h, int px, int py,
                                           bool isInsidePixel) {
  if (ImGui::IsMouseClicked(0) && isInsidePixel) {
    const auto &raw = m_workingTexture.GetRawIndices();
    int startIdx = py * w + px;
    if (startIdx >= 0 && startIdx < (int)raw.size() &&
        raw[startIdx] != (uint8_t)m_selectedColorIdx) {
      m_undoBuffer.Push(m_workingTexture.GetDecoded(), m_currentPalette,
                        "Fill Bucket");
      TextureEditTools::ApplyFloodFill(
          m_workingTexture, m_currentPalette, px, py,
          (uint8_t)m_selectedColorIdx, m_hasSelection, m_selMinX, m_selMinY,
          m_selMaxX, m_selMaxY, m_isReadOnly);
      m_isDirty = true;
    }
  }
}

void TextureEditPanel::HandleToolEyedropper(int w, int h, int px, int py,
                                           bool isInsidePixel) {
  if (ImGui::IsMouseClicked(0) && isInsidePixel) {
    const auto &raw = m_workingTexture.GetRawIndices();
    int pIdx = py * w + px;
    if (pIdx >= 0 && pIdx < (int)raw.size()) {
      m_selectedColorIdx = raw[pIdx];
    }
  }
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
        SetEditingColor(color);
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
    auto &pals = m_workingTexture.GetPalettes();
    int palIdx = std::clamp(m_currentPalette, 0, (int)pals.size() - 1);

    if (palIdx < (int)pals.size()) {
      auto &pal = pals[palIdx];

      TIMColor origColor = {0, 0, 0, 255};
      bool hasOrig =
          (palIdx < (int)m_originalTim.palettes.size() &&
           m_editingColorIdx < (int)m_originalTim.palettes[palIdx].colors.size());
      if (hasOrig) {
        origColor = m_originalTim.palettes[palIdx].colors[m_editingColorIdx];
      }

      bool isColorDifferentFromOrig = hasOrig && (m_editingColor != origColor);

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
      ImGui::BeginDisabled(m_isReadOnly);

      ImGui::Text("PS1 15-bit Quantized (0-31):");
      colorChanged |= ImGui::SliderInt("Red (R5)", &m_editingR5, 0, 31);
      colorChanged |= ImGui::SliderInt("Green (G5)", &m_editingG5, 0, 31);
      colorChanged |= ImGui::SliderInt("Blue (B5)", &m_editingB5, 0, 31);
      colorChanged |= ImGui::Checkbox("Semi-Transparent (STP Bit)", &m_editingStp);

      if (ImGui::Button("Set as Transparent (0x0000)")) {
        SetEditingColor({0, 0, 0, 0});
        colorChanged = true;
      }

      ImGui::EndDisabled();

      if (colorChanged && !m_isReadOnly) {
        m_undoBuffer.Push(m_workingTexture.GetDecoded(), m_currentPalette,
                          "Edit Palette Color");
        m_editingColor = TIMColor::FromR5G5B5(m_editingR5, m_editingG5, m_editingB5, m_editingStp);

        if (m_editingColorIdx >= 0 && m_editingColorIdx < (int)pal.colors.size()) {
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

      auto DrawSwatch = [&](const char *label, const TIMColor &col, ImU32 borderCol) {
        ImGui::Text("%s: #%02X%02X%02X", label, col.r, col.g, col.b);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        drawList->AddRectFilled(pos, ImVec2(pos.x + pWidth, pos.y + pHeight),
                                IM_COL32(col.r, col.g, col.b, 255));
        drawList->AddRect(pos, ImVec2(pos.x + pWidth, pos.y + pHeight), borderCol);
        ImGui::Dummy(ImVec2(pWidth, pHeight + 4.0f));
      };

      // Current preview
      DrawSwatch("Current", m_editingColor, IM_COL32(255, 255, 255, 255));

      if (hasOrig) {
        ImGui::SameLine(pWidth + 24.0f);
        ImGui::BeginGroup();
        DrawSwatch("Original", origColor, IM_COL32(180, 180, 180, 255));
        ImGui::EndGroup();
      }

      // Revert individual color button
      bool canRevertColor = !m_isReadOnly && isColorDifferentFromOrig;
      ImGui::BeginDisabled(!canRevertColor);
      if (ImGui::Button(ICON_FA_CLOCK_ROTATE_LEFT " Revert Color to Original")) {
        m_undoBuffer.Push(m_workingTexture.GetDecoded(), m_currentPalette,
                          "Revert Color");
        SetEditingColor(origColor);
        if (m_editingColorIdx >= 0 && m_editingColorIdx < (int)pal.colors.size()) {
          pal.colors[m_editingColorIdx] = m_editingColor;
          m_workingTexture.ApplyPalette(m_currentPalette);
          m_isDirty = true;
        }
      }
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
  if (!m_isOpen) {
    m_isFocused = false;
    return;
  }

  // Dynamically recheck workspace status if workspace directory changed
  const std::string &wsDir = fileManager.GetWorkspaceDir();
  if (wsDir != m_lastCheckedWorkspaceDir) {
    m_isReadOnly = !IsWorkspacePath(m_timPath, wsDir);
    m_lastCheckedWorkspaceDir = wsDir;
  }

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
    m_isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    // Handle shortcuts inside Texture Editor window
    ImGuiIO &io = ImGui::GetIO();
    if (m_isFocused) {
      if (!m_isReadOnly) {
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
          Save(fileManager, activeMapTexture, currentMapPalette,
               localGeometryOverlay, sceneViewport);
        }
        if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)) {
          Undo();
        }
        if ((io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) ||
            (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z))) {
          Redo();
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
          CutSelection();
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V)) {
          PasteClipboard();
        }
      }
      if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
        CopySelection();
      }
      if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A)) {
        SelectAll();
      }
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

    TextureEditTools::DrawSidePanel(
        m_activeTool, m_selectedColorIdx, m_editingColorIdx, m_editingColor,
        m_editingR5, m_editingG5, m_editingB5, m_editingStp, m_workingTexture,
        m_currentPalette, m_texName, canvasH, m_isReadOnly,
        [&]() {
          m_undoBuffer.Push(m_workingTexture.GetDecoded(), m_currentPalette,
                            "Import PNG");
          m_isDirty = true;
        });

    ImGui::SameLine();
    DrawCanvas(canvasW, canvasH);
    DrawClutEditor();
    DrawColorPickerPopup();
  } else {
    m_isFocused = false;
  }
  ImGui::End();
}
