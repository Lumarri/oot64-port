cmake_minimum_required(VERSION 3.30)
project(MM3DGlesCompatibilityProbe LANGUAGES CXX)
if(NOT ANDROID OR NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "Use the Android NDK toolchain with arm64-v8a")
endif()
add_executable(mm3d_gles_probe main.cpp)
if(NOT EXISTS "${TRIAEVUM_SOURCE_DIR}/runtime/three_ds_recomp/src/fast/renderer3ds/pica_gl_shader_dialect.cpp")
    message(FATAL_ERROR "Set TRIAEVUM_SOURCE_DIR to the pinned TriAevum checkout")
endif()
target_sources(mm3d_gles_probe PRIVATE
    ../renderer/pica_gles_shader_dialect.cpp
    "${TRIAEVUM_SOURCE_DIR}/runtime/three_ds_recomp/src/fast/renderer3ds/pica_gl_shader_dialect.cpp")
target_include_directories(mm3d_gles_probe PRIVATE ../renderer
    "${TRIAEVUM_SOURCE_DIR}/runtime/three_ds_recomp/include")
target_compile_features(mm3d_gles_probe PRIVATE cxx_std_20)
target_compile_options(mm3d_gles_probe PRIVATE -Wall -Wextra -Werror)
target_link_libraries(mm3d_gles_probe PRIVATE EGL GLESv3)
target_link_options(mm3d_gles_probe PRIVATE -Wl,-z,max-page-size=16384)
