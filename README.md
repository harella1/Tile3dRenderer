# Cesium Raylib Viewer

A C++ project to render 3D Tiles using [Cesium Native](https://github.com/CesiumGS/cesium-native) and [raylib](https://www.raylib.com/).

## Features

- Loads 3D Tiles from URL (default: Cesium Samples).
- Interactive camera control (WASD + Mouse).
- API for remote rendering (POST `/render`).
- Renders glTF tiles with basic textures.

## Prerequisites

- CMake 3.15+
- C++20 Compiler
- vcpkg (Recommended) or manual installation of dependencies.

## Dependencies

- cesium-native
- raylib
- cpp-httplib
- nlohmann-json
- spdlog
- OpenSSL (required for HTTPS)

## Build Instructions

This project uses `ezvcpkg` (bundled with `cesium-native`) to automatically download and build its dependencies (`raylib`, `nlohmann-json`, and `cesium-native`'s dependencies). You do not need to install `vcpkg` manually.

1.  Clone this repository with its submodules:
    ```bash
    git clone --recursive <repository_url>
    ```

2.  Configure with CMake:
    ```bash
    cmake -B build -S .
    ```
    *Note: This step might take a while on the first run as it downloads and builds all dependencies.*

3.  Build:
    ```bash
    cmake --build build
    ```

## Usage

Run the executable:

```bash
./build/CesiumRaylibViewer [tileset_url]
```

Default tileset: `https://raw.githubusercontent.com/CesiumGS/3d-tiles-samples/master/1.0/TilesetWithDiscreteLOD/tileset.json`

### Interactive Controls

- **W, A, S, D, Q, E**: Move camera (Forward, Left, Backward, Right, Down, Up).
- **Shift + Move**: Move faster.
- **Right Mouse Button + Drag**: Rotate camera (Yaw/Pitch).

### API

The viewer starts an HTTP server on port 8080.

**Endpoint:** `POST /render`

**Body (JSON):**
```json
{
  "lat": 37.7749,
  "lon": -122.4194,
  "height": 500.0,
  "heading": 0.0,
  "pitch": -45.0,
  "roll": 0.0,
  "fov": 60.0,
  "width": 800,
  "height": 600
}
```

**Response:** PNG image.

## Notes

- This is a prototype viewer. It handles basic glTF features but may not support advanced PBR, Draco compression (unless built into cesium-native), or complex extensions.
- Precision: Uses Relative-To-Camera rendering to handle large coordinates, but artifacts may appear at extreme distances.
- API is blocking (one request at a time).
