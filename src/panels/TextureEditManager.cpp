#include "panels/TextureEditManager.h"
#include <algorithm>
#include <filesystem>

static std::string NormalizePathKey(const std::string &path) {
  if (path.empty())
    return "";
  try {
    return std::filesystem::weakly_canonical(std::filesystem::path(path)).string();
  } catch (...) {
    return path;
  }
}

TextureEditManager::TextureEditManager() = default;
TextureEditManager::~TextureEditManager() = default;

void TextureEditManager::Open(const std::string &timPath, FileManager &fileManager,
                              int initialPaletteRow) {
  if (timPath.empty())
    return;

  std::string normKey = NormalizePathKey(timPath);

  // Check if a window is already open for this exact texture file
  for (auto &panel : m_panels) {
    if (panel && panel->IsOpen() &&
        NormalizePathKey(panel->GetTimPath()) == normKey) {
      panel->Focus();
      panel->SetPalette(initialPaletteRow);
      return;
    }
  }

  // Create new dynamic window instance
  auto newPanel = std::make_unique<TextureEditPanel>();
  newPanel->Open(timPath, fileManager, initialPaletteRow);
  m_panels.push_back(std::move(newPanel));
}

void TextureEditManager::Open(const std::string &timPath, bool isReadOnly,
                              int initialPaletteRow) {
  if (timPath.empty())
    return;

  std::string normKey = NormalizePathKey(timPath);

  // Check if a window is already open for this exact texture file
  for (auto &panel : m_panels) {
    if (panel && panel->IsOpen() &&
        NormalizePathKey(panel->GetTimPath()) == normKey) {
      panel->Focus();
      panel->SetPalette(initialPaletteRow);
      return;
    }
  }

  // Create new dynamic window instance
  auto newPanel = std::make_unique<TextureEditPanel>();
  newPanel->Open(timPath, isReadOnly, initialPaletteRow);
  m_panels.push_back(std::move(newPanel));
}

void TextureEditManager::Draw(FileManager &fileManager,
                              Textures &activeMapTexture,
                              int currentMapPalette,
                              LocalGeometryOverlay &localGeometryOverlay,
                              Viewport &sceneViewport) {
  for (auto &panel : m_panels) {
    if (panel && panel->IsOpen()) {
      panel->Draw(fileManager, activeMapTexture, currentMapPalette,
                  localGeometryOverlay, sceneViewport);
    }
  }

  // Prune closed panels dynamically
  m_panels.erase(
      std::remove_if(m_panels.begin(), m_panels.end(),
                     [](const std::unique_ptr<TextureEditPanel> &p) {
                       return !p || !p->IsOpen();
                     }),
      m_panels.end());
}

void TextureEditManager::CloseAll() {
  for (auto &panel : m_panels) {
    if (panel) {
      panel->Close();
    }
  }
  m_panels.clear();
}

bool TextureEditManager::IsOpen(const std::string &timPath) const {
  std::string normKey = NormalizePathKey(timPath);
  for (const auto &panel : m_panels) {
    if (panel && panel->IsOpen() &&
        NormalizePathKey(panel->GetTimPath()) == normKey) {
      return true;
    }
  }
  return false;
}

bool TextureEditManager::HasFocusedPanel() const {
  for (const auto &panel : m_panels) {
    if (panel && panel->IsOpen() && panel->IsFocused()) {
      return true;
    }
  }
  return false;
}
