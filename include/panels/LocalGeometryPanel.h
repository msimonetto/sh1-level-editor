#pragma once
#include "geometry/ChunkValidator.h"
#include <string>
#include <vector>

class LocalGeometryOverlay;
class History;

// ---------------------------------------------------------------------------
// LocalGeometryPanel — Tools and inspector panel for Local Geometry editing.
//
// Provides a Blender-inspired toolset across 4 discrete modes:
// 1. Global Objects: Instantiated _GLB.PLM prop placement and transforms.
// 2. Meshes: Chunk mesh containers, whole-mesh transforms, and primitives.
// 3. Faces: Low-poly polygonal topology, subdivisions, and UV texturing.
// 4. Vertices: Point-level snapping, planarization, welding, and face creation.
// ---------------------------------------------------------------------------
class LocalGeometryPanel {
public:
  LocalGeometryPanel() = default;
  ~LocalGeometryPanel() = default;

  // Draw the Local Geometry tools UI
  void DrawContent(LocalGeometryOverlay &overlay, History *history = nullptr);

private:
  // Primitive creation dimensions
  float m_primWidth = 1.0f;
  float m_primHeight = 1.0f;
  float m_primLength = 1.0f;

  // Transform / tool parameters
  float m_weldTolerance = 0.05f;
  float m_extrudeDistance = 1.0f;
  int m_faceMoveMode = 0; // 0 = Extrude, 1 = Separate

  // Global object placement state
  int m_selectedGlbPropIdx = 0;
  std::vector<std::string> m_cachedGlbPropNames;
  std::string m_lastGlbPrefix;

  // Validator UI State
  ValidationResult m_lastValidationResult;
  bool m_hasRunValidation = false;

  void DrawSelectionHeader(LocalGeometryOverlay &overlay);
  void DrawGlobalObjectsSection(LocalGeometryOverlay &overlay, History *history);
  void DrawMeshesSection(LocalGeometryOverlay &overlay, History *history);
  void DrawFacesSection(LocalGeometryOverlay &overlay, History *history);
  void DrawVerticesSection(LocalGeometryOverlay &overlay, History *history);
  void DrawValidationSection(LocalGeometryOverlay &overlay);
};
