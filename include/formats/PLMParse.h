#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <mutex>
#include "formats/Structs.h"
#include "formats/IPDParse.h"

// ---------------------------------------------------------------------------
// CachedGlb — in-memory parsed representation of a _GLB.PLM binary library.
// ---------------------------------------------------------------------------
struct CachedGlb {
    std::string path;
    std::vector<uint8_t> buffer;
    std::vector<std::string> globalTexNames;
    std::map<std::string, int> globalObjMap; // 8-char object name -> byte offset in buffer
    bool loaded = false;
};

// ---------------------------------------------------------------------------
// GlbCache — thread-safe in-memory cache for _GLB.PLM binary files.
// Avoids repeated disk I/O and header parsing when multiple chunks share
// the same global object library.
// ---------------------------------------------------------------------------
class GlbCache {
public:
    static GlbCache& Get();

    // Fetches from RAM or loads and parses once from disk (thread-safe).
    std::shared_ptr<const CachedGlb> GetOrLoad(const std::string& glbPath);

    // Invalidate a single cached file (e.g. after IPDWrite patches global faces).
    void Invalidate(const std::string& glbPath);

    // Clear all entries (e.g. on workspace clear/reload).
    void Clear();

private:
    GlbCache() = default;

    std::map<std::string, std::shared_ptr<CachedGlb>> m_cache;
    std::mutex m_mutex;
};

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

