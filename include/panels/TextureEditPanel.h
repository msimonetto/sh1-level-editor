#pragma once
#include "core/Textures.h"
#include "formats/Structs.h"
#include "formats/TIMDecoder.h"
#include "formats/TIMEncoder.h"
#include "panels/TextureWidgets.h"
#include "imgui.h"
#include <cmath>
#include <string>
#include <vector>

class FileManager;
class LocalGeometryOverlay;
class Viewport;

enum class TextureEditTool {
  Pencil,       // Pixel Edit
  RectSelect,   // Rectangular Selection
  FillBucket,   // Flood Fill Bucket
  Eyedropper,   // Eyedropper Color Picker
  Eraser,       // Eraser
  ImportExport  // Import / Export stub
};

class TextureEditPanel {
public:
  TextureEditPanel();
  ~TextureEditPanel();

  // Summons the pop-out window for a specific TIM texture path and palette row
  void Open(const std::string &timPath, FileManager &fileManager, int initialPaletteRow = 0);
  void Open(const std::string &timPath, bool isReadOnly = false, int initialPaletteRow = 0);
  void Close();
  bool IsOpen() const { return m_isOpen; }
  const std::string &GetTimPath() const { return m_timPath; }
  const std::string &GetTexName() const { return m_texName; }
  bool IsReadOnly() const { return m_isReadOnly; }

  void Focus() { m_focusRequested = true; }
  void SetPalette(int paletteRow);

  // Selected color index in active palette
  int GetSelectedColorIdx() const { return m_selectedColorIdx; }
  void SetSelectedColorIdx(int idx) { m_selectedColorIdx = idx; }

  // Active editor tool
  TextureEditTool GetActiveTool() const { return m_activeTool; }
  void SetActiveTool(TextureEditTool tool) { m_activeTool = tool; }

  // Renders the pop-out window if open
  void Draw(FileManager &fileManager, Textures &activeMapTexture,
            int currentMapPalette, LocalGeometryOverlay &localGeometryOverlay,
            Viewport &sceneViewport);

private:
  bool m_isOpen = false;
  bool m_isDirty = false;
  bool m_isReadOnly = false;
  bool m_focusRequested = false;
  std::string m_timPath;
  std::string m_texName;
  int m_currentPalette = 0;
  int m_selectedColorIdx = 0;
  TextureEditTool m_activeTool = TextureEditTool::Pencil;

  // Canvas navigation state
  float m_zoom = 2.0f; // Continuous exponential zoom scale
  ImVec2 m_panOffset = ImVec2(0.0f, 0.0f);
  bool m_showPixelGrid = true;
  bool m_showTileGrid = true;
  bool m_shouldAutoFit = false;

  void SetZoom(float newZoom);

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
  void DrawSidePanel(float canvasH);
  void DrawCanvas(float canvasW, float canvasH);
  void DrawClutEditor();
  void DrawColorPickerPopup();
};