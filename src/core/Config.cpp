#include "core/Config.h"
#include "imgui.h"
#include <fstream>
#include <sstream>
#include <vector>

Config& Config::Get() {
    static Config instance;
    return instance;
}

std::string Config::ColorToString(Color c) {
    std::stringstream ss;
    ss << (int)c.r << "," << (int)c.g << "," << (int)c.b << "," << (int)c.a;
    return ss.str();
}

Color Config::StringToColor(const std::string& str) {
    std::vector<unsigned char> vals;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        vals.push_back((unsigned char)std::stoi(item));
    }
    Color c = {0, 0, 0, 255};
    if (vals.size() > 0) c.r = vals[0];
    if (vals.size() > 1) c.g = vals[1];
    if (vals.size() > 2) c.b = vals[2];
    if (vals.size() > 3) c.a = vals[3];
    return c;
}

std::string Config::Vector3ToString(Vector3 v) {
    std::stringstream ss;
    ss << v.x << "," << v.y << "," << v.z;
    return ss.str();
}

Vector3 Config::StringToVector3(const std::string& str) {
    std::vector<float> vals;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        try { vals.push_back(std::stof(item)); } catch(...) { vals.push_back(0.0f); }
    }
    Vector3 v = {0.0f, 0.0f, 0.0f};
    if (vals.size() > 0) v.x = vals[0];
    if (vals.size() > 1) v.y = vals[1];
    if (vals.size() > 2) v.z = vals[2];
    return v;
}

std::string Config::StringListToString(const std::vector<std::string> &list) {
    std::stringstream ss;
    for (size_t i = 0; i < list.size(); ++i) {
        ss << list[i];
        if (i < list.size() - 1) ss << ",";
    }
    return ss.str();
}

std::vector<std::string> Config::ParseStringList(const std::string &str) {
    std::vector<std::string> list;
    if (str.empty()) return list;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) list.push_back(item);
    }
    return list;
}

void Config::Load() {
    std::ifstream file(m_configPath);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        size_t delim = line.find('=');
        if (delim != std::string::npos) {
            std::string key = line.substr(0, delim);
            std::string value = line.substr(delim + 1);

            while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
            size_t vstart = value.find_first_not_of(" \t");
            if (vstart != std::string::npos) value = value.substr(vstart);
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();

            if (key == "LastTexturePath") LastTexturePath = value;
            else if (key == "LastIPDPath") LastIPDPath = value;
            else if (key == "IsHiDPI") IsHiDPI = (value == "1" || value == "true");
            else if (key == "TextureScale") TextureScale = std::stof(value);
            else if (key == "ProjectDirectory") ProjectDirectory = value;
            else if (key == "GameDirectory") GameDirectory = value;
            else if (key == "SelectedPrefix") SelectedPrefix = value;
            else if (key == "LastMapKey") LastMapKey = value;
            else if (key == "ColorSelected") ColorSelected = StringToColor(value);
            else if (key == "ColorWorkspace") ColorWorkspace = StringToColor(value);
            else if (key == "ColorDeployment") ColorDeployment = StringToColor(value);
            else if (key == "ColorUnloaded") ColorUnloaded = StringToColor(value);
            else if (key == "UndoDepth") UndoDepth = std::stoi(value);
            else if (key == "ShowMajorGridlines") ShowMajorGridlines = (value == "1" || value == "true");
            else if (key == "ShowMinorGridlines") ShowMinorGridlines = (value == "1" || value == "true");
            else if (key == "GridCellSize") GridCellSize = std::stof(value);
            else if (key == "EnableDitheringMode") EnableDitheringMode = (value == "1" || value == "true");
            else if (key == "ShowPersistentWireframe") ShowPersistentWireframe = (value == "1" || value == "true");
            else if (key == "WireframeColor") WireframeColor = StringToColor(value);
            else if (key == "WireframeThickness") WireframeThickness = std::stof(value);
            else if (key == "KeyMoveForward") KeyMoveForward = std::stoi(value);
            else if (key == "KeyMoveBackward") KeyMoveBackward = std::stoi(value);
            else if (key == "KeyMoveLeft") KeyMoveLeft = std::stoi(value);
            else if (key == "KeyMoveRight") KeyMoveRight = std::stoi(value);
            else if (key == "KeyMoveUp") KeyMoveUp = std::stoi(value);
            else if (key == "KeyMoveDown") KeyMoveDown = std::stoi(value);
            else if (key == "KeyMultiselect") KeyMultiselect = std::stoi(value);
            else if (key == "KeyCamMoveForward") KeyCamMoveForward = std::stoi(value);
            else if (key == "KeyCamMoveBackward") KeyCamMoveBackward = std::stoi(value);
            else if (key == "KeyCamMoveLeft") KeyCamMoveLeft = std::stoi(value);
            else if (key == "KeyCamMoveRight") KeyCamMoveRight = std::stoi(value);
            else if (key == "KeyCamMoveUp") KeyCamMoveUp = std::stoi(value);
            else if (key == "KeyCamMoveUp") KeyCamMoveUp = std::stoi(value);
            else if (key == "KeyCamMoveDown") KeyCamMoveDown = std::stoi(value);
            else if (key == "PersistedSelection") PersistedSelection = value;
            else if (key == "PersistedViewportChunks") PersistedViewportChunks = value;
            else if (key == "PersistedCamAzimuth") PersistedCamAzimuth = std::stof(value);
            else if (key == "PersistedCamElevation") PersistedCamElevation = std::stof(value);
            else if (key == "PersistedCamDistance") PersistedCamDistance = std::stof(value);
            else if (key == "PersistedCamTarget") PersistedCamTarget = StringToVector3(value);
            else if (key == "PersistedToolsTab") PersistedToolsTab = std::stoi(value);
        }
    }
}

void Config::Save() {
    std::ofstream file(m_configPath);
    if (file.is_open()) {
        file << "LastTexturePath=" << LastTexturePath << "\n";
        file << "LastIPDPath=" << LastIPDPath << "\n";
        file << "IsHiDPI=" << (IsHiDPI ? "1" : "0") << "\n";
        file << "TextureScale=" << TextureScale << "\n";
        file << "ProjectDirectory=" << ProjectDirectory << "\n";
        file << "GameDirectory=" << GameDirectory << "\n";
        file << "SelectedPrefix=" << SelectedPrefix << "\n";
        file << "LastMapKey=" << LastMapKey << "\n";
        file << "ColorSelected=" << ColorToString(ColorSelected) << "\n";
        file << "ColorWorkspace=" << ColorToString(ColorWorkspace) << "\n";
        file << "ColorDeployment=" << ColorToString(ColorDeployment) << "\n";
        file << "ColorUnloaded=" << ColorToString(ColorUnloaded) << "\n";
        file << "UndoDepth=" << UndoDepth << "\n";
        file << "ShowMajorGridlines=" << (ShowMajorGridlines ? "1" : "0") << "\n";
        file << "ShowMinorGridlines=" << (ShowMinorGridlines ? "1" : "0") << "\n";
        file << "GridCellSize=" << GridCellSize << "\n";
        file << "EnableDitheringMode=" << (EnableDitheringMode ? "1" : "0") << "\n";
        file << "ShowPersistentWireframe=" << (ShowPersistentWireframe ? "1" : "0") << "\n";
        file << "WireframeColor=" << ColorToString(WireframeColor) << "\n";
        file << "WireframeThickness=" << WireframeThickness << "\n";
        file << "KeyMoveForward=" << KeyMoveForward << "\n";
        file << "KeyMoveBackward=" << KeyMoveBackward << "\n";
        file << "KeyMoveLeft=" << KeyMoveLeft << "\n";
        file << "KeyMoveRight=" << KeyMoveRight << "\n";
        file << "KeyMoveUp=" << KeyMoveUp << "\n";
        file << "KeyMoveDown=" << KeyMoveDown << "\n";
        file << "KeyMultiselect=" << KeyMultiselect << "\n";
        file << "KeyCamMoveForward=" << KeyCamMoveForward << "\n";
        file << "KeyCamMoveBackward=" << KeyCamMoveBackward << "\n";
        file << "KeyCamMoveLeft=" << KeyCamMoveLeft << "\n";
        file << "KeyCamMoveRight=" << KeyCamMoveRight << "\n";
        file << "KeyCamMoveUp=" << KeyCamMoveUp << "\n";
        file << "KeyCamMoveDown=" << KeyCamMoveDown << "\n";
        file << "PersistedSelection=" << PersistedSelection << "\n";
        file << "PersistedViewportChunks=" << PersistedViewportChunks << "\n";
        file << "PersistedCamAzimuth=" << PersistedCamAzimuth << "\n";
        file << "PersistedCamElevation=" << PersistedCamElevation << "\n";
        file << "PersistedCamDistance=" << PersistedCamDistance << "\n";
        file << "PersistedCamTarget=" << Vector3ToString(PersistedCamTarget) << "\n";
        file << "PersistedToolsTab=" << PersistedToolsTab << "\n";
    }
}

bool Config::IsMultiselectDown() const {
    bool isDown = IsKeyDown(KeyMultiselect);
    if (KeyMultiselect == KEY_LEFT_CONTROL || KeyMultiselect == KEY_RIGHT_CONTROL) {
        isDown = isDown || ImGui::GetIO().KeyCtrl;
    } else if (KeyMultiselect == KEY_LEFT_SHIFT || KeyMultiselect == KEY_RIGHT_SHIFT) {
        isDown = isDown || ImGui::GetIO().KeyShift;
    } else if (KeyMultiselect == KEY_LEFT_ALT || KeyMultiselect == KEY_RIGHT_ALT) {
        isDown = isDown || ImGui::GetIO().KeyAlt;
    }
    return isDown;
}
