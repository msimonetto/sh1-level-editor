#pragma once
#include "core/FileManager.h"
#include "core/Textures.h"
#include "formats/IPDParse.h"
#include "imgui.h"
#include <functional>
#include <set>
#include <string>
#include <vector>

class DependencyManager;

enum class TextureFilterScope {
  Assets = 0,
  Workspace,
  SelectedChunks,
  CurrentChunk,
  CurrentMesh,
  Count
};

class TextureSelectorWidget {
public:
  TextureFilterScope GetFilterScope() const { return m_filterScope; }
  void SetFilterScope(TextureFilterScope scope) { m_filterScope = scope; }

  // Refreshes available texture list based on the active filter scope and scene state
  void RefreshAvailable(FileManager &fileManager,
                        DependencyManager &dependencyManager,
                        const std::string &currentSelChunk = "",
                        int currentSelObj = -1, int currentSelMesh = -1,
                        const std::vector<const ParsedChunk *> &chunks = {});

  // Draws the filter icon button and texture combo dropdown
  bool DrawCombo(FileManager &fileManager, const std::string &currentTexPath,
                 const std::function<void(const std::string &path,
                                          const std::string &texName)> &onSelect);

  // Draws the "From file:" input field and "Browse..." file dialog button
  bool DrawFromFile(const std::function<void(const std::string &path,
                                             const std::string &texName)> &onSelect);

  const std::vector<std::string> &GetCachedTextures() const {
    return cachedTextures;
  }

private:
  TextureFilterScope m_filterScope = TextureFilterScope::Workspace;
  TextureFilterScope lastFilterScope = TextureFilterScope::Count;
  std::string lastWorkspaceDirForTex;
  std::string lastAssetsDirForTex;
  std::string lastSelectedPrefixForTex;
  std::vector<std::string> lastSelectedChunksForTex;
  std::string lastSelChunk;
  int lastSelObj = -1;
  int lastSelMesh = -1;
  std::vector<std::string> cachedTextures;
  double lastTexRefreshTime = 0.0;
};

enum class PaletteWidgetLayout {
  Stacked, // Stacked across lines: "CLUT Row:" [◀] [Combo] [▶] on row 1, color bar on row 2 (TextureMapPanel)
  Inline   // Single line: "CLUT Row:" [◀] [Combo] [▶] [color bar] (TextureEditPanel)
};

class PaletteInspectorWidget {
public:
  // Draws texture dimensions, CLUT row stepper buttons, CLUT row combo with color bars,
  // and the interactive hoverable color swatch palette bar in either Stacked or Inline layout.
  static bool Draw(
      Textures &activeTexture, int &currentPalette,
      const std::function<void(int newPalette)> &onPaletteChanged = nullptr,
      const std::function<void(int colorIdx, TIMColor color)> &onColorSelected = nullptr,
      bool showDimensions = true,
      PaletteWidgetLayout layout = PaletteWidgetLayout::Stacked,
      const char *label = "Palette:",
      int selectedColorIdx = -1);
};

class TextureCanvasWidget {
public:
  // Draws dark checkered transparency background tiles
  static void DrawCheckeredBackground(ImDrawList *drawList, ImVec2 pos,
                                      ImVec2 size, float checkSize = 16.0f);

  // Converts a screen position to clamped texture pixel coordinates (px, py)
  static bool ScreenToPixelCoords(ImVec2 mousePos, ImVec2 imgPos, float scale,
                                  int texWidth, int texHeight,
                                  int &outPx, int &outPy);

  // Converts pixel coordinates (px, py) to tile coordinates [tx, ty] and flat tile index
  static void PixelToTileCoords(int px, int py, int &outTileX, int &outTileY,
                                int &outTileIdx, int texWidth = 256,
                                int tileSize = 32);

  // Snaps a floating-point pixel coordinate to the nearest 32px tile boundary floor
  static float SnapCoord32(float val);
};

class TextureGridWidget {
public:
  // Draws fine 1x1 pixel gridlines (typically when zoom >= 4x)
  static void DrawPixelGrid(ImDrawList *drawList, ImVec2 p0, ImVec2 p1,
                            int texW, int texH, float zoom,
                            bool skipTileLines = true,
                            ImU32 color = IM_COL32(255, 255, 255, 25));

  // Draws 32x32 pixel tile boundary gridlines
  static void DrawTileGrid(ImDrawList *drawList, ImVec2 p0, ImVec2 p1,
                           int texW, int texH, float zoom,
                           int tileSize = 32,
                           ImU32 color = IM_COL32(0, 220, 255, 140),
                           float thickness = 1.0f);
};

class TextureInspectorWidget {
public:
  // Renders a tooltip displaying pixel coords (X, Y) and 32x32 tile [X, Y] (#index)
  static void DrawPixelTileTooltip(int px, int py, int texWidth = 256,
                                   int tileSize = 32);
};
