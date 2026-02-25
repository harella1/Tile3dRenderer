#pragma once

#include <raylib.h>
#include <glm/vec3.hpp>

namespace CesiumRaylib {

class CameraControl {
public:
    CameraControl();

    void update(float deltaTime);

    // Set camera from geodetic coordinates (degrees, meters)
    void setPosition(double latitude, double longitude, double height);
    void setOrientation(double heading, double pitch, double roll);
    void setFov(double fovDegrees);

    Camera3D getCamera() const;

    // Get current position in ECEF or Geodetic?
    // Raylib uses simple 3D world space.
    // We need to map ECEF to Raylib world space.
    // Usually we keep camera at origin or move world?
    // Cesium Native handles ECEF.
    // We'll use a simple camera model where camera moves in ECEF.
    // But precision issues with float (raylib uses float).
    // We need to subtract camera position (RTC) for rendering.
    // But Raylib's Camera3D uses float vectors.
    // So we must implement RTC (Relative To Center) rendering.
    // For now, let's just use camera position as double, and return float camera relative to 0,0,0
    // And shift all tiles by camera position?
    // Cesium Native handles this via `TilesetOptions::enableFrustumCulling`?
    // No, we need to pass a transform to shader or modify model matrix.
    // Raylib `Model.transform`.
    // We can set `model.transform` to `tileTransform - cameraPosition`.
    // But `tileTransform` is double matrix. `cameraPosition` is double vec3.
    // Result is float matrix (if small enough).

    glm::dvec3 getPosition() const { return _position; }
    glm::dvec3 getDirection() const { return _direction; }
    glm::dvec3 getUp() const { return _up; }
    double getFov() const { return _fov; }
    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    void setSize(int width, int height) { _width = width; _height = height; }

private:
    glm::dvec3 _position; // ECEF
    glm::dvec3 _direction;
    glm::dvec3 _up;
    double _fov = 60.0;
    int _width = 800;
    int _height = 600;

    // Control state
    bool _isMouseDragging = false;
    Vector2 _lastMousePos = { 0 };
};

}
