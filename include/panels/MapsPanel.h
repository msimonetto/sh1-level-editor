#pragma once
#include "core/MapTable.h"
#include "viewport/WaypointsOverlay.h"
#include <string>

class WaypointsOverlay;

class MapsPanel {
public:
  MapsPanel() = default;
  ~MapsPanel() = default;

  void Draw(WaypointsOverlay *eventOverlay);

  std::string GetSelectedMapKey() const { return m_selectedMapKey; }
  void SetSelectedMapKey(const std::string &key) { m_selectedMapKey = key; }

private:
  std::string m_selectedMapKey = "MAP0_S00";
  char m_filterBuf[64] = {0};
};
