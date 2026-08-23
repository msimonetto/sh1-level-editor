#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include "formats/Structs.h"
#include "formats/TIMDecoder.h"
class TIMEncoder {
public:
    static bool Encode(const DecodedTIM& tim, const std::string& outPath);
    static uint16_t ColorToWord(const TIMColor& c);
};
