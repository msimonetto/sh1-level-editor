#include "panels/Waypoints.h"
#include "core/History.h"
#include "imgui.h"
#include "core/OverlayLoader.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>

void WaypointsPanel::DrawContent(WaypointsOverlay& eventViewport, History* editHistory) {
    OverlayMapData& overlay = eventViewport.GetOverlay();

    if (!overlay.loaded || overlay.mapKey.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[No Map Overlay Loaded]");
        ImGui::TextWrapped("Select a map overlay from the 'Maps' tab (left panel) to inspect and edit its doors & waypoints.");
        return;
    }

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Active Map Overlay: %s", overlay.mapKey.c_str());
    ImGui::TextDisabled("Waypoints: %d  |  Events: %d", (int)overlay.waypoints.size(), (int)overlay.links.size());
    ImGui::Separator();

    // 3D Visualization Controls
    ImGui::Text("Visualization:");
    ImGui::Checkbox("Show All Door Relationships", &eventViewport.m_showAllRelationships);
    ImGui::Checkbox("Waypoints", &eventViewport.m_showWaypoints);
    ImGui::SameLine();
    ImGui::Checkbox("Door Triggers", &eventViewport.m_showTriggers);
    ImGui::SameLine();
    ImGui::Checkbox("Background Geo", &eventViewport.m_showBackgroundGeo);
    ImGui::Separator();

    ImGui::TextDisabled("Note: Map Overlay saving and C source deployment buttons are in the 'Maps' panel (left dock).");
    if (ImGui::Button("+ Add Waypoint")) {
        OverlayMapData beforeState = overlay;
        WaypointData newWp;
        newWp.index = (int)overlay.waypoints.size();
        newWp.worldX = 0.0f;
        newWp.worldZ = 0.0f;
        newWp.arrivalAngleDeg = 0;
        overlay.waypoints.push_back(newWp);
        eventViewport.SetSelectedWaypoint(newWp.index);
        overlay.dirty = true;
        if (editHistory) {
            editHistory->Push(OverlaySnapshot{ overlay.mapKey, beforeState, overlay, "Add Waypoint #" + std::to_string(newWp.index) });
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("+ Add Link")) {
        OverlayMapData beforeState = overlay;
        LinkData newLink;
        newLink.index = (int)overlay.links.size();
        newLink.waypointIdx = eventViewport.GetSelectedWaypointIdx() >= 0 ? eventViewport.GetSelectedWaypointIdx() : 0;
        newLink.sysStateValue = 6; // LoadRoom
        newLink.sysState = "SysState_LoadRoom";
        newLink.triggerTypeValue = 3; // TouchObbFacing
        newLink.triggerType = "TriggerType_TouchObbFacing";
        newLink.activationTypeValue = 2; // Button
        newLink.activationType = "TriggerActivationType_Button";
        overlay.links.push_back(newLink);
        eventViewport.SetSelectedLink(newLink.index);
        overlay.dirty = true;
        if (editHistory) {
            editHistory->Push(OverlaySnapshot{ overlay.mapKey, beforeState, overlay, "Add Event Link #" + std::to_string(newLink.index) });
        }
    }

    ImGui::Separator();

    int selWpIdx = eventViewport.GetSelectedWaypointIdx();
    int selLinkIdx = eventViewport.GetSelectedLinkIdx();

    // 1. Waypoint Inspector
    if (selWpIdx >= 0 && selWpIdx < (int)overlay.waypoints.size()) {
        WaypointData& wp = overlay.waypoints[selWpIdx];
        if (ImGui::CollapsingHeader("Selected Waypoint", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Waypoint Index: #%d", wp.index);

            float pos[2] = { wp.worldX, wp.worldZ };
            TrackWidget(overlay, editHistory, "Move Waypoint", [&]() {
                if (ImGui::DragFloat2("Position (X, Z)", pos, 0.1f)) {
                    wp.worldX = pos[0];
                    wp.worldZ = pos[1];
                    wp.dirty = true;
                    overlay.dirty = true;
                    return true;
                }
                return false;
            });

            TrackWidget(overlay, editHistory, "Set Arrival Angle", [&]() {
                if (ImGui::SliderInt("Arrival Angle (deg)", &wp.arrivalAngleDeg, 0, 359)) {
                    wp.triggerParam0 = (int)round((wp.arrivalAngleDeg / 360.0f) * 255.0f) & 0xFF;
                    wp.dirty = true;
                    overlay.dirty = true;
                    return true;
                }
                return false;
            });

            TrackWidget(overlay, editHistory, "Set Trigger Param 1", [&]() {
                if (ImGui::DragInt("Trigger Param 1 (Half Extent)", &wp.triggerParam1, 1, 0, 255)) {
                    wp.dirty = true;
                    overlay.dirty = true;
                    return true;
                }
                return false;
            });

            TrackWidget(overlay, editHistory, "Set Loading Screen ID", [&]() {
                if (ImGui::InputInt("Loading Screen ID", &wp.loadingScreenIdValue)) {
                    wp.dirty = true;
                    overlay.dirty = true;
                    return true;
                }
                return false;
            });

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Delete Waypoint")) {
                OverlayMapData beforeState = overlay;
                overlay.waypoints.erase(overlay.waypoints.begin() + selWpIdx);
                for (size_t i = 0; i < overlay.waypoints.size(); ++i) {
                    overlay.waypoints[i].index = (int)i;
                }
                eventViewport.SetSelectedWaypoint(-1);
                overlay.dirty = true;
                if (editHistory) {
                    editHistory->Push(OverlaySnapshot{ overlay.mapKey, beforeState, overlay, "Delete Waypoint #" + std::to_string(selWpIdx) });
                }
            }
            ImGui::PopStyleColor();
        }
    }

    // 2. Event Link Inspector
    if (selLinkIdx >= 0 && selLinkIdx < (int)overlay.links.size()) {
        LinkData& link = overlay.links[selLinkIdx];
        if (ImGui::CollapsingHeader("Selected Event Link", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Link Index: #%d", link.index);

            // Target Waypoint selector
            TrackWidget(overlay, editHistory, "Set Target Waypoint Index", [&]() {
                if (ImGui::InputInt("Target Waypoint Index", &link.waypointIdx)) {
                    if (link.waypointIdx < 0) link.waypointIdx = 0;
                    if (link.waypointIdx >= (int)overlay.waypoints.size() && !overlay.waypoints.empty()) {
                        link.waypointIdx = (int)overlay.waypoints.size() - 1;
                    }
                    link.dirty = true;
                    overlay.dirty = true;
                    return true;
                }
                return false;
            });

            // SysState dropdown
            int currentSysState = link.sysStateValue;
            if (currentSysState < 0) currentSysState = 0;
            if (currentSysState >= SYS_STATE_COUNT) currentSysState = SYS_STATE_COUNT - 1;
            TrackWidget(overlay, editHistory, "Set SysState", [&]() {
                if (ImGui::Combo("SysState", &currentSysState, SYS_STATE_NAMES, SYS_STATE_COUNT)) {
                    link.sysStateValue = currentSysState;
                    link.sysState = SYS_STATE_NAMES[currentSysState];
                    link.dirty = true;
                    overlay.dirty = true;
                    return true;
                }
                return false;
            });

            // Destination Map dropdown
            int currentDestMapIdx = link.destMapIdx;
            if (currentDestMapIdx < 0) currentDestMapIdx = 0;
            if (currentDestMapIdx >= ALL_MAP_KEYS_COUNT) currentDestMapIdx = ALL_MAP_KEYS_COUNT - 1;

            TrackWidget(overlay, editHistory, "Set Destination Map", [&]() {
                if (ImGui::Combo("Destination Map", &currentDestMapIdx, ALL_MAP_KEYS, ALL_MAP_KEYS_COUNT)) {
                    link.destMapIdx = currentDestMapIdx;
                    link.destMapKey = ALL_MAP_KEYS[currentDestMapIdx];
                    link.dirty = true;
                    overlay.dirty = true;
                    return true;
                }
                return false;
            });

            // Trigger Type dropdown
            int currentTrigType = link.triggerTypeValue;
            if (currentTrigType < 0) currentTrigType = 0;
            if (currentTrigType >= TRIGGER_TYPE_COUNT) currentTrigType = TRIGGER_TYPE_COUNT - 1;
            TrackWidget(overlay, editHistory, "Set Trigger Type", [&]() {
                if (ImGui::Combo("Trigger Type", &currentTrigType, TRIGGER_TYPE_NAMES, TRIGGER_TYPE_COUNT)) {
                    link.triggerTypeValue = currentTrigType;
                    link.triggerType = TRIGGER_TYPE_NAMES[currentTrigType];
                    link.dirty = true;
                    overlay.dirty = true;
                    return true;
                }
                return false;
            });

            // Activation Type dropdown
            int currentActType = link.activationTypeValue;
            if (currentActType < 0) currentActType = 0;
            if (currentActType >= ACTIVATION_TYPE_COUNT) currentActType = ACTIVATION_TYPE_COUNT - 1;
            TrackWidget(overlay, editHistory, "Set Activation Type", [&]() {
                if (ImGui::Combo("Activation Type", &currentActType, ACTIVATION_TYPE_NAMES, ACTIVATION_TYPE_COUNT)) {
                    link.activationTypeValue = currentActType;
                    link.activationType = ACTIVATION_TYPE_NAMES[currentActType];
                    link.dirty = true;
                    overlay.dirty = true;
                    return true;
                }
                return false;
            });

            TrackWidget(overlay, editHistory, "Set Event Param", [&]() {
                if (ImGui::InputInt("Event Param", &link.eventParam)) {
                    link.dirty = true;
                    overlay.dirty = true;
                    return true;
                }
                return false;
            });

            TrackWidget(overlay, editHistory, "Set Required Flag", [&]() {
                if (ImGui::InputInt("Required Flag", &link.requiredEventFlag)) {
                    link.dirty = true;
                    overlay.dirty = true;
                    return true;
                }
                return false;
            });

            TrackWidget(overlay, editHistory, "Set Disabled Flag", [&]() {
                if (ImGui::InputInt("Disabled Flag", &link.disabledEventFlagValue)) {
                    link.dirty = true;
                    overlay.dirty = true;
                    return true;
                }
                return false;
            });

            ImGui::Separator();
            ImGui::Text("Linking Options:");

            std::string destKey = ALL_MAP_KEYS[currentDestMapIdx];
            if (ImGui::Button("Link (One-Way)")) {
                OverlayMapData beforeState = overlay;
                link.destMapIdx = currentDestMapIdx;
                link.destMapKey = destKey;
                link.dirty = true;
                overlay.dirty = true;
                if (editHistory) {
                    editHistory->Push(OverlaySnapshot{ overlay.mapKey, beforeState, overlay, "Link One-Way -> " + destKey });
                }
                printf("[WaypointsPanel] Linked door #%d -> %s\n", link.index, destKey.c_str());
            }

            ImGui::SameLine();
            if (ImGui::Button("Link Bidirectionally")) {
                OverlayMapData beforeState = overlay;
                link.destMapIdx = currentDestMapIdx;
                link.destMapKey = destKey;
                link.dirty = true;
                overlay.dirty = true;

                // Open target map overlay JSON, append reverse link pointing back
                OverlayMapData targetOverlay;
                if (OverlayLoader::Load(destKey, targetOverlay)) {
                    WaypointData revWp;
                    revWp.index = (int)targetOverlay.waypoints.size();
                    if (link.waypointIdx >= 0 && link.waypointIdx < (int)overlay.waypoints.size()) {
                        const auto& srcWp = overlay.waypoints[link.waypointIdx];
                        revWp.worldX = srcWp.worldX;
                        revWp.worldZ = srcWp.worldZ;
                        revWp.arrivalAngleDeg = (srcWp.arrivalAngleDeg + 180) % 360;
                        revWp.triggerParam0 = (int)round((revWp.arrivalAngleDeg / 360.0f) * 255.0f) & 0xFF;
                    }
                    targetOverlay.waypoints.push_back(revWp);

                    LinkData revLink = link;
                    revLink.index = (int)targetOverlay.links.size();
                    revLink.waypointIdx = revWp.index;

                    int currentMapEnumVal = 0;
                    for (int m = 0; m < ALL_MAP_KEYS_COUNT; ++m) {
                        if (overlay.mapKey == ALL_MAP_KEYS[m]) {
                            currentMapEnumVal = m;
                            break;
                        }
                    }
                    revLink.destMapIdx = currentMapEnumVal;
                    revLink.destMapKey = overlay.mapKey;

                    targetOverlay.links.push_back(revLink);
                    OverlayLoader::Save(destKey, targetOverlay);
                    OverlayLoader::Save(overlay.mapKey, overlay);

                    if (editHistory) {
                        editHistory->Push(OverlaySnapshot{ overlay.mapKey, beforeState, overlay, "Link Bidirectionally <-> " + destKey });
                    }
                    printf("[WaypointsPanel] Bidirectionally linked %s <-> %s!\n", overlay.mapKey.c_str(), destKey.c_str());
                } else {
                    printf("[WaypointsPanel] Target map overlay JSON %s not found for bidirectional link.\n", destKey.c_str());
                }
            }

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Delete Link")) {
                OverlayMapData beforeState = overlay;
                overlay.links.erase(overlay.links.begin() + selLinkIdx);
                for (size_t i = 0; i < overlay.links.size(); ++i) {
                    overlay.links[i].index = (int)i;
                }
                eventViewport.SetSelectedLink(-1);
                overlay.dirty = true;
                if (editHistory) {
                    editHistory->Push(OverlaySnapshot{ overlay.mapKey, beforeState, overlay, "Delete Link #" + std::to_string(selLinkIdx) });
                }
            }
            ImGui::PopStyleColor();
        }
    }
}
