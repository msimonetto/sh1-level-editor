#pragma once
#include "core/History.h"
#include "imgui.h"
#include "viewport/Waypoints.h"


static const char *ALL_MAP_KEYS[] = {
    "MAP0_S00", "MAP0_S01", "MAP0_S02", "MAP1_S00", "MAP1_S01", "MAP1_S02",
    "MAP1_S03", "MAP1_S04", "MAP1_S05", "MAP1_S06", "MAP2_S00", "MAP2_S01",
    "MAP2_S02", "MAP2_S03", "MAP2_S04", "MAP3_S00", "MAP3_S01", "MAP3_S02",
    "MAP3_S03", "MAP3_S04", "MAP3_S05", "MAP3_S06", "MAP4_S00", "MAP4_S01",
    "MAP4_S02", "MAP4_S03", "MAP4_S04", "MAP4_S05", "MAP4_S06", "MAP5_S00",
    "MAP5_S01", "MAP5_S02", "MAP5_S03", "MAP6_S00", "MAP6_S01", "MAP6_S02",
    "MAP6_S03", "MAP6_S04", "MAP6_S05", "MAP7_S00", "MAP7_S01", "MAP7_S02",
    "MAP7_S03"};
static const int ALL_MAP_KEYS_COUNT =
    sizeof(ALL_MAP_KEYS) / sizeof(ALL_MAP_KEYS[0]);

static const char *SYS_STATE_NAMES[] = {"SysState_Gameplay (0)",
                                        "SysState_OptionsMenu (1)",
                                        "SysState_StatusMenu (2)",
                                        "SysState_MapScreen (3)",
                                        "SysState_Fmv (4)",
                                        "SysState_LoadOverlay (5)",
                                        "SysState_LoadRoom (6)",
                                        "SysState_ReadMessage (7)",
                                        "SysState_SaveMenu0 (8)",
                                        "SysState_SaveMenu1 (9)",
                                        "SysState_EventCallback (10)",
                                        "SysState_EventSetFlag (11)",
                                        "SysState_EventPlaySound (12)",
                                        "SysState_GameOver (13)",
                                        "SysState_GamePaused (14)",
                                        "SysState_Invalid (15)"};
static const int SYS_STATE_COUNT =
    sizeof(SYS_STATE_NAMES) / sizeof(SYS_STATE_NAMES[0]);

static const char *TRIGGER_TYPE_NAMES[] = {
    "TriggerType_None (0)", "TriggerType_TouchAabb (1)",
    "TriggerType_TouchFacing (2)", "TriggerType_TouchObbFacing (3)",
    "TriggerType_TouchObb (4)"};
static const int TRIGGER_TYPE_COUNT =
    sizeof(TRIGGER_TYPE_NAMES) / sizeof(TRIGGER_TYPE_NAMES[0]);

static const char *ACTIVATION_TYPE_NAMES[] = {
    "TriggerActivationType_None (0)", "TriggerActivationType_Exclusive (1)",
    "TriggerActivationType_Button (2)", "TriggerActivationType_Item (3)"};
static const int ACTIVATION_TYPE_COUNT =
    sizeof(ACTIVATION_TYPE_NAMES) / sizeof(ACTIVATION_TYPE_NAMES[0]);

class History;

// ---------------------------------------------------------------------------
// WaypointsPanel -- Inspector Panel Content for Doors and Waypoints Editing
// ---------------------------------------------------------------------------
class WaypointsPanel {
public:
  WaypointsPanel() = default;
  ~WaypointsPanel() = default;

  // Draw embedded inspector UI content
  void DrawContent(WaypointsOverlay &eventOverlay,
                   History *editHistory = nullptr);

private:
  OverlayMapData m_activeWidgetBeforeState;

  template <typename GuiFunc>
  bool TrackWidget(OverlayMapData &overlay, History *editHistory,
                   const char *desc, GuiFunc guiFunc) {
    OverlayMapData beforeState = overlay;
    bool changed = guiFunc();

    if (ImGui::IsItemActivated()) {
      m_activeWidgetBeforeState = beforeState;
    }

    if (ImGui::IsItemDeactivatedAfterEdit() ||
        (changed && !ImGui::IsItemActive())) {
      if (editHistory) {
        OverlayMapData snapBefore = m_activeWidgetBeforeState.loaded
                                        ? m_activeWidgetBeforeState
                                        : beforeState;
        editHistory->Push(
            OverlaySnapshot{overlay.mapKey, snapBefore, overlay, desc});
      }
      m_activeWidgetBeforeState = OverlayMapData{};
    }

    return changed;
  }
};

