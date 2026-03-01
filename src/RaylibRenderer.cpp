#include "RaylibRenderer.h"
#include <CesiumGltf/AccessorView.h>
#include <CesiumGltf/Image.h>
#include <CesiumGltf/Material.h>
#include <CesiumGltf/Texture.h>
#include <Cesium3DTilesSelection/TileLoadResult.h>
#include <CesiumAsync/AsyncSystem.h>
#include <iostream>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>

namespace CesiumRaylib {

RaylibRenderer::RaylibRenderer() {
}

RaylibRenderer::~RaylibRenderer() {
}

CesiumAsync::Future<Cesium3DTilesSelection::TileLoadResultAndRenderResources>
RaylibRenderer::prepareInLoadThread(
    const CesiumAsync::AsyncSystem& asyncSystem,
    Cesium3DTilesSelection::TileLoadResult&& tileLoadResult,
    const glm::dmat4& transform,
    const std::any& rendererOptions)
{
    CesiumGltf::Model* pModel = std::get_if<CesiumGltf::Model>(&tileLoadResult.contentKind);
    if (!pModel) {
        return asyncSystem.createResolvedFuture(
            Cesium3DTilesSelection::TileLoadResultAndRenderResources{
                std::move(tileLoadResult),
                nullptr
            }
        );
    }

    auto pLoadResult = std::make_unique<RaylibModelLoadResult>();
    pLoadResult->transform = transform;

    // Load Textures
    for (const auto& texture : pModel->textures) {
        RaylibTexture rayTexture;
        rayTexture.width = 0;
        rayTexture.height = 0;
        rayTexture.format = 0;
        rayTexture.mipmaps = 1;

        if (texture.source >= 0 && texture.source < static_cast<int32_t>(pModel->images.size())) {
            const auto& image = pModel->images[texture.source];
            // Access decoded data from cesium::data if available (set by CesiumGltfReader)
             if (image.pAsset && !image.pAsset->pixelData.empty()) {
                rayTexture.width = image.pAsset->width;
                rayTexture.height = image.pAsset->height;
                // Assuming RGBA8 or RGB8. CesiumGltfReader usually gives RGBA8?
                // Raylib needs raw data.
                rayTexture.data.resize(image.pAsset->pixelData.size());
                std::memcpy(rayTexture.data.data(), image.pAsset->pixelData.data(), image.pAsset->pixelData.size());

                if (image.pAsset->channels == 4) {
                    rayTexture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                } else if (image.pAsset->channels == 3) {
                    rayTexture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
                } else {
                     // Unsupported for now
                }
            }
        }
        pLoadResult->textures.push_back(std::move(rayTexture));
    }

    // Traverse glTF nodes to calculate absolute transforms and extract meshes
    auto traverseNode = [&](int nodeId, const glm::dmat4& parentTransform, auto& traverseRef) -> void {
        if (nodeId < 0 || nodeId >= static_cast<int>(pModel->nodes.size())) return;

        const auto& node = pModel->nodes[nodeId];
        glm::dmat4 nodeTransform(1.0);

        if (node.matrix.size() == 16) {
            nodeTransform = glm::dmat4(
                node.matrix[0], node.matrix[1], node.matrix[2], node.matrix[3],
                node.matrix[4], node.matrix[5], node.matrix[6], node.matrix[7],
                node.matrix[8], node.matrix[9], node.matrix[10], node.matrix[11],
                node.matrix[12], node.matrix[13], node.matrix[14], node.matrix[15]
            );
        } else {
            if (node.translation.size() == 3) {
                nodeTransform = glm::translate(nodeTransform, glm::dvec3(node.translation[0], node.translation[1], node.translation[2]));
            }
            if (node.rotation.size() == 4) {
                glm::dquat q(node.rotation[3], node.rotation[0], node.rotation[1], node.rotation[2]);
                nodeTransform = nodeTransform * glm::mat4_cast(q);
            }
            if (node.scale.size() == 3) {
                nodeTransform = glm::scale(nodeTransform, glm::dvec3(node.scale[0], node.scale[1], node.scale[2]));
            }
        }

        glm::dmat4 absoluteTransform = parentTransform * nodeTransform;
        glm::dmat3 normalTransform = glm::dmat3(glm::transpose(glm::inverse(absoluteTransform)));

        if (node.mesh >= 0 && node.mesh < static_cast<int>(pModel->meshes.size())) {
            const auto& mesh = pModel->meshes[node.mesh];
            for (const auto& primitive : mesh.primitives) {
                RaylibMeshPrimitive rayPrimitive;
                rayPrimitive.materialIndex = primitive.material;

                auto positionAccessorIt = primitive.attributes.find("POSITION");
                if (positionAccessorIt != primitive.attributes.end()) {
                    CesiumGltf::AccessorView<glm::vec3> positions(*pModel, positionAccessorIt->second);
                    if (positions.status() == CesiumGltf::AccessorViewStatus::Valid) {
                        for (int i = 0; i < positions.size(); ++i) {
                            glm::dvec3 p = absoluteTransform * glm::dvec4(positions[i].x, positions[i].y, positions[i].z, 1.0);
                            rayPrimitive.vertices.push_back(static_cast<float>(p.x));
                            rayPrimitive.vertices.push_back(static_cast<float>(p.y));
                            rayPrimitive.vertices.push_back(static_cast<float>(p.z));
                        }
                    }
                }

                auto normalAccessorIt = primitive.attributes.find("NORMAL");
                if (normalAccessorIt != primitive.attributes.end()) {
                    CesiumGltf::AccessorView<glm::vec3> normals(*pModel, normalAccessorIt->second);
                    if (normals.status() == CesiumGltf::AccessorViewStatus::Valid) {
                        for (int i = 0; i < normals.size(); ++i) {
                            glm::dvec3 n = glm::normalize(normalTransform * glm::dvec3(normals[i].x, normals[i].y, normals[i].z));
                            rayPrimitive.normals.push_back(static_cast<float>(n.x));
                            rayPrimitive.normals.push_back(static_cast<float>(n.y));
                            rayPrimitive.normals.push_back(static_cast<float>(n.z));
                        }
                    }
                }

                auto texCoordAccessorIt = primitive.attributes.find("TEXCOORD_0");
                if (texCoordAccessorIt != primitive.attributes.end()) {
                    CesiumGltf::AccessorView<glm::vec2> texcoords(*pModel, texCoordAccessorIt->second);
                    if (texcoords.status() == CesiumGltf::AccessorViewStatus::Valid) {
                        for (int i = 0; i < texcoords.size(); ++i) {
                            glm::vec2 uv = texcoords[i];
                            rayPrimitive.texcoords.push_back(uv.x);
                            rayPrimitive.texcoords.push_back(uv.y);
                        }
                    }
                }

                if (primitive.indices >= 0) {
                     CesiumGltf::AccessorView<uint32_t> indices(*pModel, primitive.indices);
                     if (indices.status() == CesiumGltf::AccessorViewStatus::Valid) {
                          for (int i = 0; i < indices.size(); ++i) {
                              rayPrimitive.indices.push_back(static_cast<unsigned short>(indices[i]));
                          }
                     } else {
                         CesiumGltf::AccessorView<uint16_t> indices16(*pModel, primitive.indices);
                         if (indices16.status() == CesiumGltf::AccessorViewStatus::Valid) {
                              for (int i = 0; i < indices16.size(); ++i) {
                                  rayPrimitive.indices.push_back(indices16[i]);
                              }
                         }
                     }
                }

                pLoadResult->primitives.push_back(std::move(rayPrimitive));
            }
        }

        for (int childId : node.children) {
            traverseRef(childId, absoluteTransform, traverseRef);
        }
    };

    if (pModel->scene >= 0 && pModel->scene < static_cast<int>(pModel->scenes.size())) {
        for (int rootNodeId : pModel->scenes[pModel->scene].nodes) {
            traverseNode(rootNodeId, glm::dmat4(1.0), traverseNode);
        }
    }

    return asyncSystem.createResolvedFuture(
        Cesium3DTilesSelection::TileLoadResultAndRenderResources{
            std::move(tileLoadResult),
            pLoadResult.release()
        }
    );
}

void* RaylibRenderer::prepareInMainThread(
    Cesium3DTilesSelection::Tile& tile,
    void* pLoadThreadResult)
{
    if (!pLoadThreadResult) return nullptr;

    RaylibModelLoadResult* pLoadResult = static_cast<RaylibModelLoadResult*>(pLoadThreadResult);
    RaylibModel* pModel = new RaylibModel();

    // Convert transform to Raylib Matrix
    // Raylib Matrix is column-major float[16]
    glm::mat4 m = static_cast<glm::mat4>(pLoadResult->transform);
    Matrix mat;
    std::memcpy(&mat, glm::value_ptr(m), sizeof(float) * 16);

    // Create textures
    for (const auto& rayTex : pLoadResult->textures) {
        if (!rayTex.data.empty()) {
            Image img = { 0 };
            img.data = (void*)rayTex.data.data(); // Temporary pointer, used by LoadTextureFromImage
            img.width = rayTex.width;
            img.height = rayTex.height;
            img.format = rayTex.format;
            img.mipmaps = 1;

            Texture2D tex = LoadTextureFromImage(img);
            pModel->textures.push_back(tex);
        } else {
            pModel->textures.push_back({ 0 }); // Invalid texture placeholder
        }
    }

    // Create meshes
    for (const auto& primitive : pLoadResult->primitives) {
        if (primitive.vertices.empty()) continue;

        Mesh mesh = { 0 };
        mesh.vertexCount = primitive.vertices.size() / 3;
        mesh.triangleCount = primitive.indices.size() / 3;

        mesh.vertices = (float*)MemAlloc(primitive.vertices.size() * sizeof(float));
        std::memcpy(mesh.vertices, primitive.vertices.data(), primitive.vertices.size() * sizeof(float));

        if (!primitive.normals.empty()) {
            mesh.normals = (float*)MemAlloc(primitive.normals.size() * sizeof(float));
            std::memcpy(mesh.normals, primitive.normals.data(), primitive.normals.size() * sizeof(float));
        }

        if (!primitive.texcoords.empty()) {
            mesh.texcoords = (float*)MemAlloc(primitive.texcoords.size() * sizeof(float));
            std::memcpy(mesh.texcoords, primitive.texcoords.data(), primitive.texcoords.size() * sizeof(float));
        }

        if (!primitive.indices.empty()) {
            mesh.indices = (unsigned short*)MemAlloc(primitive.indices.size() * sizeof(unsigned short));
            std::memcpy(mesh.indices, primitive.indices.data(), primitive.indices.size() * sizeof(unsigned short));
        }

        UploadMesh(&mesh, false);

        Model model = LoadModelFromMesh(mesh);
        model.transform = mat; // Apply tile transform

        // Apply texture if exists
        if (primitive.materialIndex >= 0 && primitive.materialIndex < pModel->textures.size()) {
            Texture2D tex = pModel->textures[primitive.materialIndex];
            if (tex.id > 0) {
                SetMaterialTexture(&model.materials[0], MATERIAL_MAP_DIFFUSE, tex);
            }
        }

        pModel->models.push_back(model);
    }

    pModel->loaded = true;

    // Transfer ownership of pLoadResult to pModel, but clear large data buffers
    // as they are already uploaded to GPU.
    pLoadResult->primitives.clear();
    pLoadResult->textures.clear();
    pModel->pLoadResult = pLoadResult;

    return pModel;
}

void RaylibRenderer::free(
    Cesium3DTilesSelection::Tile& tile,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept
{
    RaylibModel* pModel = static_cast<RaylibModel*>(pMainThreadResult);

    if (pModel) {
        for (auto& model : pModel->models) {
             UnloadModel(model);
        }
        for (auto& tex : pModel->textures) {
            if (tex.id > 0) {
                UnloadTexture(tex);
            }
        }
        if (pModel->pLoadResult) {
            delete pModel->pLoadResult;
        }
        delete pModel;
    }

    if (pLoadThreadResult) {
        // If pModel was null, or pLoadThreadResult is different (shouldn't happen if pModel took ownership),
        // we must delete it.
        // If pModel took ownership, pModel->pLoadResult == pLoadThreadResult, and it was deleted above.
        bool alreadyDeleted = (pModel && pModel->pLoadResult == pLoadThreadResult);
        if (!alreadyDeleted) {
            delete static_cast<RaylibModelLoadResult*>(pLoadThreadResult);
        }
    }
}

void RaylibRenderer::attachRasterInMainThread(
    const Cesium3DTilesSelection::Tile& tile,
    int32_t overlayTextureCoordinateID,
    const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pMainThreadRendererResources,
    const glm::dvec2& translation,
    const glm::dvec2& scale)
{
    // Not implemented
}

void RaylibRenderer::detachRasterInMainThread(
    const Cesium3DTilesSelection::Tile& tile,
    int32_t overlayTextureCoordinateID,
    const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pMainThreadRendererResources) noexcept
{
    // Not implemented
}

void* RaylibRenderer::prepareRasterInLoadThread(
    CesiumGltf::ImageAsset& image,
    const std::any& rendererOptions)
{
    return nullptr;
}

void* RaylibRenderer::prepareRasterInMainThread(
    CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pLoadThreadResult)
{
    return nullptr;
}

void RaylibRenderer::freeRaster(
    const CesiumRasterOverlays::RasterOverlayTile& rasterTile,
    void* pLoadThreadResult,
    void* pMainThreadResult) noexcept
{
}

}
