#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <mutex>

// ---------------------------------------------------------------------------
// CachedGlobal — in-memory parsed representation of a _GLB.PLM binary library.
// ---------------------------------------------------------------------------
struct CachedGlobal {
    std::string path;
    std::vector<uint8_t> buffer;
    std::vector<std::string> globalTexNames;
    std::map<std::string, int> globalObjMap; // 8-char object name -> byte offset in buffer
    bool loaded = false;
};

// ---------------------------------------------------------------------------
// GlobalCache — thread-safe in-memory cache for _GLB.PLM binary files.
// Avoids repeated disk I/O and header parsing when multiple chunks share
// the same global object library.
// ---------------------------------------------------------------------------
class GlobalCache {
public:
    static GlobalCache& Get();

    // Fetches from RAM or loads and parses once from disk (thread-safe).
    std::shared_ptr<const CachedGlobal> GetOrLoad(const std::string& glbPath);

    // Invalidate a single cached file (e.g. after IPDWrite patches global faces).
    void Invalidate(const std::string& glbPath);

    // Clear all entries (e.g. on workspace clear/reload).
    void Clear();

private:
    GlobalCache() = default;

    std::map<std::string, std::shared_ptr<CachedGlobal>> m_cache;
    std::mutex m_mutex;
};
