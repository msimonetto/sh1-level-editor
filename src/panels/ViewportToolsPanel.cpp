#include "panels/ViewportToolsPanel.h"
#include "imgui.h"
#include "extras/IconsFontAwesome6.h"
#include "viewport/CollisionOverlay.h"
#include "viewport/LocalGeometryOverlay.h"
#include "viewport/Viewport.h"
#include "viewport/ViewportBase.h"
#include "viewport/WaypointsOverlay.h"

ViewportToolsPanel::ViewportToolsPanel() {}

void ViewportToolsPanel::SetActiveViewport(ViewportBase *viewport) {
  m_activeViewport = viewport;
}

void ViewportToolsPanel::Draw(History *editHistory) {
  if (ImGui::Begin(ICON_FA_WRENCH " Tools")) {
    // 1. Viewport Mode Switcher Tab Bar
    if (ImGui::BeginTabBar("ViewportModes")) {
      ImGuiTabItemFlags flagsScene =
          (m_forceTabSelection && m_activeMode == ViewportMode::Scene)
              ? ImGuiTabItemFlags_SetSelected
              : 0;
      ImGuiTabItemFlags flagsLocal =
          (m_forceTabSelection && m_activeMode == ViewportMode::LocalGeometry)
              ? ImGuiTabItemFlags_SetSelected
              : 0;
      ImGuiTabItemFlags flagsColl =
          (m_forceTabSelection && m_activeMode == ViewportMode::Collision)
              ? ImGuiTabItemFlags_SetSelected
              : 0;
      ImGuiTabItemFlags flagsWay =
          (m_forceTabSelection &&
           m_activeMode == ViewportMode::DoorsAndWaypoints)
              ? ImGuiTabItemFlags_SetSelected
              : 0;

      if (ImGui::BeginTabItem(ICON_FA_BOX " Scene", nullptr, flagsScene)) {
        m_activeMode = ViewportMode::Scene;
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(ICON_FA_CUBE " Local Geometry", nullptr,
                              flagsLocal)) {
        m_activeMode = ViewportMode::LocalGeometry;
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(ICON_FA_BINOCULARS " Collision", nullptr,
                              flagsColl)) {
        m_activeMode = ViewportMode::Collision;
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(ICON_FA_LINK " Waypoints", nullptr, flagsWay)) {
        m_activeMode = ViewportMode::DoorsAndWaypoints;
        ImGui::EndTabItem();
      }

      m_forceTabSelection = false;
      ImGui::EndTabBar();
    }

    ImGui::Spacing();

    // 2. Active Viewport Link Status
    // if (m_activeViewport) {
    //   ImGui::TextDisabled("Linked to: %s",
    //                       m_activeViewport->GetPanelName().c_str());
    // } else {
    //   ImGui::TextDisabled("No Viewport Linked");
    // }

    ImGui::Separator();

    // 3. Delegate to respective panel based on active mode
    auto *viewport = dynamic_cast<Viewport *>(m_activeViewport);

    if (m_activeMode == ViewportMode::LocalGeometry ||
        (m_activeViewport &&
         (m_activeViewport->GetPanelName() == "Local Geometry" ||
          m_activeViewport->GetPanelName() == "Edit"))) {
      auto *localGeometryOverlay =
          viewport ? viewport->GetOverlay<LocalGeometryOverlay>(
                         ViewportMode::LocalGeometry)
                   : nullptr;
      if (localGeometryOverlay) {
        m_localGeometryPanel.DrawContent(*localGeometryOverlay, editHistory);
      } else {
        ImGui::TextDisabled("Local Geometry overlay not available.");
      }
    } else if (m_activeMode == ViewportMode::DoorsAndWaypoints ||
               (m_activeViewport &&
                m_activeViewport->GetPanelName() == "Doors and Waypoints")) {
      auto *waypointsOverlay =
          viewport ? viewport->GetOverlay<WaypointsOverlay>(
                         ViewportMode::DoorsAndWaypoints)
                   : nullptr;
      if (waypointsOverlay) {
        m_waypointsPanel.DrawContent(*waypointsOverlay, editHistory);
      } else {
        ImGui::TextDisabled("Waypoints overlay not available.");
      }
    } else if (m_activeMode == ViewportMode::Collision ||
               (m_activeViewport &&
                m_activeViewport->GetPanelName() == "Collision")) {
      auto *collisionOverlay =
          viewport ? viewport->GetOverlay<CollisionOverlay>(
                         ViewportMode::Collision)
                   : nullptr;
      if (collisionOverlay) {
        if (ImGui::CollapsingHeader(ICON_FA_SHAPES " Collision Geometry",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::Checkbox("Show Visual Geometry Overlay",
                          &collisionOverlay->m_showVisualGeometry);
        }
      } else {
        ImGui::TextDisabled("Collision overlay not available.");
      }
    } else if (m_activeMode == ViewportMode::Scene ||
               (m_activeViewport &&
                (m_activeViewport->GetPanelName() == "Scene" ||
                 m_activeViewport->GetPanelName() == "View" ||
                 m_activeViewport->GetPanelName() == "Visual Viewport"))) {
      if (ImGui::CollapsingHeader(ICON_FA_EYE " Scene Display",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("Scene Viewport displays full chunk map geometry and textures.");
      }
    } else if (m_activeViewport &&
               m_activeViewport->GetPanelName() == "Global Geometry") {
      if (ImGui::CollapsingHeader(ICON_FA_GLOBE " Global Geometry",
                                  ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("Global Geometry object manager inspector.");
      }
    }
  }
  ImGui::End();
}
