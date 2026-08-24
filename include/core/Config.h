#pragma once
#include "raylib.h" // For Color struct and KEY_* definitions
#include <string>
#include <vector>

class Config {
public:
  static Config &Get();

  void Load();
  void Save();

  // Existing
  std::string LastTexturePath;
  std::string LastIPDPath;
  bool IsHiDPI = false;
  float TextureScale = 2.0f;

  // Directories
  std::string ProjectDirectory = "";
  std::string GameDirectory = "";
  std::string SelectedPrefix = "";
  std::string LastMapKey = "MAP0_S00";

  // Interface Colors
  Color ColorSelected = {253, 249, 0, 255}; // Yellow
  Color ColorWorkspace = {100, 80, 20, 40};
  Color ColorDeployment = {20, 80, 40, 40};
  Color ColorUnloaded = {43, 43, 43, 40};
  int UndoDepth = 50;

  // Viewport
  bool ShowMajorGridlines = true;
  bool ShowMinorGridlines = true;
  float GridCellSize = 30.0f; // For Chunk Manager (viewport chunk selector)
  bool EnableDitheringMode = false;

  // Wireframe
  bool ShowPersistentWireframe = true;
  Color WireframeColor = {255, 255, 255, 63}; // White
  float WireframeThickness = 1.0f;

  // Object/Selection Controls
  int KeyMoveForward = KEY_UP;
  int KeyMoveBackward = KEY_DOWN;
  int KeyMoveLeft = KEY_LEFT;
  int KeyMoveRight = KEY_RIGHT;
  int KeyMoveUp = KEY_PAGE_UP;
  int KeyMoveDown = KEY_PAGE_DOWN;
  int KeyMultiselect = KEY_LEFT_SHIFT;
  bool IsMultiselectDown() const;

  // Camera Controls
  int KeyCamMoveForward = KEY_W;
  int KeyCamMoveBackward = KEY_S;
  int KeyCamMoveLeft = KEY_A;
  int KeyCamMoveRight = KEY_D;
  int KeyCamMoveUp = KEY_R;
  int KeyCamMoveDown = KEY_F;

  // Session Persistence
  std::string PersistedSelection = "";
  std::string PersistedViewportChunks = "";
  float PersistedCamAzimuth = 45.0f;
  float PersistedCamElevation = 30.0f;
  float PersistedCamDistance = 35.0f;
  Vector3 PersistedCamTarget = {0.0f, 0.0f, 0.0f};
  int PersistedToolsTab = 0;
  std::string Vector3ToString(Vector3 v);
  Vector3 StringToVector3(const std::string &str);
  
  std::string StringListToString(const std::vector<std::string> &list);
  std::vector<std::string> ParseStringList(const std::string &str);

private:
  Config() = default;
  const std::string m_configPath = "config.ini";

  // Helper functions for parsing
  std::string ColorToString(Color c);
  Color StringToColor(const std::string &str);
};
