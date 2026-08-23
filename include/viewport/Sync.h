#pragma once
#include "formats/Structs.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class FileManager;
class SceneOverlay;
class LocalGeometryOverlay;
struct ParsedChunk;

struct AsyncLoadResult {
  std::string chunkName;
  std::shared_ptr<ParsedChunk> parsedChunk;
};

class ViewportSync {
public:
  void Initialize(class Viewport &sceneViewport,
                  class LocalGeometryOverlay &localGeometryOverlay);

  void Update(class FileManager &fileManager, class Viewport &sceneViewport,
              class LocalGeometryOverlay &localGeometryOverlay);

  // Returns true if chunks are loading, and populates current/total progress
  void ForceReloadChunk(const std::string &chunk, class Viewport &sceneViewport,
                        LocalGeometryOverlay &localGeometryOverlay);

  bool GetLoadingProgress(int &outFinished, int &outTotal) const {
    if (m_loadingChunks.empty())
      return false;
    outFinished = m_finishedTasks;
    outTotal = m_totalLoadingTasks;
    return true;
  }

private:
  std::vector<std::string> lastViewportChunks;

  std::vector<std::string> m_loadingChunks; // Track what's currently loading
  std::vector<AsyncLoadResult> m_completedLoads; // Queue of finished tasks
  std::mutex m_queueMutex;

  int m_totalLoadingTasks = 0;
  int m_finishedTasks = 0;
};
