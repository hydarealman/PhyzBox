#include "Renderer.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>

namespace phyz {
namespace {

float mix(float a, float b, float t) {
    return a + (b - a) * t;
}

Color scaleColor(Color color, float scale, float alpha) {
    return {
        std::min(1.0f, color.r * scale),
        std::min(1.0f, color.g * scale),
        std::min(1.0f, color.b * scale),
        alpha,
    };
}

constexpr double MinSimulationSpeed = 0.01;
constexpr double MaxSimulationSpeed = 8.0;

double speedSliderPosition(double speed) {
    const double safeSpeed = clamp(speed, MinSimulationSpeed, MaxSimulationSpeed);
    return std::log(safeSpeed / MinSimulationSpeed) /
        std::log(MaxSimulationSpeed / MinSimulationSpeed);
}

} // namespace

Renderer::~Renderer() {
    cleanup();
}

bool Renderer::initialize(HDC deviceContext) {
    deviceContext_ = deviceContext;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.004f, 0.006f, 0.014f, 1.0f);

    createFont();
    createStars();
    return fontBase_ != 0;
}

void Renderer::cleanup() {
    if (fontBase_ != 0) {
        glDeleteLists(fontBase_, 96);
        fontBase_ = 0;
    }
}

void Renderer::resize(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    glViewport(0, 0, width_, height_);
}

void Renderer::render(const NBodySystem& system, const RenderState& state) {
    glViewport(0, 0, state.width, state.height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    setupCamera(system, state);
    renderStars();

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    if (state.showGrid) {
        renderReferenceGrid();
        if (state.showField) {
            renderGravityField(system);
        }
        renderDepthCues(system);
    }
    if (state.showTrails) {
        renderTrails(system);
    }
    if (state.showParticles) {
        renderTestParticles(system);
    }
    if (state.showShadow) {
        renderShadowSystem(system);
    }
    renderAccretionDisks(system);
    glDepthMask(GL_TRUE);

    renderGlows(system);
    renderBodies(system, state.focusIndex, state.selectedBody);
    renderHud(system, state);
}

void Renderer::createFont() {
    if (deviceContext_ == nullptr) {
        return;
    }

    fontBase_ = glGenLists(96);
    HFONT font = CreateFontA(
        -16,
        0,
        0,
        0,
        FW_MEDIUM,
        FALSE,
        FALSE,
        FALSE,
        ANSI_CHARSET,
        OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        FF_DONTCARE | FIXED_PITCH,
        "Consolas");
    HFONT oldFont = static_cast<HFONT>(SelectObject(deviceContext_, font));
    wglUseFontBitmapsA(deviceContext_, 32, 96, fontBase_);
    SelectObject(deviceContext_, oldFont);
    DeleteObject(font);
}

void Renderer::createStars() {
    stars_.clear();
    stars_.reserve(1800);

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    std::uniform_real_distribution<double> azimuth(0.0, 2.0 * Pi);
    std::uniform_real_distribution<double> cosine(-1.0, 1.0);

    for (int i = 0; i < 1800; ++i) {
        const double radius = mix(26.0f, 115.0f, static_cast<float>(std::pow(unit(rng), 0.35)));
        const double theta = azimuth(rng);
        const double zUnit = cosine(rng);
        const double radial = std::sqrt(std::max(0.0, 1.0 - zUnit * zUnit));
        const Vec3 position{
            radius * radial * std::cos(theta),
            radius * radial * std::sin(theta),
            radius * zUnit,
        };

        const float tint = static_cast<float>(unit(rng));
        const float brightness = mix(0.35f, 1.0f, static_cast<float>(unit(rng) * unit(rng)));
        Color color{
            mix(0.70f, 1.00f, tint) * brightness,
            mix(0.78f, 0.94f, tint) * brightness,
            mix(1.00f, 0.76f, tint) * brightness,
            0.92f,
        };

        stars_.push_back({position, color, mix(1.0f, 2.8f, static_cast<float>(unit(rng) * unit(rng)))});
    }
}

void Renderer::setupCamera(const NBodySystem& system, const RenderState& state) {
    const float aspect = static_cast<float>(std::max(1, state.width)) / static_cast<float>(std::max(1, state.height));
    const auto projection = perspective(static_cast<float>(Pi / 4.0), aspect, 0.03f, 220.0f);

    const Vec3 target = cameraTarget(system, state.focusIndex);
    const double cp = std::cos(state.cameraPitch);
    const Vec3 eye{
        target.x + state.cameraDistance * cp * std::cos(state.cameraYaw),
        target.y + state.cameraDistance * cp * std::sin(state.cameraYaw),
        target.z + state.cameraDistance * std::sin(state.cameraPitch),
    };
    const auto view = lookAt(eye, target, {0.0, 0.0, 1.0});

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projection.data());
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(view.data());
}

void Renderer::renderStars() {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (int pass = 0; pass < 3; ++pass) {
        const float low = pass == 0 ? 0.0f : (pass == 1 ? 1.55f : 2.25f);
        const float high = pass == 0 ? 1.55f : (pass == 1 ? 2.25f : 10.0f);
        glPointSize(pass == 0 ? 1.2f : (pass == 1 ? 1.9f : 2.8f));

        glBegin(GL_POINTS);
        for (const Star& star : stars_) {
            if (star.size < low || star.size >= high) {
                continue;
            }
            glColor4f(star.color.r, star.color.g, star.color.b, star.color.a);
            glVertex3d(star.position.x, star.position.y, star.position.z);
        }
        glEnd();
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
}

void Renderer::renderReferenceGrid() {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glLineWidth(1.0f);

    for (int ring = 1; ring <= 8; ++ring) {
        const double radius = ring * 0.55;
        const float alpha = 0.14f - static_cast<float>(ring) * 0.008f;
        glColor4f(0.22f, 0.34f, 0.50f, alpha);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 160; ++i) {
            const double t = (2.0 * Pi * i) / 160.0;
            glVertex3d(std::cos(t) * radius, std::sin(t) * radius, 0.0);
        }
        glEnd();

        glColor4f(0.20f, 0.42f, 0.56f, alpha * 0.58f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 160; ++i) {
            const double t = (2.0 * Pi * i) / 160.0;
            glVertex3d(std::cos(t) * radius, 0.0, std::sin(t) * radius);
        }
        glEnd();

        glColor4f(0.32f, 0.30f, 0.58f, alpha * 0.52f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 160; ++i) {
            const double t = (2.0 * Pi * i) / 160.0;
            glVertex3d(0.0, std::cos(t) * radius, std::sin(t) * radius);
        }
        glEnd();
    }

    glBegin(GL_LINES);
    glColor4f(0.90f, 0.34f, 0.30f, 0.30f);
    glVertex3d(-5.2, 0.0, 0.0);
    glVertex3d(5.2, 0.0, 0.0);
    glColor4f(0.32f, 0.82f, 0.52f, 0.30f);
    glVertex3d(0.0, -5.2, 0.0);
    glVertex3d(0.0, 5.2, 0.0);
    glColor4f(0.38f, 0.66f, 1.00f, 0.32f);
    glVertex3d(0.0, 0.0, -4.0);
    glVertex3d(0.0, 0.0, 4.0);

    glColor4f(0.25f, 0.36f, 0.58f, 0.08f);
    for (int i = -4; i <= 4; ++i) {
        glVertex3d(static_cast<double>(i), -4.0, -2.5);
        glVertex3d(static_cast<double>(i), 4.0, -2.5);
        glVertex3d(-4.0, static_cast<double>(i), -2.5);
        glVertex3d(4.0, static_cast<double>(i), -2.5);

        glVertex3d(static_cast<double>(i), -4.0, 2.5);
        glVertex3d(static_cast<double>(i), 4.0, 2.5);
        glVertex3d(-4.0, static_cast<double>(i), 2.5);
        glVertex3d(4.0, static_cast<double>(i), 2.5);
    }

    for (int x = -4; x <= 4; x += 2) {
        for (int y = -4; y <= 4; y += 2) {
            glVertex3d(static_cast<double>(x), static_cast<double>(y), -2.5);
            glVertex3d(static_cast<double>(x), static_cast<double>(y), 2.5);
        }
    }
    glEnd();
}

void Renderer::renderGravityField(const NBodySystem& system) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(1.0f);

    glBegin(GL_LINES);
    for (int x = -5; x <= 5; ++x) {
        for (int y = -5; y <= 5; ++y) {
            const Vec3 origin{static_cast<double>(x) * 0.75, static_cast<double>(y) * 0.75, 0.0};
            const Vec3 acceleration = system.accelerationAt(origin);
            const double magnitude = length(acceleration);
            if (magnitude <= 1.0e-8) {
                continue;
            }
            const double arrowLength = clamp(std::log1p(magnitude) * 0.06, 0.035, 0.22);
            const Vec3 end = origin + normalized(acceleration) * arrowLength;
            const float alpha = static_cast<float>(clamp(std::log1p(magnitude) * 0.045, 0.05, 0.26));
            glColor4f(0.42f, 0.78f, 1.00f, alpha);
            glVertex3d(origin.x, origin.y, origin.z);
            glColor4f(0.92f, 0.98f, 1.00f, alpha + 0.08f);
            glVertex3d(end.x, end.y, end.z);
        }
    }
    glEnd();

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::renderDepthCues(const NBodySystem& system) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glLineWidth(1.25f);

    for (const Body& body : system.bodies()) {
        const float alpha = body.position.z >= 0.0 ? 0.34f : 0.20f;
        glColor4f(body.color.r, body.color.g, body.color.b, alpha);
        glBegin(GL_LINES);
        glVertex3d(body.position.x, body.position.y, 0.0);
        glVertex3d(body.position.x, body.position.y, body.position.z);
        glEnd();

        glColor4f(body.color.r, body.color.g, body.color.b, 0.18f);
        glBegin(GL_LINE_LOOP);
        const double radius = body.radius * 1.35;
        for (int i = 0; i < 64; ++i) {
            const double t = 2.0 * Pi * static_cast<double>(i) / 64.0;
            glVertex3d(
                body.position.x + std::cos(t) * radius,
                body.position.y + std::sin(t) * radius,
                0.0);
        }
        glEnd();
    }
}

void Renderer::renderTrails(const NBodySystem& system) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(2.0f);

    for (const Body& body : system.bodies()) {
        if (body.trail.size() < 2) {
            continue;
        }

        glBegin(GL_LINE_STRIP);
        const double count = static_cast<double>(body.trail.size() - 1);
        int index = 0;
        for (const Vec3& point : body.trail) {
            const float age = static_cast<float>(index / count);
            const float alpha = 0.03f + age * age * 0.62f;
            const float pulse = 0.62f + age * 0.55f;
            glColor4f(
                std::min(1.0f, body.color.r * pulse),
                std::min(1.0f, body.color.g * pulse),
                std::min(1.0f, body.color.b * pulse),
                alpha);
            glVertex3d(point.x, point.y, point.z);
            ++index;
        }
        glEnd();
    }

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::renderTestParticles(const NBodySystem& system) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(1.0f);

    for (const TestParticle& particle : system.testParticles()) {
        if (particle.trail.size() >= 2) {
            glBegin(GL_LINE_STRIP);
            int index = 0;
            const double count = static_cast<double>(particle.trail.size() - 1);
            for (const Vec3& point : particle.trail) {
                const float age = static_cast<float>(index / count);
                glColor4f(0.42f, 0.72f, 1.00f, 0.015f + age * 0.075f);
                glVertex3d(point.x, point.y, point.z);
                ++index;
            }
            glEnd();
        }
    }

    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for (const TestParticle& particle : system.testParticles()) {
        glColor4f(particle.color.r, particle.color.g, particle.color.b, particle.color.a);
        glVertex3d(particle.position.x, particle.position.y, particle.position.z);
    }
    glEnd();
    glPointSize(1.0f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::renderShadowSystem(const NBodySystem& system) {
    const auto& shadow = system.shadowBodies();
    const auto& bodies = system.bodies();
    if (shadow.empty() || shadow.size() != bodies.size()) {
        return;
    }

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(1.0f);

    glBegin(GL_LINES);
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        glColor4f(0.95f, 0.58f, 1.00f, 0.10f);
        glVertex3d(bodies[i].position.x, bodies[i].position.y, bodies[i].position.z);
        glColor4f(0.95f, 0.58f, 1.00f, 0.36f);
        glVertex3d(shadow[i].position.x, shadow[i].position.y, shadow[i].position.z);
    }
    glEnd();

    glPointSize(6.0f);
    glBegin(GL_POINTS);
    for (const Body& body : shadow) {
        glColor4f(0.95f, 0.58f, 1.00f, 0.52f);
        glVertex3d(body.position.x, body.position.y, body.position.z);
    }
    glEnd();
    glPointSize(1.0f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::renderAccretionDisks(const NBodySystem& system) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);

    for (const Body& body : system.bodies()) {
        if (body.type != BodyType::BlackHole) {
            continue;
        }

        const double inner = std::max(body.radius * 1.20, body.innermostStableCircularOrbit * 18.0);
        const double diskBoost = clamp(std::log1p(body.accretionDiskMass * 8.0), 0.0, 2.5);
        const double outer = inner + body.radius * (2.8 + diskBoost);
        const float alpha = static_cast<float>(0.16 + diskBoost * 0.10);

        glPushMatrix();
        glTranslated(body.position.x, body.position.y, body.position.z);
        glRotated(12.0, 1.0, 0.0, 0.0);
        glRotated(32.0, 0.0, 1.0, 0.0);

        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= 160; ++i) {
            const double t = 2.0 * Pi * static_cast<double>(i) / 160.0;
            const double c = std::cos(t);
            const double s = std::sin(t);
            const float heat = static_cast<float>(0.55 + 0.45 * std::sin(t * 3.0 + body.accretionDiskMass));
            glColor4f(1.00f, 0.78f * heat, 0.30f * heat, alpha);
            glVertex3d(c * inner, s * inner, 0.0);
            glColor4f(0.95f, 0.30f, 0.08f, alpha * 0.15f);
            glVertex3d(c * outer, s * outer, 0.0);
        }
        glEnd();

        glPopMatrix();
    }

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::renderGlows(const NBodySystem& system) {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);

    for (const Body& body : system.bodies()) {
        if (body.type == BodyType::BlackHole) {
            continue;
        }
        const float luminosityScale = static_cast<float>(clamp(std::sqrt(std::max(0.02, body.luminosity)), 0.35, 2.8));
        for (int layer = 0; layer < 3; ++layer) {
            const float alpha = (0.12f * luminosityScale) / static_cast<float>(layer + 1);
            glPointSize(static_cast<float>((30 + layer * 22) * luminosityScale));
            glBegin(GL_POINTS);
            const Color glow = scaleColor(body.color, 1.18f, alpha);
            glColor4f(glow.r, glow.g, glow.b, glow.a);
            glVertex3d(body.position.x, body.position.y, body.position.z);
            glEnd();
        }
    }

    glPointSize(1.0f);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::renderBodies(const NBodySystem& system, int focusIndex, int selectedIndex) {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    const GLfloat lightPosition[] = {3.0f, -4.0f, 6.0f, 0.0f};
    const GLfloat lightDiffuse[] = {0.95f, 0.96f, 1.00f, 1.0f};
    const GLfloat lightAmbient[] = {0.10f, 0.12f, 0.18f, 1.0f};
    const GLfloat specular[] = {0.85f, 0.88f, 1.00f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 54.0f);

    const auto& bodies = system.bodies();
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const Body& body = bodies[i];
        glPushMatrix();
        glTranslated(body.position.x, body.position.y, body.position.z);

        if (body.type == BodyType::Spacecraft) {
            glDisable(GL_LIGHTING);
            renderSpacecraft(body);
            glEnable(GL_LIGHTING);
            glPopMatrix();
            continue;
        }

        const float emissionScale = body.type == BodyType::BlackHole
            ? 0.02f
            : static_cast<float>(clamp(0.14 * std::sqrt(std::max(0.01, body.luminosity)), 0.06, 0.55));
        const GLfloat emission[] = {
            body.color.r * emissionScale,
            body.color.g * emissionScale,
            body.color.b * emissionScale,
            1.0f,
        };
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission);
        if (body.type == BodyType::BlackHole) {
            glColor4f(0.003f, 0.002f, 0.006f, 1.0f);
        } else {
            glColor4f(body.color.r, body.color.g, body.color.b, 1.0f);
        }
        applySpinTransform(body);
        renderSphere(body.radius, 28, 42);
        if (body.type != BodyType::BlackHole) {
            glDisable(GL_LIGHTING);
            renderSpinGuides(body);
            glEnable(GL_LIGHTING);
        }
        glPopMatrix();

        if (body.type == BodyType::BlackHole) {
            glDisable(GL_LIGHTING);
            glEnable(GL_BLEND);
            glLineWidth(1.5f);
            glColor4f(0.60f, 0.50f, 0.95f, 0.50f);
            glBegin(GL_LINE_LOOP);
            const double ring = body.radius * 1.15;
            for (int k = 0; k < 96; ++k) {
                const double t = 2.0 * Pi * static_cast<double>(k) / 96.0;
                glVertex3d(body.position.x + std::cos(t) * ring, body.position.y + std::sin(t) * ring, body.position.z);
            }
            glEnd();
            glEnable(GL_LIGHTING);
        }
    }

    const GLfloat noEmission[] = {0.0f, 0.0f, 0.0f, 1.0f};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, noEmission);
    glDisable(GL_LIGHTING);

    if (focusIndex >= 0 && focusIndex < static_cast<int>(bodies.size())) {
        renderFocusMarker(bodies[static_cast<std::size_t>(focusIndex)]);
    }
    if (selectedIndex >= 0 && selectedIndex < static_cast<int>(bodies.size()) && selectedIndex != focusIndex) {
        renderFocusMarker(bodies[static_cast<std::size_t>(selectedIndex)]);
    }
}

void Renderer::renderSpacecraft(const Body& body) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glLineWidth(1.4f);

    Vec3 forward = normalized(body.velocity);
    if (lengthSquared(forward) <= 1.0e-12) {
        forward = {1.0, 0.0, 0.0};
    }
    Vec3 right = normalized(cross(forward, {0.0, 0.0, 1.0}));
    if (lengthSquared(right) <= 1.0e-12) {
        right = {0.0, 1.0, 0.0};
    }
    const Vec3 up = normalized(cross(right, forward));

    const double scale = body.radius;
    const Vec3 nose = forward * (scale * 2.4);
    const Vec3 tail = -forward * (scale * 1.35);
    const Vec3 leftWing = tail - right * (scale * 0.90);
    const Vec3 rightWing = tail + right * (scale * 0.90);
    const Vec3 topFin = tail + up * (scale * 0.72);
    const Vec3 bottomFin = tail - up * (scale * 0.52);

    glBegin(GL_TRIANGLES);
    glColor4f(body.color.r, body.color.g, body.color.b, 0.96f);
    glVertex3d(nose.x, nose.y, nose.z);
    glColor4f(0.62f, 0.78f, 1.00f, 0.86f);
    glVertex3d(leftWing.x, leftWing.y, leftWing.z);
    glVertex3d(topFin.x, topFin.y, topFin.z);

    glColor4f(body.color.r, body.color.g, body.color.b, 0.92f);
    glVertex3d(nose.x, nose.y, nose.z);
    glColor4f(0.48f, 0.64f, 0.92f, 0.78f);
    glVertex3d(topFin.x, topFin.y, topFin.z);
    glVertex3d(rightWing.x, rightWing.y, rightWing.z);

    glColor4f(0.85f, 0.92f, 1.00f, 0.88f);
    glVertex3d(nose.x, nose.y, nose.z);
    glColor4f(0.40f, 0.54f, 0.82f, 0.74f);
    glVertex3d(rightWing.x, rightWing.y, rightWing.z);
    glVertex3d(bottomFin.x, bottomFin.y, bottomFin.z);

    glColor4f(0.92f, 0.96f, 1.00f, 0.88f);
    glVertex3d(nose.x, nose.y, nose.z);
    glColor4f(0.50f, 0.66f, 0.92f, 0.72f);
    glVertex3d(bottomFin.x, bottomFin.y, bottomFin.z);
    glVertex3d(leftWing.x, leftWing.y, leftWing.z);
    glEnd();

    const Vec3 plumeStart = tail - forward * (scale * 0.10);
    const Vec3 plumeEnd = tail - forward * (scale * 4.1);
    glBegin(GL_LINES);
    glColor4f(0.95f, 0.98f, 1.00f, 0.70f);
    glVertex3d(plumeStart.x, plumeStart.y, plumeStart.z);
    glColor4f(1.00f, 0.42f, 0.16f, 0.05f);
    glVertex3d(plumeEnd.x, plumeEnd.y, plumeEnd.z);
    glEnd();

    glPointSize(5.0f);
    glBegin(GL_POINTS);
    glColor4f(0.88f, 0.96f, 1.00f, 0.95f);
    glVertex3d(0.0, 0.0, 0.0);
    glEnd();
    glPointSize(1.0f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::applySpinTransform(const Body& body) {
    Vec3 axis = normalized(body.spinAxis);
    if (lengthSquared(axis) <= 1.0e-12) {
        axis = {0.0, 0.0, 1.0};
    }

    const Vec3 localZ{0.0, 0.0, 1.0};
    const double alignment = clamp(dot(localZ, axis), -1.0, 1.0);
    Vec3 rotateAxis = cross(localZ, axis);
    if (lengthSquared(rotateAxis) > 1.0e-12) {
        rotateAxis = normalized(rotateAxis);
        glRotated(std::acos(alignment) * 180.0 / Pi, rotateAxis.x, rotateAxis.y, rotateAxis.z);
    } else if (alignment < 0.0) {
        glRotated(180.0, 1.0, 0.0, 0.0);
    }
    glRotated(body.rotationAngle * 180.0 / Pi, 0.0, 0.0, 1.0);
}

void Renderer::renderSpinGuides(const Body& body) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    glLineWidth(1.0f);

    const double radius = body.radius * 1.012;
    const Color guide = scaleColor(body.color, 1.35f, 0.34f);

    glColor4f(guide.r, guide.g, guide.b, guide.a);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 96; ++i) {
        const double t = 2.0 * Pi * static_cast<double>(i) / 96.0;
        glVertex3d(std::cos(t) * radius, std::sin(t) * radius, 0.0);
    }
    glEnd();

    for (int meridian = 0; meridian < 3; ++meridian) {
        const double turn = 2.0 * Pi * static_cast<double>(meridian) / 3.0;
        const double c = std::cos(turn);
        const double s = std::sin(turn);
        glColor4f(guide.r, guide.g, guide.b, guide.a * 0.62f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 96; ++i) {
            const double t = 2.0 * Pi * static_cast<double>(i) / 96.0;
            const double x = std::cos(t) * radius;
            const double z = std::sin(t) * radius;
            glVertex3d(x * c, x * s, z);
        }
        glEnd();
    }

    glLineWidth(1.5f);
    glBegin(GL_LINES);
    glColor4f(0.90f, 0.96f, 1.00f, 0.45f);
    glVertex3d(0.0, 0.0, radius * 1.10);
    glColor4f(0.90f, 0.96f, 1.00f, 0.05f);
    glVertex3d(0.0, 0.0, radius * 1.82);
    glEnd();

    glDepthMask(GL_TRUE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::renderFocusMarker(const Body& body) {
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glLineWidth(1.4f);
    glColor4f(0.88f, 0.94f, 1.00f, 0.46f);

    const double radius = body.radius * 1.85;
    for (int plane = 0; plane < 3; ++plane) {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 96; ++i) {
            const double t = 2.0 * Pi * static_cast<double>(i) / 96.0;
            const double a = std::cos(t) * radius;
            const double b = std::sin(t) * radius;
            if (plane == 0) {
                glVertex3d(body.position.x + a, body.position.y + b, body.position.z);
            } else if (plane == 1) {
                glVertex3d(body.position.x + a, body.position.y, body.position.z + b);
            } else {
                glVertex3d(body.position.x, body.position.y + a, body.position.z + b);
            }
        }
        glEnd();
    }
}

void Renderer::renderSphere(double radius, int stacks, int slices) {
    for (int i = 0; i < stacks; ++i) {
        const double phi0 = Pi * (-0.5 + static_cast<double>(i) / stacks);
        const double phi1 = Pi * (-0.5 + static_cast<double>(i + 1) / stacks);
        const double z0 = std::sin(phi0);
        const double z1 = std::sin(phi1);
        const double zr0 = std::cos(phi0);
        const double zr1 = std::cos(phi1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; ++j) {
            const double theta = 2.0 * Pi * static_cast<double>(j) / slices;
            const double x = std::cos(theta);
            const double y = std::sin(theta);

            glNormal3d(x * zr0, y * zr0, z0);
            glVertex3d(radius * x * zr0, radius * y * zr0, radius * z0);

            glNormal3d(x * zr1, y * zr1, z1);
            glVertex3d(radius * x * zr1, radius * y * zr1, radius * z1);
        }
        glEnd();
    }
}

void Renderer::renderHud(const NBodySystem& system, const RenderState& state) {
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, state.width, state.height, 0.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glBegin(GL_QUADS);
    glColor4f(0.02f, 0.03f, 0.06f, 0.56f);
    glVertex2f(18.0f, 18.0f);
    glVertex2f(552.0f, 18.0f);
    glVertex2f(552.0f, 370.0f);
    glVertex2f(18.0f, 370.0f);
    glEnd();

    std::ostringstream line;
    line << "PhyzBox 3D Gravity Engine";
    drawText(34.0f, 42.0f, line.str(), {0.88f, 0.94f, 1.0f, 0.98f});

    line.str("");
    line.clear();
    line << "Scenario: " << system.scenarioName() << (state.paused ? "  [paused]" : "  [running]");
    drawText(34.0f, 66.0f, line.str(), {0.72f, 0.82f, 0.96f, 0.95f});

    line.str("");
    line.clear();
    line << std::fixed << std::setprecision(2)
         << "time " << system.time() << " yr"
         << " | speed " << state.simulationSpeed << "x"
         << (state.autoTimeScale ? " auto" : " manual");
    if (state.autoTimeScale) {
        line << " | base " << state.baseSimulationSpeed << "x";
    }
    drawText(34.0f, 90.0f, line.str(), {0.70f, 0.95f, 0.82f, 0.92f});

    line.str("");
    line.clear();
    line << std::fixed << std::setprecision(6)
         << "dt " << state.physicsDt << " yr"
         << " | closest " << std::setprecision(3) << system.minSeparation() << " AU"
         << " | softening " << std::setprecision(4) << system.softeningLength() << " AU";
    drawText(34.0f, 114.0f, line.str(), {0.72f, 0.86f, 1.00f, 0.90f});

    line.str("");
    line.clear();
    line << std::fixed << std::setprecision(5)
         << "energy drift " << (system.energyDrift() * 100.0) << "%"
         << " | angular drift " << (system.angularMomentumDrift() * 100.0) << "%"
         << " | fps " << std::setprecision(0) << state.fps;
    drawText(34.0f, 138.0f, line.str(), {0.93f, 0.80f, 0.60f, 0.92f});

    line.str("");
    line.clear();
    line << "status " << system.systemStatus()
         << " | chaos " << std::scientific << std::setprecision(2) << system.chaosDivergence() << " AU"
         << " | tide " << std::fixed << std::setprecision(2) << system.maxTidalStress()
         << " | mergers " << std::defaultfloat << system.mergerCount()
         << (state.collisionsEnabled ? " | merge on" : " | merge off");
    drawText(34.0f, 162.0f, line.str(), {0.82f, 0.92f, 1.00f, 0.92f});

    line.str("");
    line.clear();
    line << std::fixed << std::setprecision(3)
         << "Base speed " << state.baseSimulationSpeed << "x";
    drawText(34.0f, 186.0f, line.str(), {0.86f, 0.91f, 1.00f, 0.92f});

    const float sliderX = 34.0f;
    const float sliderY = 202.0f;
    const float sliderWidth = 484.0f;
    const float sliderHeight = 8.0f;
    const float knobWidth = 12.0f;
    const float t = static_cast<float>(speedSliderPosition(state.baseSimulationSpeed));
    const float knobX = sliderX + sliderWidth * t;

    glBegin(GL_QUADS);
    glColor4f(0.11f, 0.14f, 0.21f, 0.92f);
    glVertex2f(sliderX, sliderY);
    glVertex2f(sliderX + sliderWidth, sliderY);
    glVertex2f(sliderX + sliderWidth, sliderY + sliderHeight);
    glVertex2f(sliderX, sliderY + sliderHeight);

    glColor4f(0.36f, 0.72f, 1.00f, 0.80f);
    glVertex2f(sliderX, sliderY);
    glVertex2f(knobX, sliderY);
    glVertex2f(knobX, sliderY + sliderHeight);
    glVertex2f(sliderX, sliderY + sliderHeight);

    glColor4f(0.88f, 0.95f, 1.00f, 0.96f);
    glVertex2f(knobX - knobWidth * 0.5f, sliderY - 5.0f);
    glVertex2f(knobX + knobWidth * 0.5f, sliderY - 5.0f);
    glVertex2f(knobX + knobWidth * 0.5f, sliderY + sliderHeight + 5.0f);
    glVertex2f(knobX - knobWidth * 0.5f, sliderY + sliderHeight + 5.0f);
    glEnd();

    const bool hasSpacecraft = system.spacecraftSpeed() > 0.0;
    const float controlsY = hasSpacecraft ? 262.0f : 238.0f;
    if (hasSpacecraft) {
        line.str("");
        line.clear();
        line << std::fixed << std::setprecision(2)
             << "spacecraft " << system.spacecraftSpeed() << " AU/yr"
             << " | gain " << (system.spacecraftSpeedGain() * 100.0) << "%"
             << " | best flyby " << system.spacecraftNearestEncounterDistance() << " AU";
        drawText(34.0f, 238.0f, line.str(), {0.55f, 0.96f, 1.00f, 0.92f});
    }

    drawText(34.0f, controlsY, "Mouse drag/wheel | Space R 0 1 2 3 4 5 +/- A C E H M P T X", {0.78f, 0.80f, 0.86f, 0.86f});

    if (state.editMode) {
        drawText(34.0f, controlsY + 24.0f, "edit mode: drag a body, Shift-drag changes z height", {0.98f, 0.88f, 0.54f, 0.92f});
    }

    float eventY = controlsY + (state.editMode ? 48.0f : 24.0f);
    int shown = 0;
    for (const EventLogEntry& event : system.events()) {
        std::ostringstream eventLine;
        eventLine << std::fixed << std::setprecision(2) << event.time << " yr  " << event.message;
        drawText(eventY >= 358.0f ? 620.0f : 34.0f, eventY >= 358.0f ? 42.0f + shown * 24.0f : eventY, eventLine.str(), event.color);
        eventY += 24.0f;
        ++shown;
        if (shown >= 4) {
            break;
        }
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void Renderer::drawText(float x, float y, const std::string& text, Color color) {
    if (fontBase_ == 0 || text.empty()) {
        return;
    }

    glColor4f(color.r, color.g, color.b, color.a);
    glRasterPos2f(x, y);
    glListBase(fontBase_ - 32);
    glCallLists(static_cast<GLsizei>(text.size()), GL_UNSIGNED_BYTE, text.c_str());
}

Vec3 Renderer::cameraTarget(const NBodySystem& system, int focusIndex) const {
    const auto& bodies = system.bodies();
    if (focusIndex >= 0 && focusIndex < static_cast<int>(bodies.size())) {
        return bodies[static_cast<std::size_t>(focusIndex)].position;
    }
    return {0.0, 0.0, 0.0};
}

} // namespace phyz
