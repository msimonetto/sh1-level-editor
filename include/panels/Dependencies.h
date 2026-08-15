#pragma once

#include "core/ChunkManager.h"
#include <string>
#include <vector>

class DependenciesPanel {
public:
    DependenciesPanel(ChunkManager& chunkManager);
    ~DependenciesPanel() = default;

    void Render();

private:
    ChunkManager& m_chunkManager;
};
