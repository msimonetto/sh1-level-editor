#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "formats/Structs.h"
#include "formats/IPDParse.h"

class PLMParse {
public:
    static void ComputeLocalVertex(int16_t px, int16_t py, int16_t pz, float& x, float& y, float& z);
    
    static void ApplyWorldTransform(float lx, float ly, float lz,
                                      const float rot[3][3],
                                      float tx, float ty, float tz,
                                      float& wx, float& wy, float& wz);

    static bool ParseAndPlaceObject(const uint8_t*                  buf,
                                    size_t                          bufSize,
                                    int                             plmBase,
                                    const PLM_OBJ_HEADER*           objHdr,
                                    const std::vector<std::string>& texNames,
                                    bool                            isGlobal,
                                    float                           worldTx,
                                    float                           worldTy,
                                    float                           worldTz,
                                    float                           rot[3][3],
                                    const IPD_OBJ_DATA*             rawDta,
                                    RenderObject&                   outObj);

    static bool ParseGlbFile(const std::string&             glbPath,
                              std::vector<RenderObject>&     outObjects,
                              std::vector<std::string>&      outTexNames,
                              std::vector<IPDParse::GlbObjectInfo>& outInfo);
};
