#include "panels/MenuPanel.h"

#include "imgui.h"
#include "extras/IconsFontAwesome6.h"
#include <string>

namespace MenuPanel {
    bool Draw(Shortcuts& shortcuts, FileManager& fileManager, Viewport& viewport, History& history, LocalGeometryOverlay& localGeometryOverlay, WaypointsOverlay& eventOverlay, SettingsPanel& settingsWindow) {
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu(ICON_FA_FILE " File")) {
                if (ImGui::MenuItem(ICON_FA_CUBES " Save Chunks", "Ctrl+S")) {
                    shortcuts.SaveSelected(fileManager, viewport);
                }
                if (ImGui::MenuItem(ICON_FA_MAP " Save Maps")) {}
                if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save All", "Ctrl+Shift+S")) {
                    shortcuts.SaveAll(fileManager, viewport);
                }
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_FOLDER " Change Workspace")) {}
                if (ImGui::MenuItem(ICON_FA_CLOCK_ROTATE_LEFT " Revert to Defaults")) {}
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_POWER_OFF " Quit")) {
                    ImGui::EndMenu();
                    ImGui::EndMenuBar();
                    return false;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(ICON_FA_PEN_TO_SQUARE " Edit")) {
                std::string undoLabel = history.CanUndo() ? (ICON_FA_ARROW_ROTATE_LEFT " Undo " + history.PeekUndoDesc()) : (ICON_FA_ARROW_ROTATE_LEFT " Undo");
                if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, history.CanUndo())) {
                    history.Undo(viewport, localGeometryOverlay, &eventOverlay, fileManager.GetWorkspaceDir());
                }
                std::string redoLabel = history.CanRedo() ? (ICON_FA_ARROW_ROTATE_RIGHT " Redo " + history.PeekRedoDesc()) : (ICON_FA_ARROW_ROTATE_RIGHT " Redo");
                if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, history.CanRedo())) {
                    history.Redo(viewport, localGeometryOverlay, &eventOverlay, fileManager.GetWorkspaceDir());
                }
                if (ImGui::MenuItem(ICON_FA_CLOCK " Action History")) {}
                ImGui::Separator();
                if (ImGui::MenuItem(ICON_FA_GEAR " Preferences")) {
                    settingsWindow.IsOpen = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(ICON_FA_WINDOW_RESTORE " Panels")) {
                if (ImGui::BeginMenu(ICON_FA_LAYER_GROUP " Toggle Panels")) {
                    if (ImGui::MenuItem(ICON_FA_CUBES " Chunks")) {}
                    if (ImGui::MenuItem(ICON_FA_SITEMAP " Dependencies")) {}
                    if (ImGui::MenuItem(ICON_FA_MAP " Maps")) {}
                    if (ImGui::MenuItem(ICON_FA_VIDEO " Viewport")) {}
                    if (ImGui::MenuItem(ICON_FA_SHAPES " Global Geometry")) {}
                    if (ImGui::MenuItem(ICON_FA_TERMINAL " Console")) {}
                    if (ImGui::MenuItem(ICON_FA_LIST_UL " Outliner")) {}
                    if (ImGui::MenuItem(ICON_FA_PAINTBRUSH " Texture Map")) {}
                    if (ImGui::MenuItem(ICON_FA_WRENCH " Tools")) {}
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem(ICON_FA_TERMINAL " Background Console")) {}
                if (ImGui::MenuItem(ICON_FA_LOCK " Lock Panels")) {}
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(ICON_FA_EYE " Viewport")) {
                if (ImGui::BeginMenu(ICON_FA_EXPAND " Resolution Scale")) {
                    if (ImGui::MenuItem("1x")) {}
                    if (ImGui::MenuItem("2x")) {}
                    if (ImGui::MenuItem("3x")) {}
                    if (ImGui::MenuItem("4x")) {}
                    ImGui::Separator();
                    if (ImGui::MenuItem("640x480 + Dithering")) {}
                    if (ImGui::MenuItem("320x240 + Dithering")) {}
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem(ICON_FA_CAMERA_ROTATE " Backface Culling")) {}
                if (ImGui::BeginMenu(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Axes")) {
                    if (ImGui::MenuItem(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Show Axis")) {}
                    if (ImGui::MenuItem(ICON_FA_TAG " Show Axis Labels")) {}
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem(ICON_FA_TABLE_CELLS " Gridlines")) {}
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu(ICON_FA_CIRCLE_QUESTION " Help")) {
                if (ImGui::MenuItem(ICON_FA_COMPASS " Startup Guide")) {}
                if (ImGui::MenuItem(ICON_FA_FILE_LINES " Documentation")) {}
                if (ImGui::MenuItem(ICON_FA_CODE_FORK " GitHub")) {}
                if (ImGui::MenuItem(ICON_FA_CIRCLE_INFO " About")) {}
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }
        return true;
    }
}
