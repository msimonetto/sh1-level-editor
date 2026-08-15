#include <filesystem>
#include <map>
#include <string>

#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"
#include "imgui_internal.h"

#include "core/ChunkManager.h"
#include "core/Config.h"
#include "core/Dictionary.h"
#include "core/History.h"
#include "core/Shortcuts.h"
#include "core/Textures.h"
#include "core/DependencyManager.h"
#include "extras/IconsFontAwesome6.h"
#include "panels/Chunks.h"
#include "panels/Dependencies.h"
#include "panels/TextureMap.h"
#include "panels/ViewportTools.h"
#include "panels/Maps.h"
#include "panels/Outliner.h"
#include "panels/Settings.h"
#include "panels/Menu.h"
#include "viewport/Collision.h"
#include "viewport/GlobalGeometry.h"
#include "viewport/LocalGeometry.h"
#include "viewport/Viewport.h"
#include "viewport/Sync.h"
#include "viewport/Waypoints.h"

int main(int argc, char **argv) {
  // Initialize Raylib window
  const int screenWidth = 1280;
  const int screenHeight = 800;

  // Load Config
  Config::Get().Load();

  // Suppress verbose Raylib GPU VAO/VBO/FBO info logs that cause console I/O
  // lag
  SetTraceLogLevel(LOG_WARNING);

  SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(screenWidth, screenHeight, "Silent Hill Level Editor");
  SetExitKey(KEY_NULL); // Disable Escape closing the window;
                        // LocalGeometryOverlay uses Escape to deselect
  SetTargetFPS(60);

  // Add program icon (SHLE256.png)
  // Need to pare this down!
  const char *appDir = GetApplicationDirectory();
  Image icon = LoadImage(TextFormat("%s../../res/SHLE256.png", appDir));
  if (icon.data == nullptr) {
    icon = LoadImage(TextFormat("%s../res/SHLE256.png", appDir));
  }
  if (icon.data == nullptr) {
    icon = LoadImage(TextFormat("%sres/SHLE256.png", appDir));
  }

  if (icon.data != nullptr) {
    SetWindowIcon(icon);
    UnloadImage(icon);
  }

  // Initialize rlImGui
  rlImGuiSetup(true); // true enables dark theme by default

  // Enable Docking
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  if (Config::Get().IsHiDPI) {
    ImGui::GetIO().FontGlobalScale = 2.0f;
    ImGui::GetStyle().ScaleAllSizes(2.0f);
  }

  Textures testTexture;
  Config::Get().LastTexturePath = ""; // User requested empty panel on startup
  Config::Get().Save(); // Remove from config.ini on startup

  int currentPalette = 0;

  ChunkManager pipelineManager;
  Dictionary dictionary;
  DependencyManager dependencyManager(pipelineManager.GetWorkspaceDir());
  History history(50); // Global undo/redo, depth configurable from UI

  Viewport viewport;
  CollisionOverlay collisionOverlay;
  LocalGeometryOverlay localGeometryOverlay;
  WaypointsOverlay eventOverlay;
  
  viewport.SetOverlay(ViewportMode::Collision, &collisionOverlay);
  viewport.SetOverlay(ViewportMode::DoorsAndWaypoints, &eventOverlay);
  viewport.SetOverlay(ViewportMode::LocalGeometry, &localGeometryOverlay);
  eventOverlay.SetSharedChunks(&viewport.GetChunks());
  
  OutlinerPanel sceneOutliner;
  TextureMapPanel textureWindow;
  ViewportSync viewportSync;
  viewportSync.Initialize(viewport, localGeometryOverlay);

  // Global Geometry panel (full implementation)
  GlobalGeometryViewport globalGeometryPanel;

  // Maps panel (full implementation)
  MapsPanel mapsPanel;

  DependenciesPanel dependenciesPanel(pipelineManager);

  Shortcuts shortcuts;
  SettingsPanel settingsWindow;
  ViewportToolsPanel viewportToolsPanel;

  localGeometryOverlay.m_history = &history;
  localGeometryOverlay.m_texManager = &textureWindow;

  // Pass workspace directory so GlobalGeometryViewport and LocalGeometryOverlay
  // can find dependencies.json and .IPD / _GLB.PLM files.
  globalGeometryPanel.SetWorkspaceDir(pipelineManager.GetWorkspaceDir());
  localGeometryOverlay.m_lastWorkspaceDir = pipelineManager.GetWorkspaceDir();

  // By default, hook up the tool panel to the edit viewport as a stub
  viewportToolsPanel.SetActiveViewport(&viewport);

  // Restore last selected map overlay from config on startup
  std::string startMap =
      Config::Get().LastMapKey.empty() ? "MAP0_S00" : Config::Get().LastMapKey;
  eventOverlay.LoadOverlay(startMap);
  eventOverlay.FocusOnOverlay(viewport);
  mapsPanel.SetSelectedMapKey(startMap);

  auto legendColorCb = [&](const std::string &name) {
    static std::map<std::string, std::pair<bool, bool>> cache;
    static double lastUpdate = 0;
    if (GetTime() - lastUpdate > 1.0) {
      cache.clear();
      lastUpdate = GetTime();
    }
    if (cache.find(name) == cache.end()) {
      bool ext = std::filesystem::exists(pipelineManager.GetWorkspaceDir() +
                                         "/chunks/" + name + ".IPD");
      bool dep = std::filesystem::exists(pipelineManager.GetOverrideDir() +
                                         "/BG/" + name + ".IPD");
      cache[name] = {ext, dep};
    }
    bool ext = cache[name].first;
    bool dep = cache[name].second;
    if (dep)
      return Config::Get().ColorDeployment;
    if (ext)
      return Config::Get().ColorWorkspace;
    return Config::Get().ColorUnloaded;
  };
  viewport.m_legendColorCallback = legendColorCb;
  localGeometryOverlay.m_legendColorCallback = legendColorCb;

  // Restore state from config
  pipelineManager.SetSelectedChunks(Config::Get().ParseStringList(Config::Get().PersistedSelection));
  std::vector<std::string> viewportChunks = Config::Get().ParseStringList(Config::Get().PersistedViewportChunks);
  pipelineManager.SetViewportChunks(viewportChunks);
  pipelineManager.QueueReloadChunks(viewportChunks);

  ViewportCameraState camState;
  camState.azimuth = Config::Get().PersistedCamAzimuth;
  camState.elevation = Config::Get().PersistedCamElevation;
  camState.distance = Config::Get().PersistedCamDistance;
  camState.target = Config::Get().PersistedCamTarget;
  camState.projMode = ProjectionMode::Perspective;
  viewport.SetCameraState(camState);

  viewportToolsPanel.SetActiveMode((ViewportMode)Config::Get().PersistedToolsTab);

  // Main game loop
  while (!WindowShouldClose()) {
    // Update
    //----------------------------------------------------------------------------------

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();
    ClearBackground(DARKGRAY);

    // Start ImGui frame
    rlImGuiBegin();

    // Default tab selections on first few frames
    static int startupFrames = 2;
    if (startupFrames == 2) {
      ImGui::SetWindowFocus(ICON_FA_VIDEO " Viewport");
      startupFrames--;
    } else if (startupFrames == 1) {
      ImGui::SetWindowFocus(ICON_FA_CUBES " Chunks");
      startupFrames--;
    }

    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent
    // window not dockable into, because it would be confusing to have two
    // docking targets within each others.
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGuiViewport *mainViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(mainViewport->WorkPos);
    ImGui::SetNextWindowSize(mainViewport->WorkSize);
    ImGui::SetNextWindowViewport(mainViewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |=
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will
    // render our background and handle the pass-thru hole, so we ask Begin() to
    // not render a background.
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
      window_flags |= ImGuiWindowFlags_NoBackground;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace Demo", nullptr, window_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);

    // Submit the DockSpace
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
      ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");

      if (ImGui::DockBuilderGetNode(dockspace_id) == NULL) {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, mainViewport->Size);

        ImGuiID dock_main_id = dockspace_id;
        ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(
            dock_main_id, ImGuiDir_Left, 0.20f, NULL, &dock_main_id);
        ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(
            dock_main_id, ImGuiDir_Right, 0.25f, NULL, &dock_main_id);
        ImGuiID dock_bottom_id = ImGui::DockBuilderSplitNode(
            dock_main_id, ImGuiDir_Down, 0.20f, NULL, &dock_main_id);
        ImGuiID dock_right_down_id = ImGui::DockBuilderSplitNode(
            dock_right_id, ImGuiDir_Down, 0.50f, NULL, &dock_right_id);

        ImGui::DockBuilderDockWindow(ICON_FA_CUBES " Chunks", dock_left_id);
        ImGui::DockBuilderDockWindow(ICON_FA_SITEMAP " Dependencies", dock_left_id);
        ImGui::DockBuilderDockWindow(ICON_FA_MAP " Maps", dock_left_id);

        ImGui::DockBuilderDockWindow(ICON_FA_VIDEO " Viewport", dock_main_id);

        ImGui::DockBuilderDockWindow(ICON_FA_SHAPES " Global Geometry", dock_main_id);

        ImGui::DockBuilderDockWindow(ICON_FA_TERMINAL " Console", dock_bottom_id);

        ImGui::DockBuilderDockWindow(ICON_FA_LIST_UL " Outliner", dock_right_id);
        ImGui::DockBuilderDockWindow(ICON_FA_PAINTBRUSH " Texture Map", dock_right_id);

        ImGui::DockBuilderDockWindow(ICON_FA_WRENCH " Tools", dock_right_down_id);

        ImGui::DockBuilderFinish(dockspace_id);
      }

      ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f),
                       ImGuiDockNodeFlags_None);
    }

    if (!MenuPanel::Draw(shortcuts, pipelineManager, viewport, history, localGeometryOverlay, eventOverlay, settingsWindow)) {
      break;
    }

    ImGui::End();

    // Draw the Chunks panel (pipeline manager UI + console)
    ChunksPanel::Draw(pipelineManager, dictionary, dependencyManager, &history);

    dependenciesPanel.Render();

    for (const auto &chunk : pipelineManager.ConsumeReloadChunks()) {
      viewportSync.ForceReloadChunk(chunk, viewport,
                                    localGeometryOverlay);
    }

    for (const auto &chunk : localGeometryOverlay.ConsumeModifiedChunks()) {
      viewport.RebuildChunkBatches(chunk,
                                        pipelineManager.GetWorkspaceDir());
    }

    viewportSync.Update(pipelineManager, viewport,
                        localGeometryOverlay);
    shortcuts.Handle(history, pipelineManager, viewport,
                     localGeometryOverlay, &eventOverlay);

    // Show Viewports
    viewport.Draw();
    // LocalGeometryOverlay.Draw(); is now handled inside viewport.Draw() when in LocalGeometry mode
    sceneOutliner.Draw(viewport, &localGeometryOverlay);

    // Global Geometry panel
    globalGeometryPanel.Draw();

    // Maps panel (left tab)
    mapsPanel.Draw(&eventOverlay);

    // ----------------------------------------------------------------
    // Our Windows
    // ----------------------------------------------------------------

    textureWindow.Draw(testTexture, currentPalette, pipelineManager, dependencyManager,
                       viewport, localGeometryOverlay, history);

    viewport.SetActiveMode(viewportToolsPanel.GetActiveMode());
    viewportToolsPanel.SetActiveViewport(&viewport);

    viewportToolsPanel.Draw(&history);

    // ----------------------------------------------------------------
    // Loading Progress UI
    // ----------------------------------------------------------------
    int loadedChunks, totalChunks;
    if (viewportSync.GetLoadingProgress(loadedChunks, totalChunks)) {
      ImVec2 center = ImGui::GetMainViewport()->GetCenter();
      ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
      ImGuiWindowFlags flags =
          ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;

      // Push a modal-like background dimming
      ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.9f));
      if (ImGui::Begin("LoadingProgress", nullptr, flags)) {
        ImGui::Text("Loading chunks...");
        ImGui::Spacing();
        float progress = (totalChunks > 0)
                             ? ((float)loadedChunks / (float)totalChunks)
                             : 0.0f;
        ImGui::ProgressBar(progress, ImVec2(300.0f, 20.0f),
                           TextFormat("%d / %d", loadedChunks, totalChunks));
        ImGui::End();
      }
      ImGui::PopStyleColor();
    }

    settingsWindow.Draw(history);

    // End ImGui frame
    rlImGuiEnd();

    EndDrawing();
    //----------------------------------------------------------------------------------
  }

  // Save state to config
  Config::Get().PersistedSelection = Config::Get().StringListToString(pipelineManager.GetSelectedChunks());
  Config::Get().PersistedViewportChunks = Config::Get().StringListToString(pipelineManager.GetViewportChunks());
  
  ViewportCameraState currentCam = viewport.GetCameraState();
  Config::Get().PersistedCamAzimuth = currentCam.azimuth;
  Config::Get().PersistedCamElevation = currentCam.elevation;
  Config::Get().PersistedCamDistance = currentCam.distance;
  Config::Get().PersistedCamTarget = currentCam.target;
  
  Config::Get().PersistedToolsTab = (int)viewportToolsPanel.GetActiveMode();
  
  Config::Get().Save();

  // De-Initialization
  //--------------------------------------------------------------------------------------
  viewport.UnloadAll();
  collisionOverlay.UnloadAll();
  TextureCache::Get().UnloadAll();
  rlImGuiShutdown();
  CloseWindow();

  return 0;
}
