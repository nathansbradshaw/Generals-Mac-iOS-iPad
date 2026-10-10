cmake_minimum_required(VERSION 3.25)

project(GeneralsXAndroidBootstrap LANGUAGES CXX)

find_package(SDL3 REQUIRED CONFIG)

add_library(main SHARED
    AndroidBootstrap.cpp
)

target_compile_features(main PRIVATE cxx_std_17)
target_compile_definitions(main PRIVATE VK_USE_PLATFORM_ANDROID_KHR)
target_link_libraries(main PRIVATE SDL3::SDL3 android log vulkan)
