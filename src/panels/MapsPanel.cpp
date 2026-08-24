#include "panels/MapsPanel.h"
#include "core/FileManager.h"
#include "core/MapTable.h"
#include "formats/OverlayLoader.h"
#include "imgui.h"
#include "extras/IconsFontAwesome6.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

void MapsPanel::Draw(WaypointsOverlay *eventViewport) {
  if (ImGui::Begin(ICON_FA_MAP " Maps")) {
    // 1. Maps Table Section
    if (ImGui::CollapsingHeader("Maps", ImGuiTreeNodeFlags_DefaultOpen)) {
      // Search Filter
      ImGui::InputTextWithHint("##MapFilter", "Filter maps or prefixes...",
                               m_filterBuf, sizeof(m_filterBuf));
      ImGui::Spacing();

      std::string filterStr = m_filterBuf;
      std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(),
                     ::tolower);

      int visibleCount = 0;
      for (size_t i = 0; i < MAP_REGISTRY_TABLE_COUNT; ++i) {
        const auto &map = MAP_REGISTRY_TABLE[i];
        if (!filterStr.empty()) {
          std::string keyLower = map.key;
          std::string pfxLower = map.prefix;
          std::string descLower = map.description;
          std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);
          std::transform(pfxLower.begin(), pfxLower.end(), pfxLower.begin(), ::tolower);
          std::transform(descLower.begin(), descLower.end(), descLower.begin(), ::tolower);

          if (keyLower.find(filterStr) == std::string::npos &&
              pfxLower.find(filterStr) == std::string::npos &&
              descLower.find(filterStr) == std::string::npos) {
            continue;
          }
        }
        visibleCount++;
      }

      float rowHeight = ImGui::GetTextLineHeightWithSpacing();
      float tableHeight = std::min((float)visibleCount, 15.0f) * rowHeight + ImGui::GetFrameHeightWithSpacing();

      if (ImGui::BeginTable("MapsTable", 3,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_ScrollY,
                            ImVec2(0, tableHeight))) {
        ImGui::TableSetupColumn("Map Key", ImGuiTableColumnFlags_WidthFixed,
                                85.0f);
        ImGui::TableSetupColumn("Prefix", ImGuiTableColumnFlags_WidthFixed,
                                55.0f);
        ImGui::TableSetupColumn("Decomp Description",
                                ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < MAP_REGISTRY_TABLE_COUNT; ++i) {
          const auto &map = MAP_REGISTRY_TABLE[i];

          if (!filterStr.empty()) {
            std::string keyLower = map.key;
            std::string pfxLower = map.prefix;
            std::string descLower = map.description;
            std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(),
                           ::tolower);
            std::transform(pfxLower.begin(), pfxLower.end(), pfxLower.begin(),
                           ::tolower);
            std::transform(descLower.begin(), descLower.end(),
                           descLower.begin(), ::tolower);

            if (keyLower.find(filterStr) == std::string::npos &&
                pfxLower.find(filterStr) == std::string::npos &&
                descLower.find(filterStr) == std::string::npos) {
              continue;
            }
          }

          ImGui::TableNextRow();

          bool isSelected = (m_selectedMapKey == map.key);
          bool isModified =
              (eventViewport && eventViewport->GetOverlay().loaded &&
               eventViewport->GetOverlay().mapKey == map.key &&
               eventViewport->GetOverlay().dirty);

          if (isModified) {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
          }

          ImGui::TableSetColumnIndex(0);
          ImGui::PushID(map.key);
          if (ImGui::Selectable(map.key, isSelected,
                                ImGuiSelectableFlags_SpanAllColumns)) {
            m_selectedMapKey = map.key;
            if (eventViewport) {
              eventViewport->LoadOverlay(map.key);
            }
          }
          if (ImGui::BeginPopupContextItem()) {
              if (ImGui::MenuItem("Load Map Layout")) {}
              if (ImGui::MenuItem("Delete Waypoint Logic (Revert)")) {}
              if (ImGui::MenuItem("Clear Override Deployments")) {}
              ImGui::Separator();
              if (ImGui::MenuItem("Deploy Map to Decomp C Source")) {}
              if (ImGui::MenuItem("Export Geometry")) {}
              if (ImGui::MenuItem("Export Textures")) {}
              if (ImGui::MenuItem("View Map Dependencies")) {}
              if (ImGui::MenuItem("Open in Explorer")) {}
              if (ImGui::MenuItem("Change Prefix")) {}
              ImGui::EndPopup();
          }
          ImGui::PopID();

          ImGui::TableSetColumnIndex(1);
          ImGui::TextColored(isModified ? ImVec4(1.0f, 0.7f, 0.2f, 1.0f)
                                        : ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
                             "%s", map.prefix);

          ImGui::TableSetColumnIndex(2);
          ImGui::TextUnformatted(map.description);

          if (isModified) {
            ImGui::PopStyleColor();
          }
        }
        ImGui::EndTable();
      }
    }

    // 2. Actions Section
    if (ImGui::CollapsingHeader("Actions", ImGuiTreeNodeFlags_DefaultOpen)) {
      bool isDirty = false;
      if (eventViewport && eventViewport->GetOverlay().loaded &&
          eventViewport->GetOverlay().mapKey == m_selectedMapKey) {
        isDirty = eventViewport->GetOverlay().dirty;
      }

      if (isDirty) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                           "Patching required (%s modified)",
                           m_selectedMapKey.c_str());
      } else {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f),
                           "No changes detected (%s)",
                           m_selectedMapKey.c_str());
      }

      float halfWidth =
          (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) *
          0.5f;
      if (ImGui::Button("Save Overlay JSON", ImVec2(halfWidth, 0))) {
        if (eventViewport && eventViewport->GetOverlay().loaded &&
            eventViewport->GetOverlay().mapKey == m_selectedMapKey) {
          if (OverlayLoader::Save(m_selectedMapKey,
                                  eventViewport->GetOverlay())) {
            eventViewport->GetOverlay().dirty = false;
            printf("[MapsPanel] Saved overlay JSON for %s\n",
                   m_selectedMapKey.c_str());
          }
        } else {
          OverlayMapData tmpOverlay;
          if (OverlayLoader::Load(m_selectedMapKey, tmpOverlay)) {
            OverlayLoader::Save(m_selectedMapKey, tmpOverlay);
            printf("[MapsPanel] Saved overlay JSON for %s\n",
                   m_selectedMapKey.c_str());
          }
        }
      }
      ImGui::SameLine();
      if (ImGui::Button("Deploy to Decomp C Source", ImVec2(halfWidth, 0))) {
        if (eventViewport && eventViewport->GetOverlay().loaded &&
            eventViewport->GetOverlay().mapKey == m_selectedMapKey) {
          OverlayLoader::Save(m_selectedMapKey, eventViewport->GetOverlay());
          eventViewport->GetOverlay().dirty = false;
        } else {
          OverlayMapData tmpOverlay;
          if (OverlayLoader::Load(m_selectedMapKey, tmpOverlay)) {
            OverlayLoader::Save(m_selectedMapKey, tmpOverlay);
          }
        }
        FileManager pipeline;
        pipeline.DeployOverlayToDecomp(m_selectedMapKey);
      }
      ImGui::Spacing();
    }

    // 3. Other Section
    if (ImGui::CollapsingHeader("Other")) {
        ImGui::TextDisabled("Future options will be added here.");
    }
  }
  ImGui::End();
}
