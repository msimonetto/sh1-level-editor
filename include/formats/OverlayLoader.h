#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct WaypointData {
    int         index = 0;
    int32_t     rawX = 0;
    int32_t     rawZ = 0;
    float       worldX = 0.0f;
    float       worldZ = 0.0f;
    int         arrivalAngleDeg = 0;
    int         triggerParam0 = 0;
    int         triggerParam1 = 0;
    std::string paperMapIdx = "PaperMapIdx_OtherPlaces";
    int         paperMapIdxValue = 0;
    std::string loadingScreenId = "LoadingScreenId_None";
    int         loadingScreenIdValue = 0;
    int         field_4_5 = 0;
    int         unused_4_12 = 0;
    bool        dirty = false;
};

struct LinkData {
    int         index = 0;
    int         waypointIdx = 0;
    std::string destMapKey = "MapIdx_None";
    int         destMapIdx = 0;
    std::string triggerType = "TriggerType_None";
    int         triggerTypeValue = 0;
    std::string activationType = "TriggerActivationType_None";
    int         activationTypeValue = 0;
    std::string sysState = "SysState_Gameplay";
    int         sysStateValue = 0;
    int         eventParam = 0;
    int         requiredEventFlag = 0;
    std::string disabledEventFlag = "EventFlag_None";
    int         disabledEventFlagValue = 0;
    std::string requiredItemId = "InvItemId_None";
    int         requiredItemIdValue = 0;
    int         flags_8_13 = 0;
    std::string sfxPairIdx = "SfxPairIdx_None";
    int         sfxPairIdxValue = 0;
    int         field_8_24 = 0;
    bool        dirty = false;
};

struct OverlayMapData {
    std::string mapKey; // e.g. "MAP0_S00"
    std::vector<WaypointData> waypoints;
    std::vector<LinkData> links;
    bool loaded = false;
    bool dirty = false;
};

class OverlayLoader {
public:
    static bool Load(const std::string& mapKey, OverlayMapData& outData);
    static bool Save(const std::string& mapKey, const OverlayMapData& data);
    static std::string GetMapKeyForChunk(const std::string& chunkName);
};
