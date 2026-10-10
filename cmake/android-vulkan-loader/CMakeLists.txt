cmake_minimum_required(VERSION 3.20)
project(GeneralsXAndroidVulkanLoader LANGUAGES CXX)
# GeneralsX @build Codex 09/10/2026 Optional app-local driver bridge, Android only.
add_library(generalsx_vulkan SHARED ../../android/app/src/main/cpp/GeneralsXVulkanLoader.cpp)
target_compile_features(generalsx_vulkan PRIVATE cxx_std_17)
target_link_libraries(generalsx_vulkan PRIVATE dl log)
