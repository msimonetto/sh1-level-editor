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

class PaletteInspectorWidget {
public:
  // Draws texture dimensions, CLUT row stepper buttons, CLUT row combo with color bars,
  // and the interactive hoverable color swatch palette bar.
  static bool Draw(
      Textures &activeTexture, int &currentPalette,
      const std::function<void(int newPalette)> &onPaletteChanged = nullptr,
      const std::function<void(int colorIdx, TIMColor color)> &onColorSelected = nullptr,
      bool showDimensions = true);
};
