#include "core/Config.h"
#include "imgui.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>

// Serialization helpers for Raylib types
inline void to_json(nlohmann::json& j, const Color& c) {
    j = nlohmann::json{{"r", c.r}, {"g", c.g}, {"b", c.b}, {"a", c.a}};
}

inline void from_json(const nlohmann::json& j, Color& c) {
    if (j.is_object()) {
        c.r = j.value("r", (unsigned char)0);
        c.g = j.value("g", (unsigned char)0);
        c.b = j.value("b", (unsigned char)0);
        c.a = j.value("a", (unsigned char)255);
    } else if (j.is_array() && j.size() >= 3) {
        c.r = j[0].get<unsigned char>();
        c.g = j[1].get<unsigned char>();
        c.b = j[2].get<unsigned char>();
        c.a = (j.size() > 3) ? j[3].get<unsigned char>() : (unsigned char)255;
    }
}

inline void to_json(nlohmann::json& j, const Vector3& v) {
    j = nlohmann::json{{"x", v.x}, {"y", v.y}, {"z", v.z}};
}

inline void from_json(const nlohmann::json& j, Vector3& v) {
    if (j.is_object()) {
        v.x = j.value("x", 0.0f);
        v.y = j.value("y", 0.0f);
        v.z = j.value("z", 0.0f);
    } else if (j.is_array() && j.size() >= 3) {
        v.x = j[0].get<float>();
        v.y = j[1].get<float>();
        v.z = j[2].get<float>();
    }
}

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

    try {
        nlohmann::json config;
        file >> config;

        LastTexturePath = config.value("LastTexturePath", LastTexturePath);
        LastIPDPath = config.value("LastIPDPath", LastIPDPath);
        IsHiDPI = config.value("IsHiDPI", IsHiDPI);
        ProjectDirectory = config.value("ProjectDirectory", ProjectDirectory);
        GameDirectory = config.value("GameDirectory", GameDirectory);
        SelectedPrefix = config.value("SelectedPrefix", SelectedPrefix);
        LastMapKey = config.value("LastMapKey", LastMapKey);

        ColorSelected = config.value("ColorSelected", ColorSelected);
        ColorWorkspace = config.value("ColorWorkspace", ColorWorkspace);
        ColorDeployment = config.value("ColorDeployment", ColorDeployment);
        ColorUnloaded = config.value("ColorUnloaded", ColorUnloaded);
        UndoDepth = config.value("UndoDepth", UndoDepth);

        ShowMajorGridlines = config.value("ShowMajorGridlines", ShowMajorGridlines);
        ShowMinorGridlines = config.value("ShowMinorGridlines", ShowMinorGridlines);
        GridCellSize = config.value("GridCellSize", GridCellSize);
        EnableDitheringMode = config.value("EnableDitheringMode", EnableDitheringMode);
        ShowPersistentWireframe = config.value("ShowPersistentWireframe", ShowPersistentWireframe);
        WireframeColor = config.value("WireframeColor", WireframeColor);
        WireframeThickness = config.value("WireframeThickness", WireframeThickness);

        KeyMoveForward = config.value("KeyMoveForward", KeyMoveForward);
        KeyMoveBackward = config.value("KeyMoveBackward", KeyMoveBackward);
        KeyMoveLeft = config.value("KeyMoveLeft", KeyMoveLeft);
        KeyMoveRight = config.value("KeyMoveRight", KeyMoveRight);
        KeyMoveUp = config.value("KeyMoveUp", KeyMoveUp);
        KeyMoveDown = config.value("KeyMoveDown", KeyMoveDown);
        KeyMultiselect = config.value("KeyMultiselect", KeyMultiselect);

        KeyCamMoveForward = config.value("KeyCamMoveForward", KeyCamMoveForward);
        KeyCamMoveBackward = config.value("KeyCamMoveBackward", KeyCamMoveBackward);
        KeyCamMoveLeft = config.value("KeyCamMoveLeft", KeyCamMoveLeft);
        KeyCamMoveRight = config.value("KeyCamMoveRight", KeyCamMoveRight);
        KeyCamMoveUp = config.value("KeyCamMoveUp", KeyCamMoveUp);
        KeyCamMoveDown = config.value("KeyCamMoveDown", KeyCamMoveDown);

        PersistedSelection = config.value("PersistedSelection", PersistedSelection);
        PersistedViewportChunks = config.value("PersistedViewportChunks", PersistedViewportChunks);
        PersistedCamAzimuth = config.value("PersistedCamAzimuth", PersistedCamAzimuth);
        PersistedCamElevation = config.value("PersistedCamElevation", PersistedCamElevation);
        PersistedCamDistance = config.value("PersistedCamDistance", PersistedCamDistance);
        PersistedCamTarget = config.value("PersistedCamTarget", PersistedCamTarget);
        PersistedToolsTab = config.value("PersistedToolsTab", PersistedToolsTab);
    } catch (const std::exception& e) {
        // Fallback gracefully if corrupted
    }
}

void Config::Save() {
    nlohmann::json config;
    config["LastTexturePath"] = LastTexturePath;
    config["LastIPDPath"] = LastIPDPath;
    config["IsHiDPI"] = IsHiDPI;
    config["ProjectDirectory"] = ProjectDirectory;
    config["GameDirectory"] = GameDirectory;
    config["SelectedPrefix"] = SelectedPrefix;
    config["LastMapKey"] = LastMapKey;

    config["ColorSelected"] = ColorSelected;
    config["ColorWorkspace"] = ColorWorkspace;
    config["ColorDeployment"] = ColorDeployment;
    config["ColorUnloaded"] = ColorUnloaded;
    config["UndoDepth"] = UndoDepth;

    config["ShowMajorGridlines"] = ShowMajorGridlines;
    config["ShowMinorGridlines"] = ShowMinorGridlines;
    config["GridCellSize"] = GridCellSize;
    config["EnableDitheringMode"] = EnableDitheringMode;
    config["ShowPersistentWireframe"] = ShowPersistentWireframe;
    config["WireframeColor"] = WireframeColor;
    config["WireframeThickness"] = WireframeThickness;

    config["KeyMoveForward"] = KeyMoveForward;
    config["KeyMoveBackward"] = KeyMoveBackward;
    config["KeyMoveLeft"] = KeyMoveLeft;
    config["KeyMoveRight"] = KeyMoveRight;
    config["KeyMoveUp"] = KeyMoveUp;
    config["KeyMoveDown"] = KeyMoveDown;
    config["KeyMultiselect"] = KeyMultiselect;

    config["KeyCamMoveForward"] = KeyCamMoveForward;
    config["KeyCamMoveBackward"] = KeyCamMoveBackward;
    config["KeyCamMoveLeft"] = KeyCamMoveLeft;
    config["KeyCamMoveRight"] = KeyCamMoveRight;
    config["KeyCamMoveUp"] = KeyCamMoveUp;
    config["KeyCamMoveDown"] = KeyCamMoveDown;

    config["PersistedSelection"] = PersistedSelection;
    config["PersistedViewportChunks"] = PersistedViewportChunks;
    config["PersistedCamAzimuth"] = PersistedCamAzimuth;
    config["PersistedCamElevation"] = PersistedCamElevation;
    config["PersistedCamDistance"] = PersistedCamDistance;
    config["PersistedCamTarget"] = PersistedCamTarget;
    config["PersistedToolsTab"] = PersistedToolsTab;

    std::ofstream file(m_configPath);
    if (file.is_open()) {
        file << config.dump(4) << "\n";
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

