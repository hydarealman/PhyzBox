#include "Application.hpp"

#include <algorithm>
#include <cmath>
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
constexpr int SpeedSliderHitTop = 170;
constexpr int SpeedSliderHitBottom = 200;
constexpr int SpeedSliderLeft = 34;
constexpr int SpeedSliderRight = 518;

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
            setSimulationSpeedFromSlider(lastMouse_.x);
        } else {
            mouseDragging_ = true;
            speedSliderDragging_ = false;
        }
        SetCapture(hwnd_);
        break;
    case WM_LBUTTONUP:
        mouseDragging_ = false;
        speedSliderDragging_ = false;
        ReleaseCapture();
        break;
    case WM_MOUSEMOVE:
        if (speedSliderDragging_) {
            setSimulationSpeedFromSlider(GET_X_LPARAM(lParam));
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
    } else if (key == 'T') {
        showTrails_ = !showTrails_;
    } else if (key == 'G') {
        showGrid_ = !showGrid_;
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
    if (autoOrbit_ && !mouseDragging_ && !speedSliderDragging_) {
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
    state.focusIndex = focusIndex_;
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

} // namespace phyz
