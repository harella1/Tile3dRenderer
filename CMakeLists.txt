cmake_minimum_required(VERSION 3.15)

project(CesiumRaylibViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/cesium-native/CMakeLists.txt")
    add_subdirectory(external/cesium-native)
else()
    find_package(cesium-native CONFIG REQUIRED)
endif()

find_package(raylib CONFIG REQUIRED)
find_package(cpp-httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(CesiumRaylibViewer
    src/main.cpp
    src/RaylibRenderer.cpp
    src/RaylibRenderer.h
    src/SimpleAssetAccessor.cpp
    src/SimpleAssetAccessor.h
    src/ApiServer.cpp
    src/ApiServer.h
    src/CameraControl.cpp
    src/CameraControl.h
)

target_link_libraries(CesiumRaylibViewer PRIVATE
    Cesium3DTilesSelection
    CesiumAsync
    CesiumGltf
    CesiumUtility
    raylib
    httplib::httplib
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Copy resources if needed, but we load from URL/Network usually.
# If local files, we might need a post-build step.
