#pragma once
#include "core/FileManager.h"
#include "core/Textures.h"
#include "panels/TextureEditPanel.h"
#include "viewport/LocalGeometryOverlay.h"
#include "viewport/Viewport.h"
#include <memory>
#include <string>
#include <vector>

class TextureEditManager {
public:
  TextureEditManager();
  ~TextureEditManager();

  // Opens or focuses a texture edit window for the specified TIM file
  void Open(const std::string &timPath, FileManager &fileManager, int initialPaletteRow = 0);
  void Open(const std::string &timPath, bool isReadOnly = false, int initialPaletteRow = 0);

  // Renders all open dynamic texture editor windows and prunes closed ones
  void Draw(FileManager &fileManager, Textures &activeMapTexture,
            int currentMapPalette, LocalGeometryOverlay &localGeometryOverlay,
            Viewport &sceneViewport);

  // Closes all open windows
  void CloseAll();

  // Returns number of active windows
  size_t GetOpenCount() const { return m_panels.size(); }

  // Checks if a window is currently open for the given path
  bool IsOpen(const std::string &timPath) const;

private:
  std::vector<std::unique_ptr<TextureEditPanel>> m_panels;
};
