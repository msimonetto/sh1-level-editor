#pragma once
#include "core/Textures.h"
#include "formats/Structs.h"
#include "formats/TIMDecoder.h"
#include "formats/TIMEncoder.h"
#include "panels/TextureWidgets.h"
#include "imgui.h"
#include "raylib.h"
#include <string>
#include <vector>

class FileManager;
class LocalGeometryOverlay;
class Viewport;

class TextureEditPanel {
public:
  TextureEditPanel();
  ~TextureEditPanel();

  // Summons the pop-out window for a specific TIM texture path and palette row
  void Open(const std::string &timPath, int initialPaletteRow = 0);
  void Close();
  bool IsOpen() const { return m_isOpen; }

  // Renders the pop-out window if open
  void Draw(FileManager &fileManager, Textures &activeMapTexture,
            int currentMapPalette, LocalGeometryOverlay &localGeometryOverlay,
            Viewport &sceneViewport);

private:
  bool m_isOpen = false;
  bool m_isDirty = false;
  std::string m_timPath;
  std::string m_texName;
  int m_currentPalette = 0;

  // Canvas navigation state
  float m_zoom = 2.0f; // 1x to 16x zoom
  ImVec2 m_panOffset = ImVec2(0.0f, 0.0f);
  bool m_showPixelGrid = true;

  // Working copy of texture data
  Textures m_workingTexture;
  DecodedTIM m_originalTim;

  // Color editing popup state
  int m_editingColorIdx = -1;
  TIMColor m_editingColor = {0, 0, 0, 255};
  int m_editingR5 = 0;
  int m_editingG5 = 0;
  int m_editingB5 = 0;
  bool m_editingStp = false;

  void Save(FileManager &fileManager, Textures &activeMapTexture,
            int currentMapPalette,
            LocalGeometryOverlay &localGeometryOverlay,
            Viewport &sceneViewport);
  void Revert();

  void DrawHeader();
  void DrawToolbar(FileManager &fileManager, Textures &activeMapTexture,
                   int currentMapPalette,
                   LocalGeometryOverlay &localGeometryOverlay,
                   Viewport &sceneViewport);
  void DrawCanvas();
  void DrawClutEditor();
  void DrawColorPickerPopup();
};