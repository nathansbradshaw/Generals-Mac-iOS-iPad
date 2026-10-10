cmake_minimum_required(VERSION 3.20)

project(GeneralsXAndroidDxvkConfiguration NONE)

foreach(required_variable IN ITEMS ANDROID_NDK_ROOT ANDROID_API ANDROID_ABI SDL3_ROOT OUTPUT_DIRECTORY)
    if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
        message(FATAL_ERROR "${required_variable} must be provided")
    endif()
endforeach()

if(NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "The initial Android port supports only arm64-v8a")
endif()

set(ANDROID_LLVM_BIN "${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/darwin-x86_64/bin")
set(ANDROID_TARGET "aarch64-linux-android${ANDROID_API}")
set(SDL3_INCLUDE_DIR "${SDL3_ROOT}/prefab/modules/SDL3-Headers/include")
set(SDL3_LIBRARY_DIR "${SDL3_ROOT}/prefab/modules/SDL3-shared/libs/android.arm64-v8a")

foreach(required_path IN ITEMS
        "${ANDROID_LLVM_BIN}/${ANDROID_TARGET}-clang"
        "${ANDROID_LLVM_BIN}/${ANDROID_TARGET}-clang++"
        "${SDL3_INCLUDE_DIR}/SDL3/SDL.h"
        "${SDL3_LIBRARY_DIR}/libSDL3.so")
    if(NOT EXISTS "${required_path}")
        message(FATAL_ERROR "Required Android DXVK input is missing: ${required_path}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}/pkgconfig")
configure_file(meson-arm64-android-cross.ini.in "${OUTPUT_DIRECTORY}/meson-arm64-android-cross.ini" @ONLY)
configure_file(SDL3.pc.in "${OUTPUT_DIRECTORY}/pkgconfig/SDL3.pc" @ONLY)

