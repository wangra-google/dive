cmake_minimum_required(VERSION 3.18)
project(dive_replay)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(ExternalProject)

set(GFXR_PROJECT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/gfxreconstruct")
set(GFXR_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/gfxreconstruct")
set(VULKAN_HEADERS_DIR "${GFXR_PROJECT_DIR}/external/Vulkan-Headers")
set(VMA_DIR "${GFXR_PROJECT_DIR}/external/VulkanMemoryAllocator/include")

ExternalProject_Add(gfxr_dependency_build
    SOURCE_DIR ${GFXR_PROJECT_DIR}
    BINARY_DIR ${GFXR_BUILD_DIR}
    CMAKE_ARGS
        -G "Ninja"
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DANDROID_ABI=${ANDROID_ABI}
        -DANDROID_PLATFORM=${CMAKE_SYSTEM_VERSION}
        -DCMAKE_ANDROID_NDK=${ANDROID_NDK}
        -DCMAKE_BUILD_TYPE=Debug
        -DGFXR_BUILD_TOOLS=OFF
        -DGFXR_BUILD_SAMPLES=OFF
    DOWNLOAD_COMMAND ""
    UPDATE_COMMAND ""
    PATCH_COMMAND ""
    INSTALL_COMMAND ""
)

add_library(native_app_glue STATIC
    "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")

add_library(dive_replay SHARED
    dive_android_main.cpp
)

add_dependencies(dive_replay gfxr_dependency_build)


target_link_libraries(dive_replay PRIVATE
    # GFXR Libraries
    "${GFXR_BUILD_DIR}/framework/application/libgfxrecon-application.a"
    "${GFXR_BUILD_DIR}/framework/decode/libgfxrecon-decode.a"
    "${GFXR_BUILD_DIR}/framework/graphics/libgfxrecon-graphics.a"
    "${GFXR_BUILD_DIR}/framework/format/libgfxrecon-format.a"
    "${GFXR_BUILD_DIR}/framework/util/libgfxrecon-util.a"
    "${GFXR_BUILD_DIR}/framework/common/libgfxrecon-common.a"
    # 3rd-Party Libraries
    "${GFXR_BUILD_DIR}/external/SPIRV-Reflect/libspirv-reflect.a"
    "${GFXR_BUILD_DIR}/external/lz4/build/cmake/lib/liblz4_static.a"
    "${GFXR_BUILD_DIR}/external/zstd/build/cmake/lib/libzstd_static.a"
    "${GFXR_BUILD_DIR}/external/zlib/libzlibstatic.a"
    # Android Libraries
    native_app_glue
    android
    log
)

target_include_directories(dive_replay PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    "${ANDROID_NDK}/sources/android/native_app_glue"
    "${GFXR_PROJECT_DIR}/framework"
    "${GFXR_PROJECT_DIR}/external/SPIRV-Reflect/include"
    "${VULKAN_HEADERS_DIR}/include"
    "${VMA_DIR}"
)

target_compile_definitions(dive_replay PRIVATE
    "PROJECT_VERSION_HEADER_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/dummy_version.h\""
    VK_ENABLE_BETA_EXTENSIONS
)