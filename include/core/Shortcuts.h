#pragma once

class History;
class FileManager;
class Viewport;
class LocalGeometryOverlay;
class WaypointsOverlay;

class Shortcuts {
public:
  void Handle(History &history, FileManager &fileManager,
              Viewport &sceneViewport,
              LocalGeometryOverlay &localGeometryOverlay,
              WaypointsOverlay *eventViewport = nullptr);
  void SaveSelected(FileManager &fileManager,
                    Viewport &sceneViewport);
  void SaveAll(FileManager &fileManager, Viewport &sceneViewport);
};
