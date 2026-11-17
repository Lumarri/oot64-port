cmake_minimum_required(VERSION 3.30)
project(MM3DGlesCompatibilityProbe LANGUAGES C CXX)
option(MM3D_HOST_GLES_PROBE "Run the synthetic GLES probe on a Linux host" OFF)
option(MM3D_BUILD_WINDOW_APP "Build the ROM-free NativeActivity prototype" OFF)
if(NOT MM3D_HOST_GLES_PROBE AND (NOT ANDROID OR NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a"))
    message(FATAL_ERROR "Use the Android NDK toolchain with arm64-v8a")
endif()
add_executable(mm3d_gles_probe main.cpp)
if(NOT EXISTS "${TRIAEVUM_SOURCE_DIR}/runtime/three_ds_recomp/src/fast/renderer3ds/pica_gl_shader_dialect.cpp")
    message(FATAL_ERROR "Set TRIAEVUM_SOURCE_DIR to the pinned TriAevum checkout")
endif()
add_library(mm3d_gles_renderer STATIC
    ../renderer/gles_render_target.cpp
    ../renderer/gles_scanout.cpp
    ../renderer/pica_gles_backend.cpp
    ../renderer/pica_gles_draw.cpp
    ../renderer/pica_gles_shader_dialect.cpp
    "${TRIAEVUM_SOURCE_DIR}/runtime/three_ds_recomp/src/fast/renderer3ds/pica_gl_shader_dialect.cpp")
set_target_properties(mm3d_gles_renderer PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(mm3d_gles_renderer PUBLIC ../renderer
    "${TRIAEVUM_SOURCE_DIR}/runtime/three_ds_recomp/include")
target_compile_features(mm3d_gles_renderer PUBLIC cxx_std_20)
target_compile_options(mm3d_gles_renderer PRIVATE -Wall -Wextra -Werror)
target_compile_options(mm3d_gles_probe PRIVATE -Wall -Wextra -Werror)
set(pica_frontend "${TRIAEVUM_SOURCE_DIR}/tools/oot3d/native_pica_frontend")
set(pica_runtime "${TRIAEVUM_SOURCE_DIR}/runtime/three_ds_recomp")
add_library(mm3d_pica_generator STATIC
    "${pica_frontend}/oot3d_native_pica_fragment_shader_gen.cpp"
    "${pica_frontend}/oot3d_native_pica_fragment_lighting_gen.cpp"
    "${pica_frontend}/oot3d_native_pica_proctex.cpp"
    "${pica_runtime}/src/fast/extension/pica_fragment_lighting.cpp"
    "${pica_runtime}/src/fast/extension/pica_texture_decode.cpp"
    "${pica_runtime}/src/fast/renderer3ds/pica_tev_program.cpp")
target_compile_features(mm3d_pica_generator PUBLIC cxx_std_20)
set_target_properties(mm3d_pica_generator PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(mm3d_pica_generator SYSTEM PUBLIC
    "${pica_frontend}" "${pica_runtime}/include"
    "${TRIAEVUM_SOURCE_DIR}/tools/triaevum_release/vendor/include")
target_link_libraries(mm3d_gles_renderer PUBLIC mm3d_pica_generator)
target_link_libraries(mm3d_gles_probe PRIVATE mm3d_gles_renderer)
if(MM3D_HOST_GLES_PROBE)
    if(ANDROID OR NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "The host probe requires Linux without the Android toolchain")
    endif()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLES REQUIRED IMPORTED_TARGET egl glesv2)
    target_link_libraries(mm3d_gles_renderer PUBLIC PkgConfig::GLES)
    enable_testing()
    add_test(NAME synthetic_pica_gles COMMAND mm3d_gles_probe)
    set_tests_properties(synthetic_pica_gles PROPERTIES
        ENVIRONMENT "EGL_PLATFORM=surfaceless;LIBGL_ALWAYS_SOFTWARE=1" TIMEOUT 60)
else()
    target_link_libraries(mm3d_gles_renderer PUBLIC EGL GLESv3)
    target_link_options(mm3d_gles_probe PRIVATE -Wl,-z,max-page-size=16384)
endif()
if(MM3D_BUILD_WINDOW_APP)
    if(MM3D_HOST_GLES_PROBE OR NOT ANDROID)
        message(FATAL_ERROR "The window prototype requires the Android toolchain")
    endif()
    add_library(mm3d_native_app_glue STATIC "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
    set_target_properties(mm3d_native_app_glue PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_include_directories(mm3d_native_app_glue PUBLIC "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
    add_library(mm3d_window SHARED ../window_app/native_main.cpp)
    target_compile_options(mm3d_window PRIVATE -Wall -Wextra -Werror)
    target_link_libraries(mm3d_window PRIVATE mm3d_gles_renderer mm3d_native_app_glue android log)
    target_link_options(mm3d_window PRIVATE -Wl,-z,max-page-size=16384 -Wl,-u,ANativeActivity_onCreate)
endif()
