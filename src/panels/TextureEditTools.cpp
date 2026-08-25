#include "panels/TextureEditTools.h"
#include "core/FileDialog.h"
#include "extras/IconsFontAwesome6.h"
#include "raylib.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <queue>

void TextureEditTools::ApplyPencil(Textures &texture, int currentPalette,
                                   int px, int py, uint8_t colorIdx,
                                   bool hasSelection, int selMinX, int selMinY,
                                   int selMaxX, int selMaxY, bool isReadOnly) {
  if (isReadOnly)
    return;

  int w = texture.GetWidth();
  int h = texture.GetHeight();
  if (px < 0 || px >= w || py < 0 || py >= h)
    return;

  if (hasSelection &&
      (px < selMinX || px > selMaxX || py < selMinY || py > selMaxY))
    return;

  auto &raw = texture.GetRawIndices();
  if (raw.empty())
    return;

  int idx = py * w + px;
  if (idx < (int)raw.size() && raw[idx] != colorIdx) {
    raw[idx] = colorIdx;
    texture.ApplyPalette(currentPalette);
  }
}

void TextureEditTools::ApplyFloodFill(Textures &texture, int currentPalette,
                                      int startX, int startY,
                                      uint8_t newColorIdx, bool hasSelection,
                                      int selMinX, int selMinY, int selMaxX,
                                      int selMaxY, bool isReadOnly) {
  if (isReadOnly)
    return;

  int w = texture.GetWidth();
  int h = texture.GetHeight();
  if (startX < 0 || startX >= w || startY < 0 || startY >= h)
    return;

  auto &raw = texture.GetRawIndices();
  if (raw.empty())
    return;

  int startIdx = startY * w + startX;
  uint8_t targetIdx = raw[startIdx];
  if (targetIdx == newColorIdx)
    return;

  std::queue<std::pair<int, int>> q;
  q.push({startX, startY});
  raw[startIdx] = newColorIdx;

  const int dx[4] = {-1, 1, 0, 0};
  const int dy[4] = {0, 0, -1, 1};

  while (!q.empty()) {
    auto [cx, cy] = q.front();
    q.pop();

    for (int i = 0; i < 4; ++i) {
      int nx = cx + dx[i];
      int ny = cy + dy[i];

      if (nx < 0 || nx >= w || ny < 0 || ny >= h)
        continue;

      if (hasSelection &&
          (nx < selMinX || nx > selMaxX || ny < selMinY || ny > selMaxY))
        continue;

      int nIdx = ny * w + nx;
      if (raw[nIdx] == targetIdx) {
        raw[nIdx] = newColorIdx;
        q.push({nx, ny});
      }
    }
  }

  texture.ApplyPalette(currentPalette);
}

void TextureEditTools::FillSelection(Textures &texture, int currentPalette,
                                     uint8_t colorIdx, int selMinX, int selMinY,
                                     int selMaxX, int selMaxY, bool isReadOnly) {
  if (isReadOnly)
    return;

  int w = texture.GetWidth();
  int h = texture.GetHeight();
  auto &raw = texture.GetRawIndices();
  if (raw.empty() || w <= 0 || h <= 0)
    return;

  bool changed = false;
  for (int y = selMinY; y <= selMaxY; ++y) {
    for (int x = selMinX; x <= selMaxX; ++x) {
      int idx = y * w + x;
      if (idx >= 0 && idx < (int)raw.size()) {
        if (raw[idx] != colorIdx) {
          raw[idx] = colorIdx;
          changed = true;
        }
      }
    }
  }

  if (changed) {
    texture.ApplyPalette(currentPalette);
  }
}

bool TextureEditTools::ExportImageFile(Textures &texture, int currentPalette,
                                      const std::string &filePath) {
  if (filePath.empty())
    return false;

  int w = texture.GetWidth();
  int h = texture.GetHeight();
  if (w <= 0 || h <= 0)
    return false;

  std::vector<Color> rgbaPixels(w * h);
  if (texture.GetBpp() == 0 || texture.GetBpp() == 1) {
    const auto &raw = texture.GetRawIndices();
    const auto &pals = texture.GetPalettes();
    int palIdx = std::clamp(currentPalette, 0, (int)pals.size() - 1);

    for (int i = 0; i < w * h; ++i) {
      uint8_t cIdx = (i < (int)raw.size()) ? raw[i] : 0;
      TIMColor c =
          (palIdx < (int)pals.size() && cIdx < (int)pals[palIdx].colors.size())
              ? pals[palIdx].colors[cIdx]
              : TIMColor{0, 0, 0, 0};
      rgbaPixels[i] = Color{c.r, c.g, c.b, c.a};
    }
  } else {
    const auto &direct = texture.GetDecoded().directPixels;
    for (int i = 0; i < w * h; ++i) {
      TIMColor c =
          (i < (int)direct.size()) ? direct[i] : TIMColor{0, 0, 0, 255};
      rgbaPixels[i] = Color{c.r, c.g, c.b, c.a};
    }
  }

  Image img = {0};
  img.data = rgbaPixels.data();
  img.width = w;
  img.height = h;
  img.mipmaps = 1;
  img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

  bool success = ExportImage(img, filePath.c_str());
  if (success) {
    printf("[TextureEditTools] Successfully exported PNG image to: %s\n",
           filePath.c_str());
  } else {
    printf("[TextureEditTools] Failed to export PNG image to: %s\n",
           filePath.c_str());
  }
  return success;
}

bool TextureEditTools::ImportImageFile(Textures &texture, int currentPalette,
                                      const std::string &filePath,
                                      bool isReadOnly) {
  if (isReadOnly || filePath.empty())
    return false;

  Image img = LoadImage(filePath.c_str());
  if (img.data == nullptr) {
    printf("[TextureEditTools] Failed to load image: %s\n", filePath.c_str());
    return false;
  }

  ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  int w = texture.GetWidth();
  int h = texture.GetHeight();
  if (w <= 0 || h <= 0) {
    UnloadImage(img);
    return false;
  }

  if (img.width != w || img.height != h) {
    ImageResize(&img, w, h);
  }

  const auto &pals = texture.GetPalettes();
  if (pals.empty()) {
    UnloadImage(img);
    return false;
  }

  int palIdx = std::clamp(currentPalette, 0, (int)pals.size() - 1);
  const auto &paletteColors = pals[palIdx].colors;
  Color *srcPixels = (Color *)img.data;
  auto &raw = texture.GetRawIndices();
  raw.resize(w * h);

  for (int i = 0; i < w * h; ++i) {
    Color sc = srcPixels[i];
    if (sc.a < 128) {
      raw[i] = 0; // Transparent
    } else {
      int bestIdx = 0;
      int bestDist = INT_MAX;
      int r5 = sc.r * 31 / 255;
      int g5 = sc.g * 31 / 255;
      int b5 = sc.b * 31 / 255;

      for (size_t c = 0; c < paletteColors.size(); ++c) {
        const auto &pc = paletteColors[c];
        if (pc.a == 0 && (pc.r == 0 && pc.g == 0 && pc.b == 0))
          continue; // Skip transparent entry for solid colors
        int pr5 = pc.r * 31 / 255;
        int pg5 = pc.g * 31 / 255;
        int pb5 = pc.b * 31 / 255;
        int dr = r5 - pr5;
        int dg = g5 - pg5;
        int db = b5 - pb5;
        int dist = dr * dr + dg * dg + db * db;
        if (dist < bestDist) {
          bestDist = dist;
          bestIdx = (int)c;
        }
      }
      raw[i] = (uint8_t)bestIdx;
    }
  }

  UnloadImage(img);
  texture.ApplyPalette(currentPalette);
  printf("[TextureEditTools] Imported & quantized PNG from: %s\n",
         filePath.c_str());
  return true;
}

void TextureEditTools::DrawSidePanel(
    TextureEditTool &activeTool, int &selectedColorIdx, int &editingColorIdx,
    TIMColor &editingColor, int &editingR5, int &editingG5, int &editingB5,
    bool &editingStp, Textures &workingTexture, int currentPalette,
    const std::string &texName, float canvasH, bool isReadOnly) {
  float sidePanelW = 46.0f;
  ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(24, 24, 28, 255));
  ImGui::BeginChild("TextureEditToolsSidePanel", ImVec2(sidePanelW, canvasH),
                    true,
                    ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);

  auto DrawToolBtn = [&](TextureEditTool tool, const char *icon,
                         const char *name, const char *desc) {
    bool isActive = (activeTool == tool);
    if (isActive) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.45f, 0.75f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.86f, 1.0f, 1.0f));
    }
    if (ImGui::Button(icon, ImVec2(34.0f, 34.0f))) {
      activeTool = tool;
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

  if (isReadOnly)
    ImGui::BeginDisabled();
  if (ImGui::Button(ICON_FA_FILE_IMPORT, ImVec2(34.0f, 34.0f))) {
    std::string path =
        FileDialog::OpenFile("PNG Files\0*.png;*.PNG\0All Files\0*.*\0");
    if (!path.empty()) {
      ImportImageFile(workingTexture, currentPalette, path, isReadOnly);
    }
  }
  if (isReadOnly)
    ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::BeginTooltip();
    ImGui::Text("Import Image (PNG)");
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
                       "Load external PNG image and quantize to active 16-color CLUT.");
    ImGui::EndTooltip();
  }

  if (ImGui::Button(ICON_FA_FILE_EXPORT, ImVec2(34.0f, 34.0f))) {
    std::string defaultName = texName + ".png";
    std::string path =
        FileDialog::SaveFile("PNG Files\0*.png\0", defaultName.c_str());
    if (!path.empty()) {
      ExportImageFile(workingTexture, currentPalette, path);
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Export Image (PNG)");
    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
                       "Export current composite RGBA image with active CLUT applied.");
    ImGui::EndTooltip();
  }

  ImGui::Separator();

  // Active Color Swatch preview & quick color picker summon
  TIMColor activeCol = {0, 0, 0, 255};
  const auto &pals = workingTexture.GetPalettes();
  if (!pals.empty()) {
    int palIdx = std::clamp(currentPalette, 0, (int)pals.size() - 1);
    if (selectedColorIdx >= 0 &&
        selectedColorIdx < (int)pals[palIdx].colors.size()) {
      activeCol = pals[palIdx].colors[selectedColorIdx];
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
    editingColorIdx = selectedColorIdx;
    editingColor = activeCol;
    editingR5 = activeCol.r * 31 / 255;
    editingG5 = activeCol.g * 31 / 255;
    editingB5 = activeCol.b * 31 / 255;
    editingStp = (activeCol.a == 0)
                     ? false
                     : (activeCol.a < 255 || (activeCol.r == 0 &&
                                              activeCol.g == 0 &&
                                              activeCol.b == 0));
    ImGui::OpenPopup("EditClutColorPopup");
  }
  if (ImGui::IsItemHovered()) {
    ImGui::BeginTooltip();
    ImGui::Text("Selected Color: #%d (Palette %d)", selectedColorIdx,
                currentPalette);
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

void TextureEditTools::DrawSelectionMarquee(ImDrawList *drawList, ImVec2 p0,
                                           float zoom, int selMinX, int selMinY,
                                           int selMaxX, int selMaxY) {
  if (!drawList)
    return;
  ImVec2 s0(p0.x + (float)selMinX * zoom, p0.y + (float)selMinY * zoom);
  ImVec2 s1(p0.x + (float)(selMaxX + 1) * zoom,
            p0.y + (float)(selMaxY + 1) * zoom);
  drawList->AddRectFilled(s0, s1, IM_COL32(0, 220, 255, 45));
  drawList->AddRect(s0, s1, IM_COL32(0, 220, 255, 255), 0.0f, 0, 1.5f);
}
