#include "formats/GlobalCache.h"
#include "formats/Structs.h"
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// GlobalCache Implementation
// ---------------------------------------------------------------------------
GlobalCache& GlobalCache::Get() {
    static GlobalCache instance;
    return instance;
}

std::shared_ptr<const CachedGlobal> GlobalCache::GetOrLoad(const std::string& glbPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_cache.find(glbPath);
    if (it != m_cache.end()) {
        return it->second;
    }

    auto entry = std::make_shared<CachedGlobal>();
    entry->path = glbPath;

    FILE* gf = fopen(glbPath.c_str(), "rb");
    if (!gf) {
        entry->loaded = false;
        m_cache[glbPath] = entry;
        return entry;
    }

    fseek(gf, 0, SEEK_END);
    long gsz = ftell(gf);
    fseek(gf, 0, SEEK_SET);

    if (gsz < (long)sizeof(PLM_FILE_HEADER)) {
        fclose(gf);
        entry->loaded = false;
        m_cache[glbPath] = entry;
        return entry;
    }

    entry->buffer.resize(gsz);
    if (fread(entry->buffer.data(), 1, gsz, gf) != (size_t)gsz) {
        fclose(gf);
        entry->loaded = false;
        m_cache[glbPath] = entry;
        return entry;
    }
    fclose(gf);

    const PLM_FILE_HEADER* gh = (const PLM_FILE_HEADER*)entry->buffer.data();
    if (gh->id != 0x0630) {
        printf("[GlobalCache] GLB PLM magic mismatch (0x%04X) in %s\n", gh->id, glbPath.c_str());
        entry->loaded = false;
        m_cache[glbPath] = entry;
        return entry;
    }

    // Global texture names
    int gTexBase = (int)gh->tex_name_offset;
    for (int i = 0; i < gh->tex_num; ++i) {
        int off = gTexBase + i * 24;
        if (off + 24 > (int)entry->buffer.size()) break;
        char name[25] = {0};
        memcpy(name, entry->buffer.data() + off, 24);
        entry->globalTexNames.push_back(std::string(name));
    }

    // Build global obj lookup
    int gObjBase = (int)gh->obj_start_offset;
    for (int i = 0; i < gh->obj_num; ++i) {
        int off = gObjBase + i * (int)sizeof(PLM_OBJ_HEADER);
        if (off + (int)sizeof(PLM_OBJ_HEADER) > (int)entry->buffer.size()) break;
        const PLM_OBJ_HEADER* oh = (const PLM_OBJ_HEADER*)(entry->buffer.data() + off);
        char name[9] = {0};
        memcpy(name, oh->name, 8);
        entry->globalObjMap[std::string(name)] = off;
    }

    entry->loaded = true;
    printf("[GlobalCache] Loaded & cached GLB (%d objects, %d textures): %s\n",
           (int)entry->globalObjMap.size(), (int)entry->globalTexNames.size(), glbPath.c_str());

    m_cache[glbPath] = entry;
    return entry;
}

void GlobalCache::Invalidate(const std::string& glbPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.erase(glbPath);
}

void GlobalCache::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.clear();
}
