cmake_minimum_required(VERSION 3.30)
project(MM3DPrivateAndroidTitle LANGUAGES C CXX)
if(NOT ANDROID OR NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "The private title requires Android ARM64")
endif()
if(NOT EXISTS "${TRIAEVUM_SOURCE_DIR}/ports/android/native/CMakeLists.txt")
    message(FATAL_ERROR "Set TRIAEVUM_SOURCE_DIR to the clean pinned checkout")
endif()
add_subdirectory("${TRIAEVUM_SOURCE_DIR}/ports/android/native" upstream-native)
set(title_host "${TRIAEVUM_SOURCE_DIR}/tools/oot3d/native_game_runtime")
set(a32 "${TRIAEVUM_SOURCE_DIR}/tools/oot3d/native_a32_runtime")
add_library(mm3d_title_loader STATIC
    "${title_host}/oot3d_native_direct_aot.cpp"
    "${a32}/upstream/recomp/a32_core.cpp"
    "${a32}/upstream/recomp/a32_core_alu.cpp"
    "${a32}/upstream/recomp/a32_core_memory.cpp"
    "${a32}/upstream/recomp/a32_runtime.cpp"
    "${a32}/a32_thumb.cpp")
target_compile_definitions(mm3d_title_loader PRIVATE OOT3D_NATIVE_DIRECT_AOT_PLUGIN=1 OOT3D_NATIVE_GENERATED_WHOLE_AOT=1 NOMINMAX)
target_compile_options(mm3d_title_loader PRIVATE -O1 -ffp-model=strict)
target_link_libraries(mm3d_title_loader PUBLIC triaevum_title_whole_aot_support ${CMAKE_DL_LIBS})
# Keep warnings strict for our probes without changing pinned upstream headers.
target_include_directories(mm3d_title_loader SYSTEM PUBLIC
    $<TARGET_PROPERTY:triaevum_title_whole_aot_support,INTERFACE_INCLUDE_DIRECTORIES>)
add_executable(mm3d_title_probe main.cpp)
add_executable(mm3d_startup_probe startup.cpp initial_state.cpp)
foreach(probe IN ITEMS mm3d_title_probe mm3d_startup_probe)
    target_compile_options(${probe} PRIVATE -O1 -ffp-model=strict -Wall -Wextra -Werror)
    target_link_libraries(${probe} PRIVATE mm3d_title_loader)
    target_link_options(${probe} PRIVATE -Wl,-z,max-page-size=16384)
endforeach()
