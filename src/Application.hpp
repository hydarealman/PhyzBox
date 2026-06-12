#pragma once

#include "NBodySystem.hpp"
#include "Renderer.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <chrono>

namespace phyz {

class Application {
public:
    Application() = default;
    ~Application();

    bool initialize();
    void run();

private:
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    bool createWindow();
    bool createOpenGLContext();
    void destroyOpenGLContext();
    void handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void handleKey(WPARAM key);
    void resetScenario(Scenario scenario);
    void resetCustomScenario();
    void update(double realDt);
    void render();
    void resize(int width, int height);
    [[nodiscard]] double automaticTimeScale() const;
    [[nodiscard]] bool speedSliderHitTest(int x, int y) const;
    void setSimulationSpeedFromSlider(int x);
    [[nodiscard]] int bodyHitTest(int x, int y) const;
    void beginBodyDrag(int x, int y);
    void updateBodyDrag(int x, int y, bool verticalOnly);
    void cameraFrame(Vec3& eye, Vec3& right, Vec3& up, Vec3& forward) const;
    void exportSnapshot() const;

    HWND hwnd_ = nullptr;
    HDC deviceContext_ = nullptr;
    HGLRC glContext_ = nullptr;

    Renderer renderer_;
    NBodySystem system_;
    InitialConditionConfig customConfig_;

    int width_ = 1280;
    int height_ = 720;
    bool running_ = true;
    bool paused_ = false;
    bool showTrails_ = true;
    bool showGrid_ = true;
    bool autoOrbit_ = true;
    bool autoTimeScale_ = true;
    bool showParticles_ = true;
    bool showShadow_ = true;
    bool showField_ = false;
    bool editMode_ = false;
    bool mouseDragging_ = false;
    bool speedSliderDragging_ = false;
    bool bodyDragging_ = false;
    POINT lastMouse_{};

    double cameraYaw_ = 0.85;
    double cameraPitch_ = 0.48;
    double cameraDistance_ = 5.8;
    double simulationSpeed_ = 0.25;
    double effectiveSimulationSpeed_ = 0.25;
    double physicsDt_ = 0.004;
    double simulationAccumulator_ = 0.0;
    double fps_ = 0.0;
    int focusIndex_ = -1;
    int selectedBody_ = -1;

    std::chrono::steady_clock::time_point lastFrameTime_{};
};

} // namespace phyz
