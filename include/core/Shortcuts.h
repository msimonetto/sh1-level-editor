#pragma once

class History;
class ChunkManager;
class Viewport;
class LocalGeometryOverlay;
class WaypointsOverlay;

class Shortcuts {
public:
  void Handle(History &history, ChunkManager &pipelineManager,
              Viewport &sceneViewport,
              LocalGeometryOverlay &localGeometryOverlay,
              WaypointsOverlay *eventViewport = nullptr);
  void SaveSelected(ChunkManager &pipelineManager,
                    Viewport &sceneViewport);
  void SaveAll(ChunkManager &pipelineManager, Viewport &sceneViewport);
};
