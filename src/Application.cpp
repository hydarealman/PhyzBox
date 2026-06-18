#include "Application.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <thread>
#include <windowsx.h>

namespace phyz {
namespace {

constexpr const char* WindowClassName = "PhyzBoxOpenGLWindow";
constexpr double MinSimulationSpeed = 0.01;
constexpr double MaxSimulationSpeed = 8.0;
constexpr int SpeedSliderHitLeft = 26;
constexpr int SpeedSliderHitRight = 526;
constexpr int SpeedSliderHitTop = 194;
constexpr int SpeedSliderHitBottom = 224;
constexpr int SpeedSliderLeft = 34;
constexpr int SpeedSliderRight = 518;
constexpr double CameraFovY = Pi / 4.0;

bool isPlusKey(WPARAM key) {
    return key == VK_OEM_PLUS || key == VK_ADD || key == '=';
}

bool isMinusKey(WPARAM key) {
    return key == VK_OEM_MINUS || key == VK_SUBTRACT || key == '-';
}

double speedFromSliderPosition(double t) {
    t = clamp(t, 0.0, 1.0);
    return MinSimulationSpeed * std::pow(MaxSimulationSpeed / MinSimulationSpeed, t);
}

} // namespace

Application::~Application() {
    renderer_.cleanup();
    destroyOpenGLContext();
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

bool Application::initialize() {
    if (!createWindow()) {
        return false;
    }
    if (!createOpenGLContext()) {
        return false;
    }

    renderer_.initialize(deviceContext_);
    renderer_.resize(width_, height_);
    customConfig_ = loadInitialConditionConfigFromDefaultLocations();
    if (customConfig_.enabled) {
        system_.reset(customConfig_);
    }
    physicsDt_ = system_.recommendedTimeStep();
    cameraDistance_ = system_.recommendedCameraDistance();
    lastFrameTime_ = std::chrono::steady_clock::now();
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

void Application::run() {
    MSG message{};

    while (running_) {
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                running_ = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        const auto now = std::chrono::steady_clock::now();
        double realDt = std::chrono::duration<double>(now - lastFrameTime_).count();
        lastFrameTime_ = now;
        realDt = std::min(realDt, 0.05);

        update(realDt);
        render();
        SwapBuffers(deviceContext_);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool Application::createWindow() {
    HINSTANCE instance = GetModuleHandleA(nullptr);

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Application::windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = WindowClassName;

    if (RegisterClassExA(&wc) == 0) {
        return false;
    }

    RECT rect{0, 0, width_, height_};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd_ = CreateWindowExA(
        0,
        WindowClassName,
        "PhyzBox - 3D Three Body Gravity Engine",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        this);

    return hwnd_ != nullptr;
}

bool Application::createOpenGLContext() {
    deviceContext_ = GetDC(hwnd_);
    if (deviceContext_ == nullptr) {
        return false;
    }

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    const int pixelFormat = ChoosePixelFormat(deviceContext_, &pfd);
    if (pixelFormat == 0 || SetPixelFormat(deviceContext_, pixelFormat, &pfd) == FALSE) {
        return false;
    }

    glContext_ = wglCreateContext(deviceContext_);
    if (glContext_ == nullptr) {
        return false;
    }

    return wglMakeCurrent(deviceContext_, glContext_) == TRUE;
}

void Application::destroyOpenGLContext() {
    if (glContext_ != nullptr) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(glContext_);
        glContext_ = nullptr;
    }
    if (deviceContext_ != nullptr && hwnd_ != nullptr) {
        ReleaseDC(hwnd_, deviceContext_);
        deviceContext_ = nullptr;
    }
}

LRESULT CALLBACK Application::windowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    Application* app = nullptr;

    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTA*>(lParam);
        app = static_cast<Application*>(createStruct->lpCreateParams);
        SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        app->hwnd_ = hwnd;
    } else {
        app = reinterpret_cast<Application*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    }

    if (app != nullptr) {
        app->handleMessage(message, wParam, lParam);
    }

    switch (message) {
    case WM_CLOSE:
        if (app != nullptr) {
            app->running_ = false;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (app != nullptr) {
            app->renderer_.cleanup();
            app->destroyOpenGLContext();
            app->hwnd_ = nullptr;
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcA(hwnd, message, wParam, lParam);
    }
}

void Application::handleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_SIZE:
        resize(LOWORD(lParam), HIWORD(lParam));
        break;
    case WM_LBUTTONDOWN:
        lastMouse_.x = GET_X_LPARAM(lParam);
        lastMouse_.y = GET_Y_LPARAM(lParam);
        if (speedSliderHitTest(lastMouse_.x, lastMouse_.y)) {
            speedSliderDragging_ = true;
            mouseDragging_ = false;
            bodyDragging_ = false;
            setSimulationSpeedFromSlider(lastMouse_.x);
        } else if (editMode_ && bodyHitTest(lastMouse_.x, lastMouse_.y) >= 0) {
            beginBodyDrag(lastMouse_.x, lastMouse_.y);
        } else {
            mouseDragging_ = true;
            speedSliderDragging_ = false;
            bodyDragging_ = false;
        }
        SetCapture(hwnd_);
        break;
    case WM_LBUTTONUP:
        mouseDragging_ = false;
        speedSliderDragging_ = false;
        bodyDragging_ = false;
        ReleaseCapture();
        break;
    case WM_MOUSEMOVE:
        if (speedSliderDragging_) {
            setSimulationSpeedFromSlider(GET_X_LPARAM(lParam));
        } else if (bodyDragging_) {
            updateBodyDrag(
                GET_X_LPARAM(lParam),
                GET_Y_LPARAM(lParam),
                (GetKeyState(VK_SHIFT) & 0x8000) != 0);
        } else if (mouseDragging_) {
            const int x = GET_X_LPARAM(lParam);
            const int y = GET_Y_LPARAM(lParam);
            const int dx = x - lastMouse_.x;
            const int dy = y - lastMouse_.y;
            lastMouse_.x = x;
            lastMouse_.y = y;

            cameraYaw_ += static_cast<double>(dx) * 0.006;
            cameraPitch_ += static_cast<double>(dy) * 0.005;
            cameraPitch_ = clamp(cameraPitch_, -1.28, 1.28);
        }
        break;
    case WM_MOUSEWHEEL: {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        cameraDistance_ *= std::pow(0.90, static_cast<double>(delta) / WHEEL_DELTA);
        cameraDistance_ = clamp(cameraDistance_, 1.4, 80.0);
        break;
    }
    case WM_KEYDOWN:
        handleKey(wParam);
        break;
    default:
        break;
    }
}

void Application::handleKey(WPARAM key) {
    if (key == VK_ESCAPE) {
        running_ = false;
        DestroyWindow(hwnd_);
        return;
    }

    if (key == VK_SPACE) {
        paused_ = !paused_;
    } else if (key == 'R') {
        if (system_.scenario() == Scenario::Custom && customConfig_.enabled) {
            resetCustomScenario();
        } else {
            resetScenario(system_.scenario());
        }
    } else if (key == '0') {
        customConfig_ = loadInitialConditionConfigFromDefaultLocations();
        if (customConfig_.enabled) {
            resetCustomScenario();
        }
    } else if (key == '1') {
        resetScenario(Scenario::TrisolarisChaos);
    } else if (key == '2') {
        resetScenario(Scenario::FigureEight);
    } else if (key == '3') {
        resetScenario(Scenario::InclinedDance);
    } else if (key == '4') {
        resetScenario(Scenario::HierarchicalTriple);
    } else if (key == '5') {
        resetScenario(Scenario::GravityAssist);
    } else if (key == 'T') {
        showTrails_ = !showTrails_;
    } else if (key == 'G') {
        showGrid_ = !showGrid_;
    } else if (key == 'H') {
        showField_ = !showField_;
    } else if (key == 'P') {
        showParticles_ = !showParticles_;
    } else if (key == 'C') {
        showShadow_ = !showShadow_;
    } else if (key == 'M') {
        system_.setCollisionMergingEnabled(!system_.collisionMergingEnabled());
    } else if (key == 'E') {
        editMode_ = !editMode_;
        if (editMode_) {
            paused_ = true;
            autoOrbit_ = false;
        } else {
            selectedBody_ = -1;
            bodyDragging_ = false;
        }
    } else if (key == 'X') {
        exportSnapshot();
    } else if (key == 'O') {
        autoOrbit_ = !autoOrbit_;
    } else if (key == 'A') {
        autoTimeScale_ = !autoTimeScale_;
        if (!autoTimeScale_) {
            effectiveSimulationSpeed_ = simulationSpeed_;
        }
    } else if (key == 'F') {
        const int count = static_cast<int>(system_.bodies().size());
        focusIndex_++;
        if (focusIndex_ >= count) {
            focusIndex_ = -1;
        }
    } else if (isPlusKey(key)) {
        simulationSpeed_ = std::min(MaxSimulationSpeed, simulationSpeed_ * 1.25);
    } else if (isMinusKey(key)) {
        simulationSpeed_ = std::max(MinSimulationSpeed, simulationSpeed_ / 1.25);
    }
}

void Application::resetScenario(Scenario scenario) {
    if (scenario == Scenario::Custom) {
        resetCustomScenario();
        return;
    }
    system_.reset(scenario);
    physicsDt_ = system_.recommendedTimeStep();
    cameraDistance_ = system_.recommendedCameraDistance();
    simulationAccumulator_ = 0.0;
    focusIndex_ = -1;
}

void Application::resetCustomScenario() {
    if (!customConfig_.enabled) {
        resetScenario(Scenario::TrisolarisChaos);
        return;
    }

    system_.reset(customConfig_);
    physicsDt_ = system_.recommendedTimeStep();
    cameraDistance_ = system_.recommendedCameraDistance();
    simulationAccumulator_ = 0.0;
    focusIndex_ = -1;
}

void Application::update(double realDt) {
    if (autoOrbit_ && !mouseDragging_ && !speedSliderDragging_ && !bodyDragging_) {
        cameraYaw_ += realDt * 0.075;
    }

    if (realDt > 1.0e-6) {
        const double instantFps = 1.0 / realDt;
        fps_ = fps_ <= 0.0 ? instantFps : fps_ * 0.92 + instantFps * 0.08;
    }

    if (autoTimeScale_) {
        const double targetSpeed = simulationSpeed_ * automaticTimeScale();
        const double response = targetSpeed < effectiveSimulationSpeed_
            ? 1.0 - std::exp(-realDt * 10.0)
            : 1.0 - std::exp(-realDt * 1.8);
        effectiveSimulationSpeed_ += (targetSpeed - effectiveSimulationSpeed_) * response;
    } else {
        effectiveSimulationSpeed_ = simulationSpeed_;
    }

    if (paused_) {
        return;
    }

    simulationAccumulator_ += realDt * effectiveSimulationSpeed_;
    int steps = 0;
    while (simulationAccumulator_ >= physicsDt_ && steps < 700) {
        system_.step(physicsDt_);
        simulationAccumulator_ -= physicsDt_;
        ++steps;
    }

    if (steps == 700) {
        simulationAccumulator_ = 0.0;
    }
}

void Application::render() {
    RenderState state;
    state.width = width_;
    state.height = height_;
    state.cameraYaw = cameraYaw_;
    state.cameraPitch = cameraPitch_;
    state.cameraDistance = cameraDistance_;
    state.simulationSpeed = effectiveSimulationSpeed_;
    state.baseSimulationSpeed = simulationSpeed_;
    state.physicsDt = physicsDt_;
    state.fps = fps_;
    state.paused = paused_;
    state.showTrails = showTrails_;
    state.showGrid = showGrid_;
    state.autoOrbit = autoOrbit_;
    state.autoTimeScale = autoTimeScale_;
    state.showParticles = showParticles_;
    state.showShadow = showShadow_;
    state.showField = showField_;
    state.editMode = editMode_;
    state.collisionsEnabled = system_.collisionMergingEnabled();
    state.focusIndex = focusIndex_;
    state.selectedBody = selectedBody_;
    renderer_.render(system_, state);
}

void Application::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    if (glContext_ != nullptr) {
        renderer_.resize(width_, height_);
    }
}

double Application::automaticTimeScale() const {
    const double closest = system_.minSeparation();
    const double closeRange = 0.16;
    const double openRange = 1.75;
    double t = clamp((closest - closeRange) / (openRange - closeRange), 0.0, 1.0);
    t = t * t * (3.0 - 2.0 * t);

    const double closeFactor = 0.06;
    const double farFactor = 1.15;
    return closeFactor + (farFactor - closeFactor) * t;
}

bool Application::speedSliderHitTest(int x, int y) const {
    return x >= SpeedSliderHitLeft && x <= SpeedSliderHitRight &&
        y >= SpeedSliderHitTop && y <= SpeedSliderHitBottom;
}

void Application::setSimulationSpeedFromSlider(int x) {
    const double t = static_cast<double>(x - SpeedSliderLeft) /
        static_cast<double>(SpeedSliderRight - SpeedSliderLeft);
    simulationSpeed_ = speedFromSliderPosition(t);
    if (!autoTimeScale_) {
        effectiveSimulationSpeed_ = simulationSpeed_;
    }
}

void Application::cameraFrame(Vec3& eye, Vec3& right, Vec3& up, Vec3& forward) const {
    Vec3 target{};
    const auto& bodies = system_.bodies();
    if (focusIndex_ >= 0 && focusIndex_ < static_cast<int>(bodies.size())) {
        target = bodies[static_cast<std::size_t>(focusIndex_)].position;
    }

    const double cp = std::cos(cameraPitch_);
    eye = {
        target.x + cameraDistance_ * cp * std::cos(cameraYaw_),
        target.y + cameraDistance_ * cp * std::sin(cameraYaw_),
        target.z + cameraDistance_ * std::sin(cameraPitch_),
    };
    forward = normalized(target - eye);
    right = normalized(cross(forward, {0.0, 0.0, 1.0}));
    if (lengthSquared(right) <= 1.0e-10) {
        right = {1.0, 0.0, 0.0};
    }
    up = cross(right, forward);
}

int Application::bodyHitTest(int x, int y) const {
    Vec3 eye{};
    Vec3 right{};
    Vec3 up{};
    Vec3 forward{};
    cameraFrame(eye, right, up, forward);

    const double aspect = static_cast<double>(std::max(1, width_)) / static_cast<double>(std::max(1, height_));
    const double tanHalfFov = std::tan(CameraFovY * 0.5);

    int best = -1;
    double bestDistance = std::numeric_limits<double>::infinity();
    const auto& bodies = system_.bodies();
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const Vec3 relative = bodies[i].position - eye;
        const double depth = dot(relative, forward);
        if (depth <= 0.01) {
            continue;
        }

        const double normalizedX = dot(relative, right) / (depth * tanHalfFov * aspect);
        const double normalizedY = dot(relative, up) / (depth * tanHalfFov);
        const double screenX = (normalizedX + 1.0) * 0.5 * static_cast<double>(width_);
        const double screenY = (1.0 - normalizedY) * 0.5 * static_cast<double>(height_);
        const double pixelDistance = std::hypot(screenX - static_cast<double>(x), screenY - static_cast<double>(y));
        const double pixelRadius = std::max(16.0, bodies[i].radius * static_cast<double>(height_) / (depth * tanHalfFov * 2.0));

        if (pixelDistance <= pixelRadius && pixelDistance < bestDistance) {
            bestDistance = pixelDistance;
            best = static_cast<int>(i);
        }
    }

    return best;
}

void Application::beginBodyDrag(int x, int y) {
    selectedBody_ = bodyHitTest(x, y);
    if (selectedBody_ < 0) {
        return;
    }
    paused_ = true;
    bodyDragging_ = true;
    mouseDragging_ = false;
    speedSliderDragging_ = false;
    lastMouse_.x = x;
    lastMouse_.y = y;
}

void Application::updateBodyDrag(int x, int y, bool verticalOnly) {
    if (selectedBody_ < 0 || selectedBody_ >= static_cast<int>(system_.bodies().size())) {
        return;
    }

    Vec3 eye{};
    Vec3 right{};
    Vec3 up{};
    Vec3 forward{};
    cameraFrame(eye, right, up, forward);

    const Body& body = system_.bodies()[static_cast<std::size_t>(selectedBody_)];
    const double depth = std::max(0.1, dot(body.position - eye, forward));
    const double worldPerPixel = 2.0 * depth * std::tan(CameraFovY * 0.5) / static_cast<double>(std::max(1, height_));
    const int dx = x - lastMouse_.x;
    const int dy = y - lastMouse_.y;

    Vec3 delta{};
    if (verticalOnly) {
        delta = Vec3{0.0, 0.0, -static_cast<double>(dy) * worldPerPixel};
    } else {
        delta = right * (static_cast<double>(dx) * worldPerPixel) -
            up * (static_cast<double>(dy) * worldPerPixel);
    }

    system_.setBodyPosition(static_cast<std::size_t>(selectedBody_), body.position + delta);
    lastMouse_.x = x;
    lastMouse_.y = y;
}

void Application::exportSnapshot() const {
    std::filesystem::create_directories("exports");
    std::ofstream file("exports/phyzbox_snapshot.csv");
    if (!file) {
        return;
    }

    file << "kind,index,name,time_yr,mass_solar,x_au,y_au,z_au,vx_au_per_yr,vy_au_per_yr,vz_au_per_yr\n";
    const auto& bodies = system_.bodies();
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const Body& body = bodies[i];
        file << "body," << i << ",\"" << body.name << "\","
             << std::setprecision(12) << system_.time() << ','
             << body.mass << ','
             << body.position.x << ',' << body.position.y << ',' << body.position.z << ','
             << body.velocity.x << ',' << body.velocity.y << ',' << body.velocity.z << '\n';
    }

    const auto& particles = system_.testParticles();
    for (std::size_t i = 0; i < particles.size(); ++i) {
        const TestParticle& particle = particles[i];
        file << "particle," << i << ",\"\","
             << std::setprecision(12) << system_.time() << ','
             << 0.0 << ','
             << particle.position.x << ',' << particle.position.y << ',' << particle.position.z << ','
             << particle.velocity.x << ',' << particle.velocity.y << ',' << particle.velocity.z << '\n';
    }
}

} // namespace phyz
