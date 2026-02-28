cmake_minimum_required(VERSION 3.15)

project(CesiumRaylibViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Fetch dependencies we control (raylib, nlohmann_json) directly from source
include(FetchContent)

# Fetch raylib
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        master # or 5.0
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# Fetch nlohmann_json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

# 2. Add cesium-native, which will handle its own heavy dependencies using ezvcpkg
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/cesium-native/CMakeLists.txt")
    # Let cesium-native use ezvcpkg for its own dependencies.
    set(CESIUM_USE_EZVCPKG ON CACHE BOOL "Enable cesium-native ezvcpkg" FORCE)
    add_subdirectory(external/cesium-native)
else()
    find_package(cesium-native CONFIG REQUIRED)
endif()

# 3. Find packages that cesium-native installed for us
find_package(cpp-httplib CONFIG REQUIRED)
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
