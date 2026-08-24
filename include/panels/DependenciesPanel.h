#pragma once

#include "core/FileManager.h"
#include <string>
#include <vector>

class DependenciesPanel {
public:
    DependenciesPanel(FileManager& chunkManager);
    ~DependenciesPanel() = default;

    void Render();

private:
    FileManager& m_chunkManager;
};
