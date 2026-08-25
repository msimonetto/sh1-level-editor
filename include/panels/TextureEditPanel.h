#pragma once
#include "core/TextureUndoBuffer.h"
#include "core/Textures.h"
#include "formats/Structs.h"
#include "formats/TIMDecoder.h"
#include "formats/TIMEncoder.h"
#include "panels/TextureEditTools.h"
#include "panels/TextureWidgets.h"
#include "imgui.h"
#include <cmath>
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
  void Open(const std::string &timPath, FileManager &fileManager, int initialPaletteRow = 0);
  void Open(const std::string &timPath, bool isReadOnly = false, int initialPaletteRow = 0,
            const std::string &assetTimPath = "");
  void Close();
  bool IsOpen() const { return m_isOpen; }
  const std::string &GetTimPath() const { return m_timPath; }
  const std::string &GetTexName() const { return m_texName; }
  bool IsReadOnly() const { return m_isReadOnly; }
  bool HasOriginalAsset() const { return m_hasOriginalAsset; }
  bool IsDifferentFromOriginal() const;
  bool IsFocused() const { return m_isFocused; }

  // Undo / Redo
  bool Undo();
  bool Redo();
  bool CanUndo() const { return m_undoBuffer.CanUndo(); }
  bool CanRedo() const { return m_undoBuffer.CanRedo(); }
  const std::string &PeekUndoDesc() const { return m_undoBuffer.PeekUndoDesc(); }
  const std::string &PeekRedoDesc() const { return m_undoBuffer.PeekRedoDesc(); }

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
  bool m_hasOriginalAsset = false;
  bool m_focusRequested = false;
  bool m_isFocused = false;
  std::string m_timPath;
  std::string m_texName;
  int m_currentPalette = 0;
  int m_selectedColorIdx = 0;
  TextureEditTool m_activeTool = TextureEditTool::Pencil;

  // Undo buffer
  TextureUndoBuffer m_undoBuffer;

  // Continuous stroke state (Pencil / Eraser)
  bool m_isPaintingStroke = false;
  DecodedTIM m_strokeStartSnapshot;
  bool m_strokeModified = false;
  std::string m_strokeDesc;

  // Selection marquee state
  bool m_hasSelection = false;
  bool m_isSelecting = false;
  int m_selStartX = 0;
  int m_selStartY = 0;
  int m_selMinX = 0;
  int m_selMinY = 0;
  int m_selMaxX = 0;
  int m_selMaxY = 0;

  // Canvas navigation state
  float m_zoom = 2.0f; // Continuous exponential zoom scale
  ImVec2 m_panOffset = ImVec2(0.0f, 0.0f);
  bool m_showPixelGrid = true;
  bool m_showTileGrid = true;
  bool m_shouldAutoFit = false;
  std::string m_lastCheckedWorkspaceDir;

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

  void SetEditingColor(const TIMColor &c);

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
  void DrawCanvas(float canvasW, float canvasH);
  void DrawClutEditor();
  void DrawColorPickerPopup();
  void DrawSelectionContextMenu();

  // Selection operations
  void CutSelection();
  void CopySelection();
  void PasteClipboard();
  void RevertSelection();
  void FillSelectionWithColor(uint8_t colorIdx);
  void FlipSelectionH();
  void FlipSelectionV();
  void SelectAll();
  void Deselect();

  // Modular tool interaction handlers
  void HandleToolRectSelect(const ImGuiIO &io, const ImVec2 &mousePos,
                           const ImVec2 &p0, int w, int h, int px, int py,
                           bool isInsidePixel);
  void HandleToolPencilEraser(int w, int h, int px, int py, bool isInsidePixel);
  void HandleToolFillBucket(int w, int h, int px, int py, bool isInsidePixel);
  void HandleToolEyedropper(int w, int h, int px, int py, bool isInsidePixel);
};