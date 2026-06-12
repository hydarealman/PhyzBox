#pragma once

#include "NBodySystem.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GL/gl.h>

#include <random>
#include <vector>

namespace phyz {

struct RenderState {
    int width = 1280;
    int height = 720;
    double cameraYaw = 0.85;
    double cameraPitch = 0.48;
    double cameraDistance = 5.8;
    double simulationSpeed = 1.0;
    double baseSimulationSpeed = 1.0;
    double physicsDt = 0.004;
    double fps = 0.0;
    bool paused = false;
    bool showTrails = true;
    bool showGrid = true;
    bool autoOrbit = true;
    bool autoTimeScale = true;
    int focusIndex = -1;
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    bool initialize(HDC deviceContext);
    void resize(int width, int height);
    void render(const NBodySystem& system, const RenderState& state);
    void cleanup();

private:
    struct Star {
        Vec3 position;
        Color color;
        float size = 1.0f;
    };

    void createFont();
    void createStars();
    void setupCamera(const NBodySystem& system, const RenderState& state);
    void renderStars();
    void renderReferenceGrid();
    void renderDepthCues(const NBodySystem& system);
    void renderTrails(const NBodySystem& system);
    void renderGlows(const NBodySystem& system);
    void renderBodies(const NBodySystem& system, int focusIndex);
    void renderFocusMarker(const Body& body);
    void renderSphere(double radius, int stacks, int slices);
    void renderHud(const NBodySystem& system, const RenderState& state);
    void drawText(float x, float y, const std::string& text, Color color);
    Vec3 cameraTarget(const NBodySystem& system, int focusIndex) const;

    HDC deviceContext_ = nullptr;
    GLuint fontBase_ = 0;
    int width_ = 1280;
    int height_ = 720;
    std::vector<Star> stars_;
};

} // namespace phyz
