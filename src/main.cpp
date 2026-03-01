#include "RaylibRenderer.h"
#include "CameraControl.h"
#include "ApiServer.h"

#include <Cesium3DTilesSelection/Tileset.h>
#include <Cesium3DTilesSelection/TilesetOptions.h>
#include <Cesium3DTilesSelection/TilesetExternals.h>
#include <Cesium3DTilesContent/registerAllTileContentTypes.h>
#include <Cesium3DTilesSelection/ViewUpdateResult.h>
#include <Cesium3DTilesSelection/ViewState.h>
#include <CesiumAsync/AsyncSystem.h>
#include <CesiumAsync/ITaskProcessor.h>
#include <CesiumCurl/CurlAssetAccessor.h>
#include <CesiumUtility/CreditSystem.h>
#include <CesiumUtility/Math.h>
#include <CesiumGeospatial/LocalHorizontalCoordinateSystem.h>

#include <raylib.h>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <iostream>
#include <thread>
#include <filesystem>
#include <chrono>
#include <Cesium3DTilesSelection/BoundingVolume.h>

// TaskProcessor implementation for CesiumAsync
class SimpleTaskProcessor : public CesiumAsync::ITaskProcessor {
public:
    void startTask(std::function<void()> task) override {
        std::thread(task).detach();
    }
};

using namespace CesiumRaylib;

int main(int argc, char** argv) {
    // 0. Initialize Cesium Native
    Cesium3DTilesContent::registerAllTileContentTypes();

    // 1. Setup Window
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "Cesium Raylib Viewer");
    SetTargetFPS(60);

    // 2. Setup Cesium
    auto pTaskProcessor = std::make_shared<SimpleTaskProcessor>();
    CesiumAsync::AsyncSystem asyncSystem(pTaskProcessor);

    auto pAssetAccessor = std::make_shared<CesiumCurl::CurlAssetAccessor>();
    auto pCreditSystem = std::make_shared<CesiumUtility::CreditSystem>();
    auto pLogger = spdlog::stdout_color_mt("cesium");

    Cesium3DTilesSelection::TilesetExternals externals{
        pAssetAccessor,
        std::make_shared<RaylibRenderer>(),
        asyncSystem,
        pCreditSystem,
        pLogger
    };

    std::string tilesetUrl = "https://raw.githubusercontent.com/CesiumGS/3d-tiles-samples/main/1.0/TilesetWithDiscreteLOD/tileset.json";
    if (argc > 1) {
        tilesetUrl = argv[1];
    }

    Cesium3DTilesSelection::TilesetOptions options;
    options.enableFrustumCulling = true;
    options.forbidHoles = true;

    // Create Tileset
    auto pTileset = std::make_unique<Cesium3DTilesSelection::Tileset>(externals, tilesetUrl, options);

    // 3. Setup Camera and API
    CameraControl cameraControl;
    SharedState sharedState;
    ApiServer apiServer(sharedState);
    apiServer.start(8080);

    RenderTexture2D target = LoadRenderTexture(screenWidth, screenHeight);

    bool cameraInitializedToTileset = false;

    // Main Loop
    while (!WindowShouldClose()) {
        // Update
        float deltaTime = GetFrameTime();

        bool apiControlled = false;

        // Check API Request
        {
            std::unique_lock<std::mutex> lock(sharedState.mutex);
            if (sharedState.hasRequest) {
                apiControlled = true;
                const auto& req = sharedState.request;
                cameraControl.setPosition(req.latitude, req.longitude, req.height);
                cameraControl.setOrientation(req.heading, req.pitch, req.roll);
                cameraControl.setFov(req.fov);

                // Resize target if needed
                if (req.imageWidth != target.texture.width || req.imageHeight != target.texture.height) {
                    UnloadRenderTexture(target);
                    target = LoadRenderTexture(req.imageWidth, req.imageHeight);
                }

                // Wait for tiles to load (API mode)
                int maxFrames = 600; // ~10 seconds
                while (maxFrames-- > 0) {
                     pAssetAccessor->tick();
                     asyncSystem.dispatchMainThreadTasks();

                     // Update view state for loading
                     Camera3D currentCam = cameraControl.getCamera();
                     glm::dvec3 cPos = cameraControl.getPosition();
                     glm::dvec3 cDir = cameraControl.getDirection();
                     glm::dvec3 cUp = cameraControl.getUp();
                     double cFov = glm::radians(cameraControl.getFov());

                     Cesium3DTilesSelection::ViewState vs = Cesium3DTilesSelection::ViewState::create(
                        cPos, cDir, cUp, glm::dvec2(req.imageWidth, req.imageHeight), cFov, cFov
                     );

                     pTileset->updateView({vs}, 0.0f); // 0 delta time for loading loop

                     float progress = pTileset->computeLoadProgress();
                     if (progress >= 1.0f) {
                         break;
                     }

                     std::this_thread::sleep_for(std::chrono::milliseconds(16));
                }
            } else {
                cameraControl.update(deltaTime);
            }
        }

        pAssetAccessor->tick();
        asyncSystem.dispatchMainThreadTasks();

        // Auto-focus camera on the tileset once the root tile is loaded
        if (!cameraInitializedToTileset) {
            const auto* pRootTile = pTileset->getRootTile();
            if (pRootTile) {
                // Get the center of the bounding volume in ECEF
                glm::dvec3 center = Cesium3DTilesSelection::getBoundingVolumeCenter(pRootTile->getBoundingVolume());

                // Offset the camera slightly along the UP vector (surface normal) so we are looking at it
                CesiumGeospatial::LocalHorizontalCoordinateSystem lhcs(center);
                glm::dvec3 up = glm::dvec3(lhcs.getLocalToEcefTransformation()[2]);

                // 50 meters above center is usually better for individual models like the dragon
                cameraControl.setPositionEcef(center + up * 50.0);
                cameraControl.setOrientation(0.0, -45.0, 0.0); // Looking slightly down

                cameraInitializedToTileset = true;
                std::cout << "Camera centered on tileset root." << std::endl;
            }
        }

        // Calculate View State
        Camera3D rayCam = cameraControl.getCamera();
        glm::dvec3 camPos = cameraControl.getPosition();
        glm::dvec3 camDir = cameraControl.getDirection();
        glm::dvec3 camUp = cameraControl.getUp();

        double fov = glm::radians(cameraControl.getFov());
        int width = target.texture.width;
        int height = target.texture.height;

        Cesium3DTilesSelection::ViewState viewState = Cesium3DTilesSelection::ViewState::create(
            camPos,
            camDir,
            camUp,
            glm::dvec2(width, height),
            fov,
            fov // Vertical FOV approx
        );

        const auto& viewResult = pTileset->updateView({viewState}, deltaTime);

        // Debug printing (once per second to avoid spam)
        static float debugTimer = 0.0f;
        debugTimer += deltaTime;
        bool shouldPrint = false;
        if (debugTimer > 1.0f) {
            shouldPrint = true;
            debugTimer = 0.0f;
            std::cout << "Tiles to render: " << viewResult.tilesToRenderThisFrame.size() << " | Camera Pos: " << camPos.x << ", " << camPos.y << ", " << camPos.z << std::endl;
        }

        // Render
        BeginTextureMode(target);
            ClearBackground(DARKGRAY); // Darker background to see untextured white models
            BeginMode3D(rayCam);

                // Render Tiles
                for (const auto& pTile : viewResult.tilesToRenderThisFrame) {
                    if (!pTile) continue;

                    // In recent cesium-native, render resources are inside content
                    auto* pRenderContent = pTile->getContent().getRenderContent();
                    if (!pRenderContent) continue;

                    RaylibModel* pModel = static_cast<RaylibModel*>(pRenderContent->getRenderResources());
                    if (pModel && pModel->loaded) {
                        // Calculate RTC transform
                        // Tile transform is ECEF. Camera is ECEF.
                        // Raylib camera is at (0,0,0).
                        // Model transform should be (TilePos - CamPos).

                        // Wait, pModel->models is a list of Raylib Models.
                        // Each Model has a transform.
                        // We must update it.

                        // We stored the original tile transform in RaylibModel?
                        // No, I didn't store it in RaylibModel struct.
                        // I used `pLoadResult->transform` to set `model.transform` in `prepareInMainThread`.
                        // But that baked the ECEF transform into the model's transform matrix.
                        // Since Raylib uses float, this is bad for precision.
                        // But I can't change it easily without re-uploading or modifying matrix every frame.
                        // `Model.transform` IS the matrix.
                        // So I can modify `model.transform`.

                        // I need the original double transform.
                        // Use the transform stored in load result which includes glTF corrections.
                        glm::dmat4 tileTransform = glm::dmat4(1.0);
                        if (pModel->pLoadResult) {
                            tileTransform = pModel->pLoadResult->transform;
                        } else {
                            tileTransform = pTile->getTransform();
                        }

                        // Compute relative transform
                        glm::dmat4 relTransform = tileTransform;
                        relTransform[3] = glm::dvec4(
                            glm::dvec3(tileTransform[3]) - camPos,
                            1.0
                        );

                        if (shouldPrint) {
                            std::cout << "  Tile relative pos: " << relTransform[3].x << ", " << relTransform[3].y << ", " << relTransform[3].z << std::endl;
                        }

                        // Convert to float matrix
                        Matrix rayMat;
                        glm::mat4 m = static_cast<glm::mat4>(relTransform);
                        std::memcpy(&rayMat, glm::value_ptr(m), sizeof(float) * 16);

                        for (auto& model : pModel->models) {
                            model.transform = rayMat;
                            DrawModel(model, {0,0,0}, 1.0f, WHITE);

                            // Debug: Draw a bounding box for the model to ensure it's not invisible due to scale
                            DrawBoundingBox(GetModelBoundingBox(model), RED);
                        }
                    }
                }

            EndMode3D();

            // Draw Credits
            // pCreditSystem->getCreditsToShowThisFrame()

        EndTextureMode();

        // Draw to Screen
        BeginDrawing();
            ClearBackground(BLACK);
            // Draw the render texture to screen, flip Y
            DrawTexturePro(
                target.texture,
                { 0.0f, 0.0f, (float)target.texture.width, (float)-target.texture.height },
                { 0.0f, 0.0f, (float)screenWidth, (float)screenHeight },
                { 0.0f, 0.0f },
                0.0f,
                WHITE
            );
            DrawFPS(10, 10);
        EndDrawing();

        // Handle API Result
        if (apiControlled) {
            Image img = LoadImageFromTexture(target.texture);
            ImageFlipVertical(&img);

            int size = 0;
            unsigned char* data = ExportImageToMemory(img, ".png", &size);

            std::unique_lock<std::mutex> lock(sharedState.mutex);
            sharedState.resultImage.assign(data, data + size);
            sharedState.resultReady = true;
            sharedState.hasRequest = false;
            sharedState.cv.notify_one();

            UnloadImage(img);
            RL_FREE(data);
        }
    }

    // Cleanup
    apiServer.stop();
    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
