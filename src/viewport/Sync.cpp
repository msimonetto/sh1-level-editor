#include "core/ChunkManager.h"
#include "core/IPDParse.h"
#include "core/OverlayLoader.h"
#include "core/Textures.h"
#include "viewport/Collision.h"
#include "viewport/LocalGeometry.h"
#include "viewport/Viewport.h"
#include "viewport/Sync.h"
#include "viewport/Waypoints.h"
#include <algorithm>
#include <cstdio>
#include <memory>


void ViewportSync::Initialize(Viewport &sceneViewport,
                              LocalGeometryOverlay &localGeometryOverlay) {
  localGeometryOverlay.SetSharedChunks(&sceneViewport.GetChunks());
}

void ViewportSync::Update(ChunkManager &pipelineManager, Viewport &sceneViewport,
                          LocalGeometryOverlay &localGeometryOverlay) {
  // Auto-sync viewports with ChunkManager's RMB selection
  auto curViewport = pipelineManager.GetViewportChunks();
  std::string workspaceDir = pipelineManager.GetWorkspaceDir();

  // Additions (Spawn async task)
  int newLoads = 0;
  for (const auto &c : curViewport) {
    if (std::find(lastViewportChunks.begin(), lastViewportChunks.end(), c) ==
        lastViewportChunks.end()) {
      // Check if it's already loading
      if (std::find(m_loadingChunks.begin(), m_loadingChunks.end(), c) ==
          m_loadingChunks.end()) {
        m_loadingChunks.push_back(c);
        newLoads++;

        std::string path = workspaceDir + "/chunks/" + c + ".IPD";

        // Spawn detached thread
        std::thread([this, path, workspaceDir, c]() {
          auto parsedChunk = std::make_shared<ParsedChunk>();
          if (IPDParse::Parse(path, workspaceDir, *parsedChunk)) {
            IPDParse::BuildBatches(*parsedChunk);

            // Preload textures on the background thread
            std::vector<std::string> texToLoad;
            for (const auto &batch : parsedChunk->batches) {
              if (!batch.texName.empty()) {
                texToLoad.push_back(batch.texName);
              }
            }
            std::sort(texToLoad.begin(), texToLoad.end());
            texToLoad.erase(std::unique(texToLoad.begin(), texToLoad.end()),
                            texToLoad.end());

            for (const auto &t : texToLoad) {
              TextureCache::Get().Preload(t, workspaceDir);
            }

            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_completedLoads.push_back(AsyncLoadResult{c, parsedChunk});
          } else {
            printf("[Main] Failed to parse %s\n", path.c_str());
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_completedLoads.push_back(AsyncLoadResult{c, nullptr});
          }
        }).detach();
      }
    }
  }

  if (newLoads > 0) {
    if (m_loadingChunks.size() == (size_t)newLoads) {
      m_totalLoadingTasks = newLoads;
      m_finishedTasks = 0;
    } else {
      m_totalLoadingTasks += newLoads;
    }
  }

  // Process completed async tasks (One per frame to avoid UI stutter)


  {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (!m_completedLoads.empty()) {
      auto res = m_completedLoads.front();
      m_completedLoads.erase(m_completedLoads.begin());

      if (res.parsedChunk) {
        sceneViewport.LoadChunk(res.parsedChunk, workspaceDir);
        auto* collisionOverlay = sceneViewport.GetOverlay<CollisionOverlay>(ViewportMode::Collision);
        if (collisionOverlay) {
          collisionOverlay->LoadChunk(*res.parsedChunk);
        }
        localGeometryOverlay.SetSharedChunks(&sceneViewport.GetChunks());

        auto* eventOverlay = sceneViewport.GetOverlay<WaypointsOverlay>(ViewportMode::DoorsAndWaypoints);
        if (eventOverlay) {
          eventOverlay->SetSharedChunks(&sceneViewport.GetChunks());
          if (!eventOverlay->GetOverlay().loaded) {
            std::string mapKey =
                OverlayLoader::GetMapKeyForChunk(res.chunkName);
            eventOverlay->LoadOverlay(mapKey);
          }
        }
      }
      // Remove from loading tracking list
      auto it = std::find(m_loadingChunks.begin(), m_loadingChunks.end(),
                          res.chunkName);
      if (it != m_loadingChunks.end())
        m_loadingChunks.erase(it);

      m_finishedTasks++;
    }

    if (m_loadingChunks.empty()) {
      m_totalLoadingTasks = 0;
      m_finishedTasks = 0;
    }
  }

  // Removals
  for (const auto &c : lastViewportChunks) {
    if (std::find(curViewport.begin(), curViewport.end(), c) ==
        curViewport.end()) {
      sceneViewport.UnloadChunk(c);
      auto* collisionOverlay = sceneViewport.GetOverlay<CollisionOverlay>(ViewportMode::Collision);
      if (collisionOverlay) {
        collisionOverlay->UnloadChunk(c);
      }
    }
  }
  lastViewportChunks = curViewport;

  // Auto-sync cameras between viewports
  // Note: Since they share the same Viewport, they inherently share the same camera.
  // We no longer overwrite localGeometryOverlay's selection here because it handles its own picking state.
}

void ViewportSync::ForceReloadChunk(const std::string &chunk,
                                    Viewport &sceneViewport,
                                    LocalGeometryOverlay &localGeometryOverlay) {
  auto it =
      std::find(lastViewportChunks.begin(), lastViewportChunks.end(), chunk);
  if (it != lastViewportChunks.end()) {
    lastViewportChunks.erase(it);
    sceneViewport.UnloadChunk(chunk);
    auto* collisionOverlay = sceneViewport.GetOverlay<CollisionOverlay>(ViewportMode::Collision);
    if (collisionOverlay) {
      collisionOverlay->UnloadChunk(chunk);
    }
    auto* sceneLocalGeometryOverlay = sceneViewport.GetOverlay<LocalGeometryOverlay>(ViewportMode::LocalGeometry);
    if (sceneLocalGeometryOverlay) {
      sceneLocalGeometryOverlay->UnloadChunk(chunk);
    }
  }
}
