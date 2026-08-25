#pragma once
#include "core/Textures.h"
#include "formats/Structs.h"
#include "imgui.h"
#include <functional>
#include <string>

enum class TextureEditTool {
  Pencil,       // Pixel Edit (Pencil)
  RectSelect,   // Rectangular / Marquee Select
  FillBucket,   // Flood Fill Bucket
  Eyedropper,   // Eyedropper Color Picker
  Eraser,       // Eraser (Draw color #0)
  Import,       // Import from PNG
  Export        // Export to PNG
};

class TextureEditTools {
public:
  // Tool operation routines on working texture
  static void ApplyPencil(Textures &texture, int currentPalette, int px, int py,
                          uint8_t colorIdx, bool hasSelection = false,
                          int selMinX = 0, int selMinY = 0,
                          int selMaxX = 0, int selMaxY = 0,
                          bool isReadOnly = false);

  static void ApplyFloodFill(Textures &texture, int currentPalette,
                             int startX, int startY, uint8_t newColorIdx,
                             bool hasSelection = false,
                             int selMinX = 0, int selMinY = 0,
                             int selMaxX = 0, int selMaxY = 0,
                             bool isReadOnly = false);

  static void FillSelection(Textures &texture, int currentPalette,
                            uint8_t colorIdx, int selMinX, int selMinY,
                            int selMaxX, int selMaxY, bool isReadOnly = false);

  static bool ExportImageFile(Textures &texture, int currentPalette,
                              const std::string &filePath);

  static bool ImportImageFile(Textures &texture, int currentPalette,
                              const std::string &filePath, bool isReadOnly = false);

  // Renders the vertical square tool buttons side panel
  static void DrawSidePanel(TextureEditTool &activeTool, int &selectedColorIdx,
                            int &editingColorIdx, TIMColor &editingColor,
                            int &editingR5, int &editingG5, int &editingB5,
                            bool &editingStp, Textures &workingTexture,
                            int currentPalette, const std::string &texName,
                            float canvasH, bool isReadOnly,
                            std::function<void()> onBeforeImport = nullptr);

  // Renders marquee selection overlay on canvas
  static void DrawSelectionMarquee(ImDrawList *drawList, ImVec2 p0, float zoom,
                                   int selMinX, int selMinY, int selMaxX,
                                   int selMaxY);
};
