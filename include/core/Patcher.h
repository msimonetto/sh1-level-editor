#pragma once
#include <string>
#include <vector>

class EnginePatcher {
public:
    enum class Version {
        USA,
        EU,
        JP,
        ALL
    };

    EnginePatcher() = default;
    ~EnginePatcher() = default;

    static bool PatchMemoryAllocations(const std::string& overrideDir, const std::string& engineSrcDir, Version version);
    static bool RevertMemoryAllocations(const std::string& overrideDir, const std::string& engineSrcDir, Version version);
    static bool CheckPatchingRequired(const std::string& overrideDir, const std::string& engineSrcDir, Version version, std::vector<std::string>* outNeedsPatchingChunks = nullptr);
};
