cmake_minimum_required(VERSION 3.15)

# If cesium-native is included as a submodule, use its ezvcpkg script to fetch our dependencies.
# We must do this BEFORE project() so the toolchain is set correctly.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/cesium-native/cmake/ezvcpkg/ezvcpkg.cmake")
    set(VCPKG_MANIFEST_MODE OFF CACHE BOOL "Disable manifest mode to use ezvcpkg")

    # ezvcpkg needs to know the triplet before fetching. We use cesium-native's detection script.
    include("${CMAKE_CURRENT_SOURCE_DIR}/external/cesium-native/cmake/detect-vcpkg-triplet.cmake")
    if (NOT VCPKG_TRIPLET)
        if (DEFINED ENV{VCPKG_TRIPLET})
            set(VCPKG_TRIPLET "$ENV{VCPKG_TRIPLET}")
        elseif(DETECTED_VCPKG_TRIPLET_ERROR)
            message(FATAL_ERROR "${DETECTED_VCPKG_TRIPLET_ERROR}")
        elseif(DETECTED_VCPKG_TRIPLET STREQUAL "x64-windows")
            set(VCPKG_TRIPLET "x64-windows-static-md")
        else()
            set(VCPKG_TRIPLET "${DETECTED_VCPKG_TRIPLET}")
        endif()
    endif()

    include("${CMAKE_CURRENT_SOURCE_DIR}/external/cesium-native/cmake/ezvcpkg/ezvcpkg.cmake")
    ezvcpkg_fetch(
        COMMIT afc0a2e01ae104a2474216a2df0e8d78516fd5af
        REPO microsoft/vcpkg
        PACKAGES raylib nlohmann-json spdlog
        UPDATE_TOOLCHAIN
    )

    # The ezvcpkg toolchain does not reliably set CMAKE_PREFIX_PATH on its own.
    # cesium-native applies this workaround internally, and we must do the same here
    # so that find_package() works for the root project's dependencies.
    list(APPEND CMAKE_PREFIX_PATH "${EZVCPKG_DIR}/share")
    list(APPEND CMAKE_PREFIX_PATH "${EZVCPKG_DIR}")
endif()

project(CesiumRaylibViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/cesium-native/CMakeLists.txt")
    # Let cesium-native use ezvcpkg for its own dependencies.
    set(CESIUM_USE_EZVCPKG ON CACHE BOOL "Enable cesium-native ezvcpkg" FORCE)
    add_subdirectory(external/cesium-native)
else()
    find_package(cesium-native CONFIG REQUIRED)
endif()

find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(CesiumRaylibViewer
    src/main.cpp
    src/RaylibRenderer.cpp
    src/RaylibRenderer.h
    src/ApiServer.cpp
    src/ApiServer.h
    src/CameraControl.cpp
    src/CameraControl.h
)

target_link_libraries(CesiumRaylibViewer PRIVATE
    Cesium3DTilesSelection
    Cesium3DTilesContent
    CesiumAsync
    CesiumGltf
    CesiumUtility
    CesiumCurl
    raylib
    glfw
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(WIN32)
    target_link_libraries(CesiumRaylibViewer PRIVATE winmm)
endif()

# Copy resources if needed, but we load from URL/Network usually.
# If local files, we might need a post-build step.
