#include "viewport/WaypointsOverlay.h"
#include "imgui.h"
#include "core/Config.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>

#include "viewport/Viewport.h"

WaypointsOverlay::WaypointsOverlay() {
}

WaypointsOverlay::~WaypointsOverlay() {}

bool WaypointsOverlay::LoadOverlay(const std::string &mapKey) {
  bool success = OverlayLoader::Load(mapKey, m_overlay);
  m_selectedWaypointIdx = -1;
  m_selectedLinkIdx = -1;

  if (success) {
    // Preserve camera view when switching maps (do not reset camera)
    Config::Get().LastMapKey = mapKey;
    Config::Get().Save();
  }
  return success;
}

void WaypointsOverlay::FocusOnOverlay(Viewport &vp) {
  if (!m_overlay.loaded ||
      (m_overlay.waypoints.empty() && m_overlay.links.empty()))
    return;

  float minX = 1e9f, maxX = -1e9f;
  float minZ = 1e9f, maxZ = -1e9f;

  for (const auto &wp : m_overlay.waypoints) {
    float wz = -wp.worldZ; // Convert PS1 Z to Raylib 3D World Z
    if (wp.worldX < minX)
      minX = wp.worldX;
    if (wp.worldX > maxX)
      maxX = wp.worldX;
    if (wz < minZ)
      minZ = wz;
    if (wz > maxZ)
      maxZ = wz;
  }

  if (minX <= maxX && minZ <= maxZ) {
    float centerX = (minX + maxX) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;
    float spanX = maxX - minX;
    float spanZ = maxZ - minZ;
    float maxSpan = std::max(spanX, spanZ);

    vp.SetCameraTarget({centerX, 0.0f, centerZ}, std::max(20.0f, maxSpan * 0.85f), 45.0f, 30.0f);
  }
}

bool WaypointsOverlay::GetTargetWaypointPos(const LinkData &link, Vector3 &outPos,
                                         std::string &outMapLabel) {
  // Only SysState_LoadOverlay (5) and SysState_LoadRoom (6) represent actual
  // door transitions where eventParam encodes a destination waypoint index.
  // Other SysStates use eventParam for message/callback IDs.
  if (link.sysStateValue != 5 && link.sysStateValue != 6) {
    return false;
  }

  std::string targetMap = link.destMapKey;
  if (targetMap.empty() || targetMap == "MapIdx_None") {
    targetMap = m_overlay.mapKey;
  }

  outMapLabel = targetMap;

  int targetWpIdx = link.eventParam;

  // Intra-map link
  if (targetMap == m_overlay.mapKey) {
    if (targetWpIdx >= 0 && targetWpIdx < (int)m_overlay.waypoints.size()) {
      const auto &wp = m_overlay.waypoints[targetWpIdx];
      outPos = {wp.worldX, 1.2f, -wp.worldZ};
      return true;
    }
    return false;
  }

  // Inter-map link: look up target map overlay in cache or load from workspace
  // JSON
  auto it = m_overlayCache.find(targetMap);
  if (it == m_overlayCache.end()) {
    OverlayMapData destData;
    if (OverlayLoader::Load(targetMap, destData)) {
      m_overlayCache[targetMap] = destData;
      it = m_overlayCache.find(targetMap);
    } else {
      return false;
    }
  }

  const auto &destData = it->second;
  if (targetWpIdx >= 0 && targetWpIdx < (int)destData.waypoints.size()) {
    const auto &wp = destData.waypoints[targetWpIdx];
    outPos = {wp.worldX, 1.2f, -wp.worldZ};
    return true;
  }

  return false;
}

Color WaypointsOverlay::GetEventColor(const LinkData &link) const {
  if (link.sysStateValue == 5 ||
      link.sysStateValue == 6) { // LoadOverlay, LoadRoom
    if (!link.destMapKey.empty() && link.destMapKey != "MapIdx_None" &&
        link.destMapKey != m_overlay.mapKey) {
      return Color{255, 140, 0,
                   255}; // Bright Orange (Cross-map transition door)
    }
    return Color{0, 230, 120, 255}; // Bright Green (Active doorway)
  }
  if (link.sysStateValue == 7) {     // ReadMessage (Locked / Jammed door)
    return Color{255, 190, 20, 255}; // Bright Amber / Gold
  }
  if (link.sysStateValue == 8 || link.sysStateValue == 9) { // SaveMenu
    return Color{30, 160, 255, 255};                        // Bright Blue
  }
  return Color{180, 180, 180, 255}; // Grey (Scripted/other)
}

void WaypointsOverlay::DrawBackgroundGeometry() {
  if (!m_sharedChunks || !m_showBackgroundGeo)
    return;

  rlSetBlendMode(BLEND_ALPHA);
  for (const auto &chunk : *m_sharedChunks) {
    if (!chunk.visible)
      continue;
    for (const auto &batch : chunk.batches) {
      if (batch.meshUploaded && batch.mesh.vertexCount > 0) {
        DrawMesh(batch.mesh, batch.material, MatrixIdentity());
      }
    }
  }
  rlSetBlendMode(BLEND_ALPHA);
}

void WaypointsOverlay::DrawWaypoints() {
  if (!m_showWaypoints || !m_overlay.loaded)
    return;

  for (const auto &wp : m_overlay.waypoints) {
    // PS1 Z coordinate is inverted in Raylib 3D world space (Z -> -Z)
    Vector3 pos = {wp.worldX, 0.0f, -wp.worldZ};
    bool isSelected = (wp.index == m_selectedWaypointIdx);

    bool isReferenced = false;
    Color pinColor = Color{200, 200, 200, 255};

    for (const auto &link : m_overlay.links) {
      if (link.waypointIdx == wp.index) {
        isReferenced = true;
        pinColor = GetEventColor(link);
        break;
      }
    }

    if (!isReferenced) {
      pinColor = Color{230, 40, 40, 255}; // Red for orphaned waypoint
    }

    if (isSelected) {
      pinColor = Color{255, 255, 0, 255}; // Yellow highlight
    }

    // Vertical spike pin
    DrawCylinderEx({pos.x, pos.y, pos.z}, {pos.x, pos.y + 2.5f, pos.z}, 0.15f,
                   0.05f, 8, pinColor);
    DrawSphere({pos.x, pos.y + 2.5f, pos.z}, isSelected ? 0.55f : 0.38f,
               pinColor);
    DrawCircle3D(pos, 0.6f, {1, 0, 0}, 90.0f, pinColor);

    // Facing angle indicator arrow
    float rad = (360.0f - wp.arrivalAngleDeg) * (PI / 180.0f);
    Vector3 dir = {sinf(rad) * 1.8f, 0.05f, cosf(rad) * 1.8f};
    DrawLine3D({pos.x, pos.y + 0.1f, pos.z},
               Vector3Add({pos.x, pos.y + 0.1f, pos.z}, dir), pinColor);
  }
}

void WaypointsOverlay::DrawDoorFrame(Vector3 center, float angleDeg, float width,
                                  float height, Color color, bool isSelected) {
  rlPushMatrix();
  rlTranslatef(center.x, center.y, center.z);
  rlRotatef(angleDeg, 0.0f, 1.0f, 0.0f);

  float halfW = width * 0.5f;
  float postRadius = isSelected ? 0.12f : 0.08f;

  Color frameColor = isSelected ? Color{255, 255, 0, 255} : color;

  // Left vertical post
  DrawCylinderEx({-halfW, 0.0f, 0.0f}, {-halfW, height, 0.0f}, postRadius,
                 postRadius, 8, frameColor);
  // Right vertical post
  DrawCylinderEx({halfW, 0.0f, 0.0f}, {halfW, height, 0.0f}, postRadius,
                 postRadius, 8, frameColor);
  // Top horizontal lintel beam
  DrawCylinderEx({-halfW, height, 0.0f}, {halfW, height, 0.0f}, postRadius,
                 postRadius, 8, frameColor);

  // Semi-transparent door panel volume
  Color panelColor = frameColor;
  panelColor.a = isSelected ? 140 : 70;
  DrawCubeV({0.0f, height * 0.5f, 0.0f}, {width, height, 0.15f}, panelColor);

  rlPopMatrix();
}

void WaypointsOverlay::DrawTriggers() {
  if (!m_showTriggers || !m_overlay.loaded)
    return;

  for (const auto &link : m_overlay.links) {
    if (link.waypointIdx < 0 ||
        link.waypointIdx >= (int)m_overlay.waypoints.size())
      continue;

    const auto &wp = m_overlay.waypoints[link.waypointIdx];
    Vector3 center = {wp.worldX, 0.0f, -wp.worldZ};
    bool isSelected = (link.index == m_selectedLinkIdx);

    Color color = isSelected ? Color{255, 255, 0, 255} : GetEventColor(link);
    float angleDeg = (wp.arrivalAngleDeg > 0)
                         ? (360.0f - (float)wp.arrivalAngleDeg)
                         : (360.0f - ((wp.triggerParam0 / 255.0f) * 360.0f));

    // Only draw 3D Doorway Arch Frame for actual door transitions (LoadOverlay
    // / LoadRoom)
    bool isDoor = (link.sysStateValue == 5 || link.sysStateValue == 6);
    if (isDoor) {
      DrawDoorFrame(center, angleDeg, 1.8f, 2.4f, color, isSelected);
    }

    // Draw precise trigger volume shape
    if (link.triggerTypeValue == 1) {
      // TouchAabb
      float extX = wp.triggerParam0 * 0.25f;
      float extZ = wp.triggerParam1 * 0.25f;
      if (extX < 0.3f)
        extX = 0.8f;
      if (extZ < 0.3f)
        extZ = 0.8f;

      BoundingBox box = {{center.x - extX, center.y, center.z - extZ},
                         {center.x + extX, center.y + 2.2f, center.z + extZ}};
      DrawBoundingBox(box, color);

      Color fillColor = color;
      fillColor.a = 40;
      DrawCubeV({center.x, center.y + 1.1f, center.z},
                {extX * 2.0f, 2.2f, extZ * 2.0f}, fillColor);
    } else if (link.triggerTypeValue == 2) {
      // TouchFacing (0.8m radius circle/cylinder)
      DrawCircle3D({center.x, center.y + 0.05f, center.z}, 0.8f, {1, 0, 0},
                   90.0f, color);
      DrawCylinderWires({center.x, center.y + 1.0f, center.z}, 0.8f, 0.8f, 2.0f,
                        12, color);
    } else if (link.triggerTypeValue == 3 || link.triggerTypeValue == 4) {
      // TouchObbFacing or TouchObb
      float halfExt = wp.triggerParam1 * 0.25f;
      if (halfExt < 0.3f)
        halfExt = 0.8f;
      float width = 1.8f;

      rlPushMatrix();
      rlTranslatef(center.x, center.y + 1.1f, center.z);
      rlRotatef(angleDeg, 0.0f, 1.0f, 0.0f);

      Vector3 size = {width, 2.2f, halfExt * 2.0f};
      DrawCubeWiresV({0, 0, 0}, size, color);

      Color fillColor = color;
      fillColor.a = 40;
      DrawCubeV({0, 0, 0}, size, fillColor);

      rlPopMatrix();
    }
  }
}

void WaypointsOverlay::DrawDirectionLines() {
  if (!m_showDirectionLines || !m_overlay.loaded)
    return;

  for (const auto &link : m_overlay.links) {
    if (link.waypointIdx < 0 ||
        link.waypointIdx >= (int)m_overlay.waypoints.size())
      continue;

    const auto &wp = m_overlay.waypoints[link.waypointIdx];
    Vector3 wpPos = {wp.worldX, 0.1f, -wp.worldZ};

    bool isSelected = (link.index == m_selectedLinkIdx);
    Color color = isSelected ? Color{255, 255, 0, 255} : GetEventColor(link);

    // Vertical connecting line
    DrawLine3D({wpPos.x, wpPos.y, wpPos.z}, {wpPos.x, wpPos.y + 2.5f, wpPos.z},
               color);
  }
}

void WaypointsOverlay::DrawRelationshipBeams() {
  if (!m_overlay.loaded)
    return;

  for (const auto &link : m_overlay.links) {
    int srcWpIdx = link.waypointIdx;
    if (srcWpIdx < 0 || srcWpIdx >= (int)m_overlay.waypoints.size())
      continue;

    bool isSelectedLink = (link.index == m_selectedLinkIdx);
    bool isSelectedWp = (srcWpIdx == m_selectedWaypointIdx);
    bool isSelected = isSelectedLink || isSelectedWp;

    // Draw beam if "Show All Relationships" is checked OR if this link/waypoint
    // is selected
    if (!m_showAllRelationships && !isSelected)
      continue;

    const auto &srcWp = m_overlay.waypoints[srcWpIdx];
    Vector3 srcPos = {srcWp.worldX, 1.2f, -srcWp.worldZ};

    Vector3 targetPos;
    std::string destMapLabel;
    bool hasTarget = GetTargetWaypointPos(link, targetPos, destMapLabel);

    if (!hasTarget)
      continue;

    Color beamColor;
    float radius;

    if (isSelected) {
      beamColor = Color{255, 255, 0,
                        255}; // Bright Yellow glow for selected relationship
      radius = 0.22f;
    } else if (link.destMapKey != m_overlay.mapKey &&
               !link.destMapKey.empty() && link.destMapKey != "MapIdx_None") {
      beamColor =
          Color{255, 140, 0, 220}; // Bright Orange beam for cross-map doors
      radius = 0.12f;
    } else {
      beamColor =
          Color{0, 230, 120, 220}; // Bright Green beam for intra-map doors
      radius = 0.12f;
    }

    // Draw 3D connecting beam cylinder
    DrawCylinderEx(srcPos, targetPos, radius, radius, 8, beamColor);

    // Endpoint pulse spheres
    DrawSphere(srcPos, isSelected ? 0.55f : 0.35f, beamColor);
    DrawSphere(targetPos, isSelected ? 0.55f : 0.35f, beamColor);
  }
}

void WaypointsOverlay::DrawOverlay(Viewport &vp) {
  DrawBackgroundGeometry();
  DrawWaypoints();
  DrawTriggers();
  DrawDirectionLines();
  DrawRelationshipBeams();
}

void WaypointsOverlay::HandlePicking(Viewport &vp, Ray ray) {
  if (!m_overlay.loaded)
    return;

  m_selectedWaypointIdx = -1;
  m_selectedLinkIdx = -1;

  float closestDist = 1e9f;

  // Pick Waypoints (Ray-Sphere)
  for (const auto &wp : m_overlay.waypoints) {
    Vector3 pos = {wp.worldX, 2.5f, -wp.worldZ};
    RayCollision hit = GetRayCollisionSphere(ray, pos, 0.6f);

    if (hit.hit && hit.distance < closestDist) {
      closestDist = hit.distance;
      m_selectedWaypointIdx = wp.index;
    }
  }

  // Pick Triggers if no waypoint picked
  if (m_selectedWaypointIdx < 0) {
    for (const auto &link : m_overlay.links) {
      if (link.waypointIdx < 0 ||
          link.waypointIdx >= (int)m_overlay.waypoints.size())
        continue;

      const auto &wp = m_overlay.waypoints[link.waypointIdx];
      Vector3 center = {wp.worldX, 1.1f, -wp.worldZ};
      BoundingBox box = {{center.x - 1.2f, center.y - 1.2f, center.z - 1.2f},
                         {center.x + 1.2f, center.y + 1.2f, center.z + 1.2f}};

      RayCollision hit = GetRayCollisionBox(ray, box);
      if (hit.hit && hit.distance < closestDist) {
        closestDist = hit.distance;
        m_selectedLinkIdx = link.index;
      }
    }
  }
}

void WaypointsOverlay::DrawContextMenu() {
  if (m_selectedWaypointIdx == -1 && m_selectedLinkIdx == -1) {
    if (ImGui::MenuItem("Add Waypoint Here")) {}
    if (ImGui::MenuItem("Paste")) {}
  } else if (m_selectedWaypointIdx != -1) {
    if (ImGui::MenuItem("Add Trigger / Link")) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Cut")) {}
    if (ImGui::MenuItem("Copy")) {}
    if (ImGui::MenuItem("Paste")) {}
    if (ImGui::MenuItem("Duplicate")) {}
    if (ImGui::MenuItem("Delete")) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Teleport Camera to Waypoint")) {}
    if (ImGui::BeginMenu("Rotate Waypoint")) {
      if (ImGui::MenuItem("+90*")) {}
      if (ImGui::MenuItem("-90*")) {}
      if (ImGui::MenuItem("+15*")) {}
      if (ImGui::MenuItem("-15*")) {}
      ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Snap to Floor")) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Edit Node Metadata")) {}
  } else if (m_selectedLinkIdx != -1) {
    if (ImGui::MenuItem("Teleport Camera to Destination")) {}
    if (ImGui::MenuItem("Load Destination Map")) {}
    ImGui::Separator();
    if (ImGui::MenuItem("Cut")) {}
    if (ImGui::MenuItem("Copy")) {}
    if (ImGui::MenuItem("Duplicate")) {}
    if (ImGui::MenuItem("Delete")) {}
    ImGui::Separator();
    if (ImGui::BeginMenu("Quick Convert Type")) {
      if (ImGui::MenuItem("Door Transition")) {}
      if (ImGui::MenuItem("Read Message")) {}
      if (ImGui::MenuItem("Save Menu")) {}
      if (ImGui::MenuItem("Script Event")) {}
      ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Resize Volume")) {}
    if (ImGui::MenuItem("Edit Trigger Properties")) {}
  }
}
