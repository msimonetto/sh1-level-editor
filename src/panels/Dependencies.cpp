#include "panels/Dependencies.h"
#include "imgui.h"
#include "extras/IconsFontAwesome6.h"

DependenciesPanel::DependenciesPanel(FileManager& chunkManager)
    : m_chunkManager(chunkManager) {
}

void DependenciesPanel::Render() {
    if (ImGui::Begin(ICON_FA_SITEMAP " Dependencies")) {
        ImGui::Text("Dependencies Panel - Workspace File Manager");
        // TODO: Implement dependency visualization and tracking
    }
    ImGui::End();
}
