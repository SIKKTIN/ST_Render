#pragma once

#include "core/math/Vector3.hpp"
#include "core/math/Matrix4x4.hpp"
#include "core/camera/Camera.hpp"
#include <cmath>
#include <algorithm>

namespace ST {

// Orbit camera controller — modifies a Camera's m_eye in spherical coordinates
// around a target point. Used by the 2D editor's "3D view" mode.
//
// Inputs (call from editor event handlers):
//   - onMouseDrag(dx, dy)        — right mouse drag for orbit (yaw/pitch)
//   - onMouseWheel(deltaY)       — zoom by adjusting orbit radius
//   - onKeyboardPan(dt, ...)     — WASD movement translated into horizontal target offset
//
// Conventions:
//   - Yaw (m_yaw) rotates around world Y, Pitch (m_pitch) tilts up/down.
//   - m_target is the focus point; m_eye = target + spherical(radius, yaw, pitch).
//   - m_radius is the distance from target to eye.
class OrbitCameraController {
public:
    OrbitCameraController()
        : m_target(0, 0, 0)
        , m_yaw(0.0f)
        , m_pitch(0.3f)
        , m_radius(800.0f)
        , m_minRadius(20.0f)
        , m_maxRadius(8000.0f)
        , m_orbitSpeed(0.005f)
        , m_panSpeed(0.4f)
        , m_zoomSpeed(1.1f)
        , m_minPitch(-1.55f)
        , m_maxPitch(1.55f)
    {}

    // Apply controller state into a Camera (m_eye / m_target / m_up).
    void applyTo(Camera& camera) const {
        camera.m_target = m_target;
        camera.m_eye = computeEye();
        // Keep up world-up; this is the convention used everywhere.
        camera.m_up = Vector3(0.0f, 1.0f, 0.0f);
    }

    Vector3 getTarget() const { return m_target; }
    void setTarget(const Vector3& t) { m_target = t; }

    float getYaw() const { return m_yaw; }
    void setYaw(float y) { m_yaw = y; }

    float getPitch() const { return m_pitch; }
    void setPitch(float p) {
        m_pitch = std::clamp(p, m_minPitch, m_maxPitch);
    }

    float getRadius() const { return m_radius; }
    void setRadius(float r) { m_radius = std::clamp(r, m_minRadius, m_maxRadius); }

    void resetToDefault() {
        m_target = Vector3(0, 0, 0);
        m_yaw = 0.0f;
        m_pitch = 0.3f;
        m_radius = 800.0f;
    }

    // ---- Input handlers (camera-space deltas only; the editor handles button states) ----

    // dx, dy: pixels since last call. Right-button orbit.
    void onOrbit(int dx, int dy) {
        m_yaw   -= dx * m_orbitSpeed;
        m_pitch -= dy * m_orbitSpeed;
        m_pitch  = std::clamp(m_pitch, m_minPitch, m_maxPitch);
    }

    // deltaY: wheel ticks (positive = zoom in / radius shrinks).
    void onZoom(float deltaY) {
        if (deltaY > 0) {
            m_radius /= m_zoomSpeed;
        } else if (deltaY < 0) {
            m_radius *= m_zoomSpeed;
        }
        m_radius = std::clamp(m_radius, m_minRadius, m_maxRadius);
    }

    // Pan target horizontally. forward/back/left/right in camera space are
    // computed from yaw. deltaSeconds = frame time, used for keyboard pan.
    // Used by WASD: forward=+zCamera, right=+xCamera.
    void panLocal(float forward, float right, float up) {
        Vector3 fwd = computeForwardFlat();
        Vector3 rightVec = computeRightFlat();
        Vector3 upVec(0.0f, 1.0f, 0.0f);
        m_target += fwd * (forward * m_panSpeed);
        m_target += rightVec * (right * m_panSpeed);
        m_target += upVec * (up * m_panSpeed);
    }

    // Pan in screen-pixel space (matches the 2D editor's drag-pan feel).
    // dx, dy: pixel deltas; converts to world-space by inverse mapping through
    // a given screen height and current radius (approx for orbit camera).
    void panByPixels(int dx, int dy, int canvasHeight) {
        if (canvasHeight <= 0) return;
        float worldPerPixel = (m_radius * 0.5f) / (float)canvasHeight;
        Vector3 rightVec = computeRightFlat();
        Vector3 upVec(0.0f, 1.0f, 0.0f);
        m_target -= rightVec * (dx * worldPerPixel);
        m_target -= upVec   * (dy * worldPerPixel);
    }

    Vector3 getEye() const { return computeEye(); }

private:
    Vector3 computeEye() const {
        // Yaw rotates around Y, pitch tilts around camera-X (X-like).
        float cy = std::cos(m_yaw);
        float sy = std::sin(m_yaw);
        float cp = std::cos(m_pitch);
        float sp = std::sin(m_pitch);

        // Spherical -> Cartesian:
        //   eye - target = (cos(pitch)*sin(yaw), sin(pitch), cos(pitch)*cos(yaw)) * r
        Vector3 offset(
            cp * sy * m_radius,
            sp * m_radius,
            cp * cy * m_radius
        );
        return m_target + offset;
    }

    // Horizontal forward direction at current yaw (ignores pitch) — used for WASD.
    Vector3 computeForwardFlat() const {
        float cy = std::cos(m_yaw);
        float sy = std::sin(m_yaw);
        return Vector3(cy, 0.0f, -sy).normalized();
    }

    Vector3 computeRightFlat() const {
        float cy = std::cos(m_yaw);
        float sy = std::sin(m_yaw);
        return Vector3(sy, 0.0f, cy).normalized();
    }

    Vector3 m_target;
    float   m_yaw;
    float   m_pitch;
    float   m_radius;

    float m_minRadius;
    float m_maxRadius;

    float m_orbitSpeed;
    float m_panSpeed;
    float m_zoomSpeed;
    float m_minPitch;
    float m_maxPitch;
};

} // namespace ST
