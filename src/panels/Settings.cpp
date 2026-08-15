#include "panels/Settings.h"
#include "core/Config.h"
#include "core/History.h"
#include "core/FileDialog.h"
#include "imgui.h"
#include "raylib.h"
#include <cstring>

std::string SettingsPanel::GetKeyName(int keycode) {
  switch (keycode) {
  case KEY_APOSTROPHE:
    return "'";
  case KEY_COMMA:
    return ",";
  case KEY_MINUS:
    return "-";
  case KEY_PERIOD:
    return ".";
  case KEY_SLASH:
    return "/";
  case KEY_ZERO:
    return "0";
  case KEY_ONE:
    return "1";
  case KEY_TWO:
    return "2";
  case KEY_THREE:
    return "3";
  case KEY_FOUR:
    return "4";
  case KEY_FIVE:
    return "5";
  case KEY_SIX:
    return "6";
  case KEY_SEVEN:
    return "7";
  case KEY_EIGHT:
    return "8";
  case KEY_NINE:
    return "9";
  case KEY_SEMICOLON:
    return ";";
  case KEY_EQUAL:
    return "=";
  case KEY_A:
    return "A";
  case KEY_B:
    return "B";
  case KEY_C:
    return "C";
  case KEY_D:
    return "D";
  case KEY_E:
    return "E";
  case KEY_F:
    return "F";
  case KEY_G:
    return "G";
  case KEY_H:
    return "H";
  case KEY_I:
    return "I";
  case KEY_J:
    return "J";
  case KEY_K:
    return "K";
  case KEY_L:
    return "L";
  case KEY_M:
    return "M";
  case KEY_N:
    return "N";
  case KEY_O:
    return "O";
  case KEY_P:
    return "P";
  case KEY_Q:
    return "Q";
  case KEY_R:
    return "R";
  case KEY_S:
    return "S";
  case KEY_T:
    return "T";
  case KEY_U:
    return "U";
  case KEY_V:
    return "V";
  case KEY_W:
    return "W";
  case KEY_X:
    return "X";
  case KEY_Y:
    return "Y";
  case KEY_Z:
    return "Z";
  case KEY_LEFT_BRACKET:
    return "[";
  case KEY_BACKSLASH:
    return "\\";
  case KEY_RIGHT_BRACKET:
    return "]";
  case KEY_GRAVE:
    return "`";
  case KEY_SPACE:
    return "Space";
  case KEY_ESCAPE:
    return "Escape";
  case KEY_ENTER:
    return "Enter";
  case KEY_TAB:
    return "Tab";
  case KEY_BACKSPACE:
    return "Backspace";
  case KEY_INSERT:
    return "Insert";
  case KEY_DELETE:
    return "Delete";
  case KEY_RIGHT:
    return "Right Arrow";
  case KEY_LEFT:
    return "Left Arrow";
  case KEY_DOWN:
    return "Down Arrow";
  case KEY_UP:
    return "Up Arrow";
  case KEY_PAGE_UP:
    return "Page Up";
  case KEY_PAGE_DOWN:
    return "Page Down";
  case KEY_HOME:
    return "Home";
  case KEY_END:
    return "End";
  case KEY_CAPS_LOCK:
    return "Caps Lock";
  case KEY_SCROLL_LOCK:
    return "Scroll Lock";
  case KEY_NUM_LOCK:
    return "Num Lock";
  case KEY_PRINT_SCREEN:
    return "Print Screen";
  case KEY_PAUSE:
    return "Pause";
  case KEY_F1:
    return "F1";
  case KEY_F2:
    return "F2";
  case KEY_F3:
    return "F3";
  case KEY_F4:
    return "F4";
  case KEY_F5:
    return "F5";
  case KEY_F6:
    return "F6";
  case KEY_F7:
    return "F7";
  case KEY_F8:
    return "F8";
  case KEY_F9:
    return "F9";
  case KEY_F10:
    return "F10";
  case KEY_F11:
    return "F11";
  case KEY_F12:
    return "F12";
  case KEY_LEFT_SHIFT:
    return "Left Shift";
  case KEY_LEFT_CONTROL:
    return "Left Ctrl";
  case KEY_LEFT_ALT:
    return "Left Alt";
  case KEY_LEFT_SUPER:
    return "Left Super";
  case KEY_RIGHT_SHIFT:
    return "Right Shift";
  case KEY_RIGHT_CONTROL:
    return "Right Ctrl";
  case KEY_RIGHT_ALT:
    return "Right Alt";
  case KEY_RIGHT_SUPER:
    return "Right Super";
  case KEY_KP_0:
    return "Numpad 0";
  case KEY_KP_1:
    return "Numpad 1";
  case KEY_KP_2:
    return "Numpad 2";
  case KEY_KP_3:
    return "Numpad 3";
  case KEY_KP_4:
    return "Numpad 4";
  case KEY_KP_5:
    return "Numpad 5";
  case KEY_KP_6:
    return "Numpad 6";
  case KEY_KP_7:
    return "Numpad 7";
  case KEY_KP_8:
    return "Numpad 8";
  case KEY_KP_9:
    return "Numpad 9";
  case KEY_KP_DECIMAL:
    return "Numpad .";
  case KEY_KP_DIVIDE:
    return "Numpad /";
  case KEY_KP_MULTIPLY:
    return "Numpad *";
  case KEY_KP_SUBTRACT:
    return "Numpad -";
  case KEY_KP_ADD:
    return "Numpad +";
  case KEY_KP_ENTER:
    return "Numpad Enter";
  case KEY_KP_EQUAL:
    return "Numpad =";
  default:
    return "Key " + std::to_string(keycode);
  }
}

#include "imgui_internal.h"
#include "extras/IconsFontAwesome6.h"

SettingsPanel::SettingsPanel() {}

SettingsPanel::~SettingsPanel() {}

void SettingsPanel::Draw(History &history) {
  if (!IsOpen)
    return;

  ImGui::SetNextWindowSize(ImVec2(600, 450), ImGuiCond_FirstUseEver);
  if (ImGui::Begin(ICON_FA_GEAR " Settings", &IsOpen, ImGuiWindowFlags_NoCollapse)) {

    float footerHeight = 40.0f;

    // Left pane
    ImGui::BeginChild("SettingsTabs", ImVec2(150, -footerHeight), true);
    if (ImGui::Selectable("Directories", m_activeTab == Tab::Directories))
      m_activeTab = Tab::Directories;
    if (ImGui::Selectable("Interface", m_activeTab == Tab::Interface))
      m_activeTab = Tab::Interface;
    if (ImGui::Selectable("Viewport", m_activeTab == Tab::Viewport))
      m_activeTab = Tab::Viewport;
    if (ImGui::Selectable("Controls", m_activeTab == Tab::Controls))
      m_activeTab = Tab::Controls;
    ImGui::EndChild();

    ImGui::SameLine();

    // Right pane
    ImGui::BeginChild("SettingsContent", ImVec2(0, -footerHeight), true);

    switch (m_activeTab) {
    case Tab::Directories:
      DrawDirectoriesTab();
      break;
    case Tab::Interface:
      DrawInterfaceTab(history);
      break;
    case Tab::Viewport:
      DrawViewportTab();
      break;
    case Tab::Controls:
      DrawControlsTab();
      break;
    }

    ImGui::EndChild();

    // Footer buttons
    ImGui::Separator();
    if (ImGui::Button("Save Settings", ImVec2(120, 26))) {
      Config::Get().Save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Close", ImVec2(90, 26))) {
      IsOpen = false;
    }
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 140 -
                         ImGui::GetStyle().WindowPadding.x);
    if (ImGui::Button("Restore Defaults", ImVec2(140, 26))) {
      switch (m_activeTab) {
      case Tab::Directories:
        Config::Get().ProjectDirectory = "";
        Config::Get().GameDirectory = "";
        m_projectDirBuf[0] = '\0';
        m_gameDirBuf[0] = '\0';
        break;
      case Tab::Interface:
        Config::Get().ColorSelected = {253, 249, 0, 255};
        Config::Get().ColorWorkspace = {100, 80, 20, 40};
        Config::Get().ColorDeployment = {20, 80, 40, 40};
        Config::Get().ColorUnloaded = {43, 43, 43, 40};
        Config::Get().UndoDepth = 50;
        history.SetMaxDepth(50);
        break;
      case Tab::Viewport:
        Config::Get().ShowMajorGridlines = true;
        Config::Get().ShowMinorGridlines = true;
        Config::Get().GridCellSize = 30.0f;
        Config::Get().EnableDitheringMode = false;
        Config::Get().ShowPersistentWireframe = true;
        Config::Get().WireframeColor = {255, 255, 255, 63};
        Config::Get().WireframeThickness = 1.0f;
        break;
      case Tab::Controls:
        Config::Get().KeyMoveForward = KEY_W;
        Config::Get().KeyMoveBackward = KEY_S;
        Config::Get().KeyMoveLeft = KEY_A;
        Config::Get().KeyMoveRight = KEY_D;
        Config::Get().KeyMoveUp = KEY_R;
        Config::Get().KeyMoveDown = KEY_F;
        Config::Get().KeyMultiselect = KEY_LEFT_CONTROL;
        break;
      }
    }
  }
  ImGui::End();
}

void SettingsPanel::DrawDirectoriesTab() {
  ImGui::Text("Directories");
  ImGui::Separator();

  // Copy to buffers if empty (lazy init)
  if (m_projectDirBuf[0] == '\0' && !Config::Get().ProjectDirectory.empty()) {
    std::strncpy(m_projectDirBuf, Config::Get().ProjectDirectory.c_str(),
                 sizeof(m_projectDirBuf) - 1);
  }
  if (m_gameDirBuf[0] == '\0' && !Config::Get().GameDirectory.empty()) {
    std::strncpy(m_gameDirBuf, Config::Get().GameDirectory.c_str(),
                 sizeof(m_gameDirBuf) - 1);
  }

  float width = ImGui::GetContentRegionAvail().x - 100.0f;

  ImGui::PushItemWidth(width);
  if (ImGui::InputText("##ProjDir", m_projectDirBuf, sizeof(m_projectDirBuf))) {
    Config::Get().ProjectDirectory = m_projectDirBuf;
  }
  ImGui::PopItemWidth();
  ImGui::SameLine();
  if (ImGui::Button("Browse...##Proj", ImVec2(90, 0))) {
    std::string path = FileDialog::OpenDirectory("Select Project Directory");
    if (!path.empty()) {
      Config::Get().ProjectDirectory = path;
      std::strncpy(m_projectDirBuf, path.c_str(), sizeof(m_projectDirBuf) - 1);
    }
  }
  ImGui::Text("Project Directory");

  ImGui::Spacing();

  ImGui::PushItemWidth(width);
  if (ImGui::InputText("##GameDir", m_gameDirBuf, sizeof(m_gameDirBuf))) {
    Config::Get().GameDirectory = m_gameDirBuf;
  }
  ImGui::PopItemWidth();
  ImGui::SameLine();
  if (ImGui::Button("Browse...##Game", ImVec2(90, 0))) {
    std::string path = FileDialog::OpenDirectory("Select Game Directory");
    if (!path.empty()) {
      Config::Get().GameDirectory = path;
      std::strncpy(m_gameDirBuf, path.c_str(), sizeof(m_gameDirBuf) - 1);
    }
  }
  ImGui::Text("Game Directory");
}

void SettingsPanel::DrawInterfaceTab(History &history) {
  ImGui::Text("Interface Settings");
  ImGui::Separator();

  if (ImGui::Checkbox("Hi-DPI Scale (2x) (Requires Restart)",
                      &Config::Get().IsHiDPI)) {
    // Will apply on next restart or immediate if desired
  }

  ImGui::Spacing();
  int undoDepth = Config::Get().UndoDepth;
  if (ImGui::SliderInt("Undo Depth", &undoDepth, 1, 200)) {
    Config::Get().UndoDepth = undoDepth;
    history.SetMaxDepth(undoDepth);
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Maximum number of undo steps kept in memory");

  ImGui::Spacing();
  ImGui::Text("Viewport Colors");

  auto colorToFloat4 = [](Color c) -> ImVec4 {
    return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
  };
  auto float4ToColor = [](ImVec4 v) -> Color {
    return Color{(unsigned char)(v.x * 255), (unsigned char)(v.y * 255),
                 (unsigned char)(v.z * 255), (unsigned char)(v.w * 255)};
  };

  ImGuiColorEditFlags colorFlags =
      ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_AlphaBar;

  ImVec4 colSel = colorToFloat4(Config::Get().ColorSelected);
  if (ImGui::ColorEdit4("Selected Box", (float *)&colSel, colorFlags))
    Config::Get().ColorSelected = float4ToColor(colSel);

  ImVec4 colWork = colorToFloat4(Config::Get().ColorWorkspace);
  if (ImGui::ColorEdit4("Workspace Chunk", (float *)&colWork, colorFlags))
    Config::Get().ColorWorkspace = float4ToColor(colWork);

  ImVec4 colDep = colorToFloat4(Config::Get().ColorDeployment);
  if (ImGui::ColorEdit4("Deployment Chunk", (float *)&colDep, colorFlags))
    Config::Get().ColorDeployment = float4ToColor(colDep);

  ImVec4 colUnl = colorToFloat4(Config::Get().ColorUnloaded);
  if (ImGui::ColorEdit4("Unloaded Chunk", (float *)&colUnl, colorFlags))
    Config::Get().ColorUnloaded = float4ToColor(colUnl);
}

void SettingsPanel::DrawViewportTab() {
  ImGui::Text("Viewport Settings");
  ImGui::Separator();

  ImGui::Checkbox("Enable Dithering Mode (PS1 Rendering)",
                  &Config::Get().EnableDitheringMode);

  ImGui::Spacing();
  ImGui::Checkbox("Show Major Gridlines", &Config::Get().ShowMajorGridlines);
  ImGui::Checkbox("Show Minor Gridlines", &Config::Get().ShowMinorGridlines);

  ImGui::Spacing();
  ImGui::Checkbox("Persistent Wireframe (Edit Viewport)",
                  &Config::Get().ShowPersistentWireframe);
  if (Config::Get().ShowPersistentWireframe) {
    auto colorToFloat4 = [](Color c) -> ImVec4 {
      return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
    };
    auto float4ToColor = [](ImVec4 v) -> Color {
      return Color{(unsigned char)(v.x * 255), (unsigned char)(v.y * 255),
                   (unsigned char)(v.z * 255), (unsigned char)(v.w * 255)};
    };

    ImGuiColorEditFlags colorFlags = ImGuiColorEditFlags_PickerHueWheel |
                                     ImGuiColorEditFlags_AlphaBar |
                                     ImGuiColorEditFlags_NoInputs;
    ImVec4 colWire = colorToFloat4(Config::Get().WireframeColor);
    if (ImGui::ColorEdit4("Wireframe Color", (float *)&colWire, colorFlags)) {
      Config::Get().WireframeColor = float4ToColor(colWire);
    }

    float thickness = Config::Get().WireframeThickness;
    if (ImGui::SliderFloat("Wireframe Thickness", &thickness, 1.0f, 5.0f,
                           "%.1f")) {
      Config::Get().WireframeThickness = thickness;
    }
  }

  ImGui::Spacing();
  ImGui::Text("Chunk Selection Grid");
  ImGui::SliderFloat("Grid Cell Size", &Config::Get().GridCellSize, 10.0f,
                     40.0f, "%.0f");
}

void SettingsPanel::DrawControlsTab() {
  ImGui::Text("Controls Keybindings");
  ImGui::Separator();

  struct KeyBindingItem {
    const char *label;
    int *pKey;
  };

  KeyBindingItem bindings[] = {
      {"Camera Move Forward", &Config::Get().KeyCamMoveForward},
      {"Camera Move Backward", &Config::Get().KeyCamMoveBackward},
      {"Camera Move Left", &Config::Get().KeyCamMoveLeft},
      {"Camera Move Right", &Config::Get().KeyCamMoveRight},
      {"Camera Move Up", &Config::Get().KeyCamMoveUp},
      {"Camera Move Down", &Config::Get().KeyCamMoveDown},
      {"Selection Move Forward", &Config::Get().KeyMoveForward},
      {"Selection Move Backward", &Config::Get().KeyMoveBackward},
      {"Selection Move Left", &Config::Get().KeyMoveLeft},
      {"Selection Move Right", &Config::Get().KeyMoveRight},
      {"Selection Move Up", &Config::Get().KeyMoveUp},
      {"Selection Move Down", &Config::Get().KeyMoveDown},
      {"Multiselect Modifier", &Config::Get().KeyMultiselect},
  };
  const int numBindings = 13;

  int pressedKey = GetKeyPressed();

  // If waiting for key press in popup
  if (m_rebindingAction >= 0) {
    ImGui::OpenPopup("Rebind Key");
  }

  if (ImGui::BeginPopupModal("Rebind Key", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Press any key to assign, or Escape to cancel...");
    ImGui::Separator();

    if (pressedKey != 0) {
      if (pressedKey != KEY_ESCAPE && m_rebindingAction >= 0 &&
          m_rebindingAction < numBindings) {
        *bindings[m_rebindingAction].pKey = pressedKey;
      }
      m_rebindingAction = -1;
      ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      m_rebindingAction = -1;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }

  for (int i = 0; i < numBindings; i++) {
    ImGui::PushID(i);
    std::string keyLabel = GetKeyName(*bindings[i].pKey);

    // Check for conflicts
    bool conflict = false;
    for (int j = 0; j < numBindings; j++) {
      if (i != j && *bindings[i].pKey == *bindings[j].pKey &&
          *bindings[i].pKey != 0) {
        conflict = true;
        break;
      }
    }

    if (conflict) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
    }

    if (ImGui::Button(keyLabel.c_str(), ImVec2(160, 0))) {
      m_rebindingAction = i;
    }

    if (conflict) {
      ImGui::PopStyleColor(3);
    }

    ImGui::SameLine();
    ImGui::Text("%s", bindings[i].label);
    ImGui::PopID();
  }

  ImGui::Spacing();
  ImGui::TextDisabled("Click any key button above to rebind controls.");
}
