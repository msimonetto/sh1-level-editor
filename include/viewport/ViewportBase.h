#pragma once
#include <string>
#include <vector>
#include <cmath>
#include <functional>
#include <memory>
#include "raylib.h"
#include "rlgl.h"
#include "formats/IPDParse.h"

// ---------------------------------------------------------------------------
// ViewportBase — Base class for 3D rendering panels via Raylib + rlImGui
//
// Manages the RenderTarget, Orbital Camera, Grid, and ImGui integration.
// Derived classes implement specific scene drawing logic.
// ---------------------------------------------------------------------------
enum class ProjectionMode {
    Perspective = 0,
    OrthoTop,
    OrthoFront,
    OrthoBack,
    OrthoLeft,
    OrthoRight
};

struct GpuBatch {
    std::string texName;
    uint8_t   paletteRow;
    Mesh      mesh;
    Material  material;
    bool      meshUploaded;
    
    GpuBatch() : texName(""), paletteRow(0), mesh{0}, material{0}, meshUploaded(false) {}
    
    GpuBatch(const GpuBatch&) = delete;
    GpuBatch& operator=(const GpuBatch&) = delete;
    
    GpuBatch(GpuBatch&& other) noexcept 
        : texName(std::move(other.texName)), paletteRow(other.paletteRow),
          mesh(other.mesh), material(other.material), meshUploaded(other.meshUploaded) {
        other.meshUploaded = false;
        other.mesh = {0};
        other.material = {0};
    }
    
    GpuBatch& operator=(GpuBatch&& other) noexcept {
        if (this != &other) {
            if (meshUploaded) {
                UnloadMesh(mesh);
                if (material.maps != nullptr) {
                    material.maps[MATERIAL_MAP_DIFFUSE].texture = {0};
                    material.shader.id = rlGetShaderIdDefault();
                    UnloadMaterial(material);
                }
            }
            texName = std::move(other.texName);
            paletteRow = other.paletteRow;
            mesh = other.mesh;
            material = other.material;
            meshUploaded = other.meshUploaded;
            
            other.meshUploaded = false;
            other.mesh = {0};
            other.material = {0};
        }
        return *this;
    }
    
    ~GpuBatch() {
        if (meshUploaded) {
            UnloadMesh(mesh);
            if (material.maps != nullptr) {
                material.maps[MATERIAL_MAP_DIFFUSE].texture = {0};
                material.shader.id = rlGetShaderIdDefault();
                UnloadMaterial(material);
            }
            meshUploaded = false;
        }
    }
};

struct LoadedChunk {
    std::shared_ptr<ParsedChunk> data;
    std::vector<GpuBatch> batches;
    bool    visible  = true;
    bool    hasError = false;
    std::string errorMsg;
    BoundingBox bounds;
    
    LoadedChunk() = default;
    LoadedChunk(const LoadedChunk&) = delete;
    LoadedChunk& operator=(const LoadedChunk&) = delete;
    LoadedChunk(LoadedChunk&&) noexcept = default;
    LoadedChunk& operator=(LoadedChunk&&) noexcept = default;
};

struct ViewportCameraState {
    float azimuth;
    float elevation;
    float distance;
    Vector3 target;
    ProjectionMode projMode;
};

struct ChunkLocation {
    std::string name;
    int xPos;
    int yPos;
    BoundingBox bounds;
};

class ViewportBase {
public:
    ViewportBase(const std::string& panelName);
    virtual ~ViewportBase();

    void UnloadChunk(const std::string& chunkName);
    
    virtual std::vector<ChunkLocation> GetChunkLocations() const { return {}; }
    
    const std::string& GetPanelName() const { return m_panelName; }

    // Optional legend color callback

    std::function<Color(const std::string&)> m_legendColorCallback;

    bool IsHovered() const { return m_hovered; }
    bool IsFocused() const { return m_focused; }

    int GetWidth() const { return m_rtWidth; }
    int GetHeight() const { return m_rtHeight; }
    Vector2 GetLocalMousePos() const { return m_localMousePos; }

    ViewportCameraState GetCameraState() const {
        return { m_azimuth, m_elevation, m_distance, m_camera.target, m_projMode };
    }

    const Camera3D& GetCamera() const { return m_camera; }

    void ResetCamera();

    void SetCameraState(const ViewportCameraState& state) {
        if (m_projMode != state.projMode) {
            // Only sync target and zoom if projection modes differ, keep rotation decoupled
            m_camera.target = state.target;
            m_distance = state.distance;
        } else {
            // Sync fully
            m_azimuth = state.azimuth;
            m_elevation = state.elevation;
            m_distance = state.distance;
            m_camera.target = state.target;
            m_projMode = state.projMode;
        }
        UpdateCameraVectors();
    }

    // Draw the ImGui panel (call between rlImGuiBegin / rlImGuiEnd)
    virtual void Draw();

    // Draw only the 3D canvas inside a specific pixel dimension
    void DrawViewportCanvas(int w, int h);

    // Remove all loaded chunks
    void UnloadAll();

protected:
    std::string m_panelName;

    // Raylib render target (resized when panel size changes)
    RenderTexture2D m_renderTarget;
    RenderTexture2D m_postProcessTarget;
    Shader m_ditherShader;
    bool m_shaderLoaded = false;
    int m_rtWidth  = 0;
    int m_rtHeight = 0;

    // Orbital camera
    Camera3D m_camera;

    // ImGui panel state
    bool m_hovered     = false;
    bool m_focused     = false;
    Vector2 m_localMousePos = {0};
    
    // Box selection state
    bool m_isBoxSelecting = false;
    Vector2 m_boxSelectStart = {0};
    Vector2 m_boxSelectEnd = {0};

    // Grid rendering
    static inline bool m_showChunkLegend = false;

    // Manual camera state (spherical coords around target)
    float m_azimuth   = 45.0f;   // degrees, horizontal angle
    float m_elevation = 30.0f;   // degrees, vertical angle (clamped -89..89)
    float m_distance  = 35.0f;   // distance from target
    float m_moveSpeedMultiplier = 0.40f;
    ProjectionMode m_projMode = ProjectionMode::Perspective;

    // Resize render target if needed
    void EnsureRenderTarget(int w, int h);

    // Update camera state from user input
    void UpdateCamera();
    void UpdateCameraVectors();

    void DrawCustomGrid(float extent = 320.0f);
    void DrawCustomGrid(float minX, float maxX, float minZ, float maxZ);
    void DrawChunkLegend(std::vector<std::pair<std::string, Vector2>>& outLabels);

    void Shutdown();

    // --- Virtual interface for derived classes ---

    // Draw the background grid (called between BeginMode3D/EndMode3D before DrawScene)
    virtual void DrawViewportGrid() { DrawCustomGrid(); }

    // Draw the actual scene contents (called between BeginMode3D/EndMode3D)
    virtual void DrawScene() = 0;

    // Draw context menu for the viewport (called within an ImGui Popup)
    virtual void DrawContextMenu() {}

    // Return the number of chunks loaded, for display in the toolbar
    virtual size_t GetChunkCount() const = 0;

    // Optional: hook for unloading resources before base shutdown
    virtual void OnUnloadAll() {}

    // Handle picking from mouse ray
    virtual void HandlePicking(Ray ray) {}
    
    // Handle picking from marquee box
    virtual void HandleBoxPicking(Rectangle box) {}
};
