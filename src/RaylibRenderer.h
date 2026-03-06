#pragma once

#include <Cesium3DTilesSelection/IPrepareRendererResources.h>
#include <CesiumGltf/Model.h>
#include <raylib.h>
#include <vector>
#include <memory>

namespace CesiumRaylib {

struct RaylibMeshPrimitive {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<float> texcoords;
    std::vector<unsigned short> indices;
    int materialIndex = -1;
};

struct RaylibTexture {
    std::vector<unsigned char> data;
    int width;
    int height;
    int format; // raylib PixelFormat
    int mipmaps;
};

struct RaylibModelLoadResult {
    std::vector<RaylibMeshPrimitive> primitives;
    std::vector<RaylibTexture> textures;
    glm::dmat4 transform;
};

struct RaylibModel {
    std::vector<Model> models;
    std::vector<Texture2D> textures; // Store textures to manage lifetime
    RaylibModelLoadResult* pLoadResult = nullptr; // Keep load result for proper cleanup
    bool loaded = false;
};

class RaylibRenderer : public Cesium3DTilesSelection::IPrepareRendererResources {
public:
    RaylibRenderer();
    ~RaylibRenderer() override;

    CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources>
    prepareInLoadThread(
        const CesiumAsync::AsyncSystem& asyncSystem,
        Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
        const glm::dmat4& transform,
        const std::any& rendererOptions) override;

    void* prepareInMainThread(
        Cesium3DTilesSelection::Tile& tile,
        void* pLoadThreadResult) override;

    void free(
        Cesium3DTilesSelection::Tile& tile,
        void* pLoadThreadResult,
        void* pMainThreadResult) noexcept override;

    void attachRasterInMainThread(
        const Cesium3DTilesSelection::Tile& tile,
        int32_t overlayTextureCoordinateID,
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pMainThreadRendererResources,
        const glm::dvec2& translation,
        const glm::dvec2& scale) override;

    void detachRasterInMainThread(
        const Cesium3DTilesSelection::Tile& tile,
        int32_t overlayTextureCoordinateID,
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pMainThreadRendererResources) noexcept override;

    void* prepareRasterInLoadThread(
        CesiumGltf::ImageAsset& image,
        const std::any& rendererOptions) override;

    void* prepareRasterInMainThread(
        CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pLoadThreadResult) override;

    void freeRaster(
        const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
        void* pLoadThreadResult,
        void* pMainThreadResult) noexcept override;
};

}
