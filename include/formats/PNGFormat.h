#pragma once
#include <string>
#include "formats/TIMDecoder.h"

class PNGFormat {
public:
    static bool Save(const DecodedTIM& tim, const std::string& outPathStem);
    static bool Load(const std::string& inPathStem, DecodedTIM& outTim);
};
