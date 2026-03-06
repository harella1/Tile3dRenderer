#include "CameraControl.h"
#include <CesiumGeospatial/Ellipsoid.h>
#include <CesiumGeospatial/Cartographic.h>
#include <CesiumGeospatial/LocalHorizontalCoordinateSystem.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

namespace CesiumRaylib {

CameraControl::CameraControl() {
    // Default position: San Francisco
    setPosition(37.7749, -122.4194, 500.0);
    setOrientation(0.0, -45.0, 0.0);
}

void CameraControl::update(float deltaTime) {
    // Simple WASD movement
    // But moving in ECEF is hard with simple vector addition (up vector changes).
    // We should move in ENU frame.

    CesiumGeospatial::LocalHorizontalCoordinateSystem lhcs(_position);
    glm::dmat4 enuToFixed = lhcs.getLocalToEcefTransformation();
    glm::dvec3 east = glm::dvec3(enuToFixed[0]);
    glm::dvec3 north = glm::dvec3(enuToFixed[1]);
    glm::dvec3 up = glm::dvec3(enuToFixed[2]);

    double speed = 10.0; // meters per second
    if (IsKeyDown(KEY_LEFT_SHIFT)) speed *= 5.0;

    glm::dvec3 move = glm::dvec3(0.0);
    if (IsKeyDown(KEY_W)) move += _direction;
    if (IsKeyDown(KEY_S)) move -= _direction;
    if (IsKeyDown(KEY_D)) move += glm::cross(_direction, up); // Right
    if (IsKeyDown(KEY_A)) move -= glm::cross(_direction, up); // Left
    if (IsKeyDown(KEY_Q)) move -= up;
    if (IsKeyDown(KEY_E)) move += up;

    if (glm::length(move) > 0.0) {
        _position += glm::normalize(move) * speed * (double)deltaTime;
    }

    // Mouse rotation
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
        Vector2 mousePos = GetMousePosition();
        if (!_isMouseDragging) {
            _isMouseDragging = true;
            _lastMousePos = mousePos;
        } else {
            Vector2 delta = { mousePos.x - _lastMousePos.x, mousePos.y - _lastMousePos.y };
            _lastMousePos = mousePos;

            float sensitivity = 0.002f;

            // Rotate around Up axis (Yaw)
            glm::dquat yawQuat = glm::angleAxis((double)(-delta.x * sensitivity), up);
            _direction = yawQuat * _direction;

            // Rotate around Right axis (Pitch)
            glm::dvec3 right = glm::cross(_direction, up);
            glm::dquat pitchQuat = glm::angleAxis((double)(-delta.y * sensitivity), right);
            _direction = pitchQuat * _direction;
        }
    } else {
        _isMouseDragging = false;
    }
}

void CameraControl::setPosition(double latitude, double longitude, double height) {
    _position = CesiumGeospatial::Ellipsoid::WGS84.cartographicToCartesian(
        CesiumGeospatial::Cartographic::fromDegrees(longitude, latitude, height)
    );
}

void CameraControl::setPositionEcef(const glm::dvec3& ecef) {
    _position = ecef;
}

void CameraControl::setOrientation(double heading, double pitch, double roll) {
    // Heading: 0 is North, 90 is East.
    // Pitch: -90 is looking down.
    // Roll: 0 is level.

    CesiumGeospatial::LocalHorizontalCoordinateSystem lhcs(_position);
    glm::dmat4 enuToFixed = lhcs.getLocalToEcefTransformation();

    // Create rotation in ENU
    // Cesium uses: x=East, y=North, z=Up.
    // Heading rotates around Z.
    // Pitch rotates around X (Right)? No, Y is North. Right is East (X).
    // Let's use standard conversions.

    // Heading (Yaw)
    // Pitch
    // Roll

    // We want direction vector.
    // Start with North (0, 1, 0) in ENU.
    // Rotate by Heading around Z.
    // Rotate by Pitch around Right.

    // Use CesiumTransforms to get rotation matrix?
    // Or just construct manually.

    // Convert degrees to radians
    double h = glm::radians(heading);
    double p = glm::radians(pitch);
    double r = glm::radians(roll);

    // Heading is rotation from North towards East?
    // Standard: Heading 0 = North. 90 = East.
    // In ENU: North is +Y. East is +X.
    // Rotation -H around Z (Up)?

    // Let's use a simpler approach: define direction in ENU, then transform to Fixed.

    // Direction in local ENU:
    // Z is Up.
    // Y is North.
    // X is East.
    // Pitch -90 means looking down (-Z).
    // Heading 0 means looking North (+Y).

    // Rotate vector (0, 1, 0) by pitch around X axis -> (0, cos(p), sin(p)).
    // Wait, pitch is usually down negative.

    // Let's use glm quaternion.
    // Identity is North (+Y).
    // Pitch rotates around East (+X).
    // Heading rotates around Up (+Z).

    glm::dquat qPitch = glm::angleAxis(p, glm::dvec3(1, 0, 0));
    glm::dquat qHeading = glm::angleAxis(-h, glm::dvec3(0, 0, 1)); // -h because heading is clockwise usually?
    glm::dquat qRoll = glm::angleAxis(r, glm::dvec3(0, 1, 0)); // Roll around North (Forward)?

    glm::dquat orientation = qHeading * qPitch * qRoll;
    glm::dvec3 localDir = orientation * glm::dvec3(0, 1, 0); // Start looking North
    glm::dvec3 localUp = orientation * glm::dvec3(0, 0, 1); // Start Up

    // Transform to Fixed
    glm::dmat3 rotation = glm::dmat3(enuToFixed);
    _direction = rotation * localDir;
    _up = rotation * localUp;
}

void CameraControl::setFov(double fovDegrees) {
    _fov = fovDegrees;
}

Camera3D CameraControl::getCamera() const {
    // Return a camera at (0,0,0) looking at direction with up.
    // Models will be shifted relative to this.
    Camera3D cam = { 0 };
    cam.position = { 0.0f, 0.0f, 0.0f };
    cam.target = { (float)_direction.x, (float)_direction.y, (float)_direction.z };
    cam.up = { (float)_up.x, (float)_up.y, (float)_up.z };
    cam.fovy = (float)_fov;
    cam.projection = CAMERA_PERSPECTIVE;
    return cam;
}

}
