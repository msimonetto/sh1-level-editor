#include "viewport/ViewportBase.h"
#include "core/Config.h"
#include "imgui.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

ViewportBase::ViewportBase(const std::string &panelName)
    : m_panelName(panelName) {
  m_renderTarget = {0};
  m_rtWidth = 0;
  m_rtHeight = 0;

  // Initialise camera from spherical coords
  m_azimuth = 45.0f;
  m_elevation = 30.0f;
  m_distance = 35.0f;

  m_camera = {0};
  m_camera.target = {0.0f, 0.0f, 0.0f};
  m_camera.up = {0.0f, 1.0f, 0.0f};
  m_camera.fovy = 60.0f;
  m_camera.projection = CAMERA_PERSPECTIVE;

  // Derive initial position
  UpdateCameraVectors();

  const char *appDir = GetApplicationDirectory();
  if (FileExists(TextFormat("%s../res/shaders/ps1_dither.fs", appDir))) {
    m_ditherShader =
        LoadShader(0, TextFormat("%s../res/shaders/ps1_dither.fs", appDir));
    m_shaderLoaded = true;
  } else if (FileExists(TextFormat("%sres/shaders/ps1_dither.fs", appDir))) {
    m_ditherShader =
        LoadShader(0, TextFormat("%sres/shaders/ps1_dither.fs", appDir));
    m_shaderLoaded = true;
  } else {
    if (FileExists("res/shaders/ps1_dither.fs")) {
      m_ditherShader = LoadShader(0, "res/shaders/ps1_dither.fs");
      m_shaderLoaded = true;
    } else {
      printf("[%s] Warning: ps1_dither.fs not found.\n", m_panelName.c_str());
      m_shaderLoaded = false;
    }
  }
}

void ViewportBase::ResetCamera() {
  m_projMode = ProjectionMode::Perspective;
  m_azimuth = 45.0f;
  m_elevation = 30.0f;
  m_distance = 35.0f;
  m_camera.target = {0.0f, 0.0f, 0.0f};
  m_camera.up = {0.0f, 1.0f, 0.0f};
  m_camera.fovy = 60.0f;
  m_camera.projection = CAMERA_PERSPECTIVE;
  UpdateCameraVectors();
}

void ViewportBase::UpdateCameraVectors() {
  if (m_projMode == ProjectionMode::Perspective) {
    m_camera.projection = CAMERA_PERSPECTIVE;
    m_camera.fovy = 60.0f;
    float azR = m_azimuth * DEG2RAD;
    float elR = m_elevation * DEG2RAD;
    m_camera.position.x =
        m_camera.target.x + m_distance * cosf(elR) * sinf(azR);
    m_camera.position.y = m_camera.target.y + m_distance * sinf(elR);
    m_camera.position.z =
        m_camera.target.z + m_distance * cosf(elR) * cosf(azR);
  } else {
    m_camera.projection = CAMERA_ORTHOGRAPHIC;
    m_camera.fovy = m_distance * 2.0f; // Scale ortho width by distance

    switch (m_projMode) {
    case ProjectionMode::OrthoTop:
      m_elevation = 89.9f;
      m_azimuth = 0.0f;
      break;
    case ProjectionMode::OrthoFront:
      m_elevation = 0.0f;
      m_azimuth = 0.0f;
      break;
    case ProjectionMode::OrthoBack:
      m_elevation = 0.0f;
      m_azimuth = 180.0f;
      break;
    case ProjectionMode::OrthoLeft:
      m_elevation = 0.0f;
      m_azimuth = -90.0f;
      break;
    case ProjectionMode::OrthoRight:
      m_elevation = 0.0f;
      m_azimuth = 90.0f;
      break;
    default:
      break;
    }

    float azR = m_azimuth * DEG2RAD;
    float elR = m_elevation * DEG2RAD;

    // Push the camera far away to ensure we don't clip geometry; fovy handles
    // zoom.
    m_camera.position.x = m_camera.target.x + 1000.0f * cosf(elR) * sinf(azR);
    m_camera.position.y = m_camera.target.y + 1000.0f * sinf(elR);
    m_camera.position.z = m_camera.target.z + 1000.0f * cosf(elR) * cosf(azR);
  }
}

ViewportBase::~ViewportBase() { Shutdown(); }

void ViewportBase::Shutdown() {
  OnUnloadAll();
  if (m_renderTarget.id != 0) {
    UnloadRenderTexture(m_renderTarget);
    m_renderTarget.id = 0;
  }
  if (m_postProcessTarget.id != 0) {
    UnloadRenderTexture(m_postProcessTarget);
    m_postProcessTarget.id = 0;
  }
  if (m_shaderLoaded) {
    UnloadShader(m_ditherShader);
    m_shaderLoaded = false;
  }
}

void ViewportBase::UnloadAll() { OnUnloadAll(); }

// ---------------------------------------------------------------------------
// Render target management
// ---------------------------------------------------------------------------

void ViewportBase::EnsureRenderTarget(int w, int h) {
  if (w <= 0)
    w = 1;
  if (h <= 0)
    h = 1;

  int targetW = w;
  int targetH = h;

  if (Config::Get().EnableDitheringMode) {
    float scale = 360.0f / (float)h;
    targetW = (int)(w * scale);
    targetH = 360;
    if (targetW <= 0)
      targetW = 1;
  }

  if (targetW == m_rtWidth && targetH == m_rtHeight)
    return;

  // Defer resizing render target while user is holding left mouse button (e.g.
  // dragging panel splitters)
  if (m_rtWidth > 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    return;

  if (m_renderTarget.id != 0) {
    UnloadRenderTexture(m_renderTarget);
  }
  if (m_postProcessTarget.id != 0) {
    UnloadRenderTexture(m_postProcessTarget);
  }
  m_renderTarget = LoadRenderTexture(targetW, targetH);
  m_postProcessTarget = LoadRenderTexture(targetW, targetH);
  m_rtWidth = targetW;
  m_rtHeight = targetH;
  UpdateCameraVectors();
  printf("[%s] Render target resized to %dx%d\n", m_panelName.c_str(), targetW,
         targetH);
}

// ---------------------------------------------------------------------------
// Camera update — only active when the ImGui panel is hovered
// ---------------------------------------------------------------------------

void ViewportBase::UpdateCamera() {
  // Manual orbit / pan / zoom camera.
  // All input gated on panel hover; uses ImGui mouse delta to avoid
  // stealing input from other panels.
  ImGuiIO &io = ImGui::GetIO();
  if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_None))
    return;

  const float ORBIT_SPEED = 0.4f; // degrees per pixel
  const float PAN_SPEED = 0.002f; // world-units per pixel per unit of distance
  const float ZOOM_SPEED = 0.12f; // fraction of distance per scroll tick
  const float MOVE_SPEED =
      2.0f * io.DeltaTime; // WASD speed (scaled by distance)

  float dx = io.MouseDelta.x;
  float dy = io.MouseDelta.y;
  bool isShift = io.KeyShift;
  bool isMidDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);

  // Rebuild vectors for movement/pan
  float az = m_azimuth * DEG2RAD;
  float el = m_elevation * DEG2RAD;
  Vector3 forward = {cosf(el) * sinf(az), sinf(el), cosf(el) * cosf(az)};
  Vector3 worldUp = {0.0f, 1.0f, 0.0f};
  Vector3 right = Vector3Normalize(Vector3CrossProduct(worldUp, forward));

  // --- Orbit & Pan: MMB held ---
  if (isMidDown && (dx != 0 || dy != 0)) {
    if (isShift) {
      // Shift + MMB = Pan (Blender style)
      Vector3 camForward =
          Vector3Normalize({m_camera.target.x - m_camera.position.x,
                            m_camera.target.y - m_camera.position.y,
                            m_camera.target.z - m_camera.position.z});
      Vector3 cUp = {0.0f, 1.0f, 0.0f};
      if (fabs(camForward.y) > 0.99f)
        cUp = {0.0f, 0.0f, -1.0f};
      Vector3 camRight = Vector3Normalize(Vector3CrossProduct(camForward, cUp));
      Vector3 camUp =
          Vector3Normalize(Vector3CrossProduct(camRight, camForward));

      float panScale = m_projMode == ProjectionMode::Perspective
                           ? PAN_SPEED
                           : (PAN_SPEED * 2.0f);
      m_camera.target.x -=
          (camRight.x * dx - camUp.x * dy) * m_distance * panScale;
      m_camera.target.y -=
          (camRight.y * dx - camUp.y * dy) * m_distance * panScale;
      m_camera.target.z -=
          (camRight.z * dx - camUp.z * dy) * m_distance * panScale;
    } else {
      // MMB only = Orbit. If in ortho mode, snap back to perspective (Blender
      // style)
      if (m_projMode != ProjectionMode::Perspective) {
        m_projMode = ProjectionMode::Perspective;
      }

      m_azimuth -= dx * ORBIT_SPEED; // Swapped left-right
      m_elevation -= dy * ORBIT_SPEED;
      if (m_elevation > 89.0f)
        m_elevation = 89.0f;
      if (m_elevation < -89.0f)
        m_elevation = -89.0f;
    }
  }

  // --- WASD + Space/Shift Movement ---
  float currentMoveSpeed = MOVE_SPEED * m_distance * m_moveSpeedMultiplier;

  if (m_projMode == ProjectionMode::Perspective) {
    Vector3 flatForward = Vector3Normalize({forward.x, 0.0f, forward.z});
    Vector3 flatRight =
        Vector3Normalize(Vector3CrossProduct(worldUp, flatForward));
    if (!io.KeyCtrl) {
      if (IsKeyDown(Config::Get().KeyCamMoveForward)) {
        m_camera.target.x -= flatForward.x * currentMoveSpeed;
        m_camera.target.z -= flatForward.z * currentMoveSpeed;
      }
      if (IsKeyDown(Config::Get().KeyCamMoveBackward)) {
        m_camera.target.x += flatForward.x * currentMoveSpeed;
        m_camera.target.z += flatForward.z * currentMoveSpeed;
      }
      if (IsKeyDown(Config::Get().KeyCamMoveLeft)) {
        m_camera.target.x -= flatRight.x * currentMoveSpeed;
        m_camera.target.z -= flatRight.z * currentMoveSpeed;
      }
      if (IsKeyDown(Config::Get().KeyCamMoveRight)) {
        m_camera.target.x += flatRight.x * currentMoveSpeed;
        m_camera.target.z += flatRight.z * currentMoveSpeed;
      }
    }
    float verticalMoveSpeed = currentMoveSpeed * 0.5f;
    if (IsKeyDown(Config::Get().KeyCamMoveUp)) {
      m_camera.target.y += verticalMoveSpeed;
    }
    if (IsKeyDown(Config::Get().KeyCamMoveDown)) {
      m_camera.target.y -= verticalMoveSpeed;
    }
  } else {
    Vector3 camForward =
        Vector3Normalize({m_camera.target.x - m_camera.position.x,
                          m_camera.target.y - m_camera.position.y,
                          m_camera.target.z - m_camera.position.z});
    Vector3 cUp = {0.0f, 1.0f, 0.0f};
    if (fabs(camForward.y) > 0.99f)
      cUp = {0.0f, 0.0f, -1.0f};
    Vector3 camRight = Vector3Normalize(Vector3CrossProduct(camForward, cUp));
    Vector3 camUp = Vector3Normalize(Vector3CrossProduct(camRight, camForward));

    if (!io.KeyCtrl) {
      if (IsKeyDown(Config::Get().KeyCamMoveForward)) {
        m_camera.target.x += camUp.x * currentMoveSpeed;
        m_camera.target.y += camUp.y * currentMoveSpeed;
        m_camera.target.z += camUp.z * currentMoveSpeed;
      }
      if (IsKeyDown(Config::Get().KeyCamMoveBackward)) {
        m_camera.target.x -= camUp.x * currentMoveSpeed;
        m_camera.target.y -= camUp.y * currentMoveSpeed;
        m_camera.target.z -= camUp.z * currentMoveSpeed;
      }
      if (IsKeyDown(Config::Get().KeyCamMoveLeft)) {
        m_camera.target.x -= camRight.x * currentMoveSpeed;
        m_camera.target.y -= camRight.y * currentMoveSpeed;
        m_camera.target.z -= camRight.z * currentMoveSpeed;
      }
      if (IsKeyDown(Config::Get().KeyCamMoveRight)) {
        m_camera.target.x += camRight.x * currentMoveSpeed;
        m_camera.target.y += camRight.y * currentMoveSpeed;
        m_camera.target.z += camRight.z * currentMoveSpeed;
      }
    }
  }

  // --- Movement Speed Adjust: [ and ] keys ---
  if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
    m_moveSpeedMultiplier -= 0.05f;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
    m_moveSpeedMultiplier += 0.05f;
  }
  m_moveSpeedMultiplier = roundf(m_moveSpeedMultiplier * 20.0f) / 20.0f;
  if (m_moveSpeedMultiplier < 0.05f)
    m_moveSpeedMultiplier = 0.05f;
  if (m_moveSpeedMultiplier > 3.0f)
    m_moveSpeedMultiplier = 3.0f;

  // --- Zoom Adjust: scroll wheel ---
  if (io.MouseWheel != 0.0f) {
    m_distance -= io.MouseWheel * ZOOM_SPEED * m_distance;
    if (m_distance < 0.5f)
      m_distance = 0.5f;
    if (m_distance > 1000.0f)
      m_distance = 1000.0f;
  }

  // --- Visual Mode Shortcuts ---
  if (ImGui::IsKeyDown(ImGuiKey_GraveAccent)) {
    if (ImGui::BeginTooltip()) {
      ImGui::Text("      8 Top");
      ImGui::Text("4 Left     6 Right");
      ImGui::Text("     9 Back");
      ImGui::Text("     7 Front");
      ImGui::EndTooltip();
    }

    if (ImGui::IsKeyPressed(ImGuiKey_4) ||
        ImGui::IsKeyPressed(ImGuiKey_Keypad4)) {
      m_projMode = ProjectionMode::OrthoLeft;
      UpdateCameraVectors();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_7) ||
        ImGui::IsKeyPressed(ImGuiKey_Keypad7)) {
      m_projMode = ProjectionMode::OrthoFront;
      UpdateCameraVectors();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_8) ||
        ImGui::IsKeyPressed(ImGuiKey_Keypad8)) {
      m_projMode = ProjectionMode::OrthoTop;
      UpdateCameraVectors();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_9) ||
        ImGui::IsKeyPressed(ImGuiKey_Keypad9)) {
      m_projMode = ProjectionMode::OrthoBack;
      UpdateCameraVectors();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_6) ||
        ImGui::IsKeyPressed(ImGuiKey_Keypad6)) {
      m_projMode = ProjectionMode::OrthoRight;
      UpdateCameraVectors();
    }
  }

  // Update vectors
  UpdateCameraVectors();
}

// ---------------------------------------------------------------------------
// Draw — called every frame between rlImGuiBegin / rlImGuiEnd
// ---------------------------------------------------------------------------

void ViewportBase::Draw() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  bool visible = ImGui::Begin(m_panelName.c_str());
  m_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
  ImGui::PopStyleVar();

  if (!visible) {
    ImGui::End();
    return;
  }

  // Available space for the render target
  ImVec2 avail = ImGui::GetContentRegionAvail();
  int w = (int)avail.x;
  int h = (int)avail.y;
  if (h <= 0)
    h = 1;

  DrawViewportCanvas(w, h);

  ImGui::End();
}

void ViewportBase::DrawViewportCanvas(int w, int h) {
  EnsureRenderTarget(w, h);

  // --- Render scene to texture ---
  m_hovered = ImGui::IsWindowHovered();
  ImVec2 cursorPos = ImGui::GetCursorScreenPos();

  if (m_hovered) {
    ImVec2 mousePos = ImGui::GetMousePos();
    m_localMousePos = {mousePos.x - cursorPos.x, mousePos.y - cursorPos.y};

    if (w > 0 && h > 0 && (w != m_rtWidth || h != m_rtHeight)) {
      m_localMousePos.x = (m_localMousePos.x / (float)w) * (float)m_rtWidth;
      m_localMousePos.y = (m_localMousePos.y / (float)h) * (float)m_rtHeight;
    }

    UpdateCamera();
  }

  // Pick ray calculation must happen while TextureMode is active so raylib uses
  // the correct viewport dimensions instead of the main window's dimensions.
  bool doPicking = false;
  bool doBoxPicking = false;
  Rectangle finalBox = {0};

  Vector2 pickPos = m_localMousePos;
  if (m_hovered) {
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_boxSelectStart = m_localMousePos;
        bool isMultiselectHeld = Config::Get().IsMultiselectDown();
        if (isMultiselectHeld) {
          m_isBoxSelecting = true;
        } else {
          doPicking = true;
          pickPos = m_localMousePos;
        }
      } else {
        doPicking = true;
        pickPos = m_localMousePos;
      }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      if (m_isBoxSelecting) {
        m_isBoxSelecting = false;

        float minX = std::min(m_boxSelectStart.x, m_localMousePos.x);
        float minY = std::min(m_boxSelectStart.y, m_localMousePos.y);
        float maxX = std::max(m_boxSelectStart.x, m_localMousePos.x);
        float maxY = std::max(m_boxSelectStart.y, m_localMousePos.y);

        if (maxX - minX > 5.0f || maxY - minY > 5.0f) {
          doBoxPicking = true;
          finalBox = {minX, minY, maxX - minX, maxY - minY};
        } else {
          doPicking = true;
          pickPos = m_boxSelectStart;
        }
      }
    }

  }

  if (m_isBoxSelecting) {
    m_boxSelectEnd = m_localMousePos;
  }

  BeginTextureMode(m_renderTarget);
  ClearBackground({30, 30, 30, 255});

  BeginMode3D(m_camera);

  DrawViewportGrid();

  DrawScene();

  std::vector<std::pair<std::string, Vector2>> chunkLabels;
  bool isTabHeld = (m_focused || m_hovered) &&
                   (ImGui::IsKeyDown(ImGuiKey_Tab) || IsKeyDown(KEY_TAB));
  if (m_showChunkLegend || isTabHeld) {
    DrawChunkLegend(chunkLabels);
  }

  EndMode3D();

  // Draw 2D box selection marquee
  if (m_isBoxSelecting) {
    float minX = std::min(m_boxSelectStart.x, m_boxSelectEnd.x);
    float minY = std::min(m_boxSelectStart.y, m_boxSelectEnd.y);
    float maxX = std::max(m_boxSelectStart.x, m_boxSelectEnd.x);
    float maxY = std::max(m_boxSelectStart.y, m_boxSelectEnd.y);
    Rectangle box = {minX, minY, maxX - minX, maxY - minY};

    if (box.width > 5.0f || box.height > 5.0f) {
      DrawRectangleRec(box, {100, 150, 255, 60});
      DrawRectangleLinesEx(box, 1.0f, {100, 150, 255, 200});
    }
  }

  // Draw 2D chunk legends over the 3D scene
  if (!chunkLabels.empty()) {
    int fontSize = 20;
    for (const auto &lbl : chunkLabels) {
      int textWidth = MeasureText(lbl.first.c_str(), fontSize);
      DrawText(lbl.first.c_str(), (int)lbl.second.x - textWidth / 2,
               (int)lbl.second.y - fontSize / 2, fontSize, WHITE);
    }
  }
  if (doPicking) {
    Ray ray =
        GetScreenToWorldRayEx(pickPos, m_camera, m_rtWidth, m_rtHeight);
    HandlePicking(ray);
  }
  if (doBoxPicking) {
    HandleBoxPicking(finalBox);
  }

  EndTextureMode();

  unsigned int finalTextureId = m_renderTarget.texture.id;

  if (Config::Get().EnableDitheringMode && m_shaderLoaded) {
    BeginTextureMode(m_postProcessTarget);
    ClearBackground(BLACK);
    BeginShaderMode(m_ditherShader);
    DrawTextureRec(m_renderTarget.texture,
                   {0.0f, 0.0f, (float)m_rtWidth, (float)-m_rtHeight},
                   {0.0f, 0.0f}, WHITE);
    EndShaderMode();
    EndTextureMode();
    finalTextureId = m_postProcessTarget.texture.id;
  }

  // --- Blit render texture into ImGui ---
  // Raylib's RenderTexture is Y-flipped relative to ImGui's texture coordinate
  // system
  ImVec2 uv0 = ImVec2(0.0f, 1.0f);
  ImVec2 uv1 = ImVec2(1.0f, 0.0f);
  ImGui::Image((ImTextureID)(intptr_t)finalTextureId,
               ImVec2((float)w, (float)h), uv0, uv1);

  if (ImGui::BeginPopupContextItem("ViewportContextMenu")) {
    DrawContextMenu();
    ImGui::EndPopup();
  }
}

void ViewportBase::DrawCustomGrid(float extent) {
  if (extent == 320.0f) {
    // Default global viewport grid aligned with FileManager's 16x18 range:
    // X: [-8, 7] -> world X [-320.0f, +320.0f]
    // Z: [-8, 9] -> world Z [-400.0f, +320.0f]
    DrawCustomGrid(-320.0f, 320.0f, -400.0f, 320.0f);
  } else {
    DrawCustomGrid(-extent, extent, -extent, extent);
  }
}

void ViewportBase::DrawCustomGrid(float minX, float maxX, float minZ,
                                  float maxZ) {
  float chunkSize = 40.0f;
  float maxDim = std::max(
      {std::abs(minX), std::abs(maxX), std::abs(minZ), std::abs(maxZ)});
  float yLen = (maxDim <= 40.0f) ? 15.0f : 100.0f;

  // Minor lines
  if (Config::Get().ShowMinorGridlines) {
    rlBegin(RL_LINES);
    rlColor4ub(100, 100, 100, 80);
    for (float x = minX; x <= maxX; x += 1.0f) {
      if (fmod(fabs(x), chunkSize) < 0.1f)
        continue;
      rlVertex3f(x, 0.0f, minZ);
      rlVertex3f(x, 0.0f, maxZ);
    }
    for (float z = minZ; z <= maxZ; z += 1.0f) {
      if (fmod(fabs(z), chunkSize) < 0.1f)
        continue;
      rlVertex3f(minX, 0.0f, z);
      rlVertex3f(maxX, 0.0f, z);
    }
    rlEnd();
  }

  // Major lines
  if (Config::Get().ShowMajorGridlines) {
    rlBegin(RL_LINES);
    rlColor4ub(150, 150, 150, 150);
    float step = (maxDim <= 40.0f) ? 10.0f : chunkSize;
    for (float x = minX; x <= maxX; x += step) {
      rlVertex3f(x, 0.0f, minZ);
      rlVertex3f(x, 0.0f, maxZ);
    }
    for (float z = minZ; z <= maxZ; z += step) {
      rlVertex3f(minX, 0.0f, z);
      rlVertex3f(maxX, 0.0f, z);
    }
    rlEnd();
  }

  // Axes
  rlBegin(RL_LINES);
  rlColor4ub(255, 50, 50, 255); // X Red
  rlVertex3f(minX, 0.0f, 0.0f);
  rlVertex3f(maxX, 0.0f, 0.0f);
  rlColor4ub(50, 255, 50, 255); // Y Green
  rlVertex3f(0.0f, -yLen, 0.0f);
  rlVertex3f(0.0f, yLen, 0.0f);
  rlColor4ub(50, 50, 255, 255); // Z Blue
  rlVertex3f(0.0f, 0.0f, minZ);
  rlVertex3f(0.0f, 0.0f, maxZ);
  rlEnd();

  // Billboard XYZ Coordinates
  auto drawText3D = [&](const char *text, Vector3 p1, Vector3 p2, Color c) {
    Vector3 forward =
        Vector3Normalize({m_camera.target.x - m_camera.position.x,
                          m_camera.target.y - m_camera.position.y,
                          m_camera.target.z - m_camera.position.z});

    Vector2 tr = {(float)m_rtWidth, 0.0f}; // Top-right corner of viewport
    float bestDistSq = 999999999.0f;
    Vector2 bestScreen = {-1.0f, -1.0f};
    bool found = false;

    int steps = 25; // 25 steps is perfectly smooth and extremely efficient
    int fontSize = 20;
    int textWidth = MeasureText(text, fontSize);

    for (int i = 0; i <= steps; i++) {
      float t = (float)i / (float)steps;
      Vector3 p = {p1.x + (p2.x - p1.x) * t, p1.y + (p2.y - p1.y) * t,
                   p1.z + (p2.z - p1.z) * t};

      // Ensure point is in front of camera
      Vector3 toP = {p.x - m_camera.position.x, p.y - m_camera.position.y,
                     p.z - m_camera.position.z};
      if (Vector3DotProduct(forward, toP) > 0.1f) {
        // IMPORTANT: Use GetWorldToScreenEx to map to the local viewport size,
        // avoiding full window scaling!
        Vector2 s = GetWorldToScreenEx(p, m_camera, m_rtWidth, m_rtHeight);

        // Ensure text bounding box remains fully visible inside the viewport
        float marginX = textWidth / 2.0f + 5.0f;
        float marginY = fontSize / 2.0f + 5.0f;

        if (s.x > marginX && s.y > marginY && s.x < m_rtWidth - marginX &&
            s.y < m_rtHeight - marginY) {
          float dx = s.x - tr.x;
          float dy = s.y - tr.y;
          float distSq =
              dx * dx +
              dy * dy; // Squared Euclidean distance for maximum performance
          if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestScreen = s;
            found = true;
          }
        }
      }
    }

    if (found) {
      rlDrawRenderBatchActive();
      EndMode3D();
      DrawText(text, (int)bestScreen.x - textWidth / 2,
               (int)bestScreen.y - fontSize / 2, fontSize, c);
      BeginMode3D(m_camera);
    }
  };

  drawText3D("X", {maxX + 2.0f, 0.0f, 0.0f}, {minX - 2.0f, 0.0f, 0.0f}, RED);
  drawText3D("Y", {0.0f, yLen + 2.0f, 0.0f}, {0.0f, -yLen - 2.0f, 0.0f}, GREEN);
  drawText3D("Z", {0.0f, 0.0f, maxZ + 2.0f}, {0.0f, 0.0f, minZ - 2.0f}, BLUE);
}

void ViewportBase::DrawChunkLegend(
    std::vector<std::pair<std::string, Vector2>> &outLabels) {
  auto locs = GetChunkLocations();
  rlDisableDepthMask();
  for (const auto &loc : locs) {
    Color c = m_legendColorCallback ? m_legendColorCallback(loc.name)
                                    : Color{255, 255, 255, 40};

    // Force XZ widths to 40x40 (full chunk width), using maximum theoretical
    // vertical geometry bounds (-16.0f to 16.0f)
    float width = 40.0f;
    float length = 40.0f;
    float minY = -16.0f;
    float maxY = 16.0f;
    float height = maxY - minY;

    Vector3 center = {loc.xPos * 40.0f + 20.0f, (minY + maxY) / 2.0f,
                      -(loc.yPos * 40.0f + 20.0f)};

    // Translucent bounding box
    rlDrawRenderBatchActive();
    DrawCube(center, width, height, length, c);

    // Border
    Color border = c;
    border.a = 255;
    BoundingBox chunkBounds = {
        {center.x - width / 2.0f, minY, center.z - length / 2.0f},
        {center.x + width / 2.0f, maxY, center.z + length / 2.0f}};
    DrawBoundingBox(chunkBounds, border);

    // Record screen position for 2D billboard text
    Vector3 textPos = {center.x, maxY + 2.0f, center.z};

    // Ensure point is in front of camera before projecting
    Vector3 forward =
        Vector3Normalize({m_camera.target.x - m_camera.position.x,
                          m_camera.target.y - m_camera.position.y,
                          m_camera.target.z - m_camera.position.z});
    Vector3 toP = {textPos.x - m_camera.position.x,
                   textPos.y - m_camera.position.y,
                   textPos.z - m_camera.position.z};

    if (Vector3DotProduct(forward, toP) > 0.1f) {
      Vector2 screenPos =
          GetWorldToScreenEx(textPos, m_camera, m_rtWidth, m_rtHeight);
      if (screenPos.x > 0 && screenPos.y > 0 && screenPos.x < m_rtWidth &&
          screenPos.y < m_rtHeight) {
        outLabels.push_back({loc.name, screenPos});
      }
    }
  }
  rlDrawRenderBatchActive();
  rlEnableDepthMask();
}
