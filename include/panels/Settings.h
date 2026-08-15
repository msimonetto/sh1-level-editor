#pragma once
#include "imgui.h"
#include <string>

class History;

class SettingsPanel {
public:
    SettingsPanel();
    ~SettingsPanel();
    
    void Draw(History& history);
    
    bool IsOpen = false;

private:
    enum class Tab {
        Directories,
        Interface,
        Viewport,
        Controls
    };
    
    Tab m_activeTab = Tab::Directories;
    
    void DrawDirectoriesTab();
    void DrawInterfaceTab(History& history);
    void DrawViewportTab();
    void DrawControlsTab();
    
    int m_rebindingAction = -1;
    static std::string GetKeyName(int keycode);
    
    // Buffers for input text
    char m_projectDirBuf[512] = "";
    char m_gameDirBuf[512] = "";
};
