# CMakeLists.txt - ST_Render 软件渲染器项目
cmake_minimum_required(VERSION 3.16)

# 项目配置
project(ST_Render VERSION 0.2.0 LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# vcpkg 集成
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain")

# SDL2 依赖
find_package(SDL2 CONFIG REQUIRED)

# SDL2_mixer 依赖
find_package(SDL2_mixer CONFIG REQUIRED)

# ImGui 依赖
find_package(imgui CONFIG REQUIRED)

# nlohmann/json 依赖
find_package(nlohmann_json CONFIG REQUIRED)

# 输出目录设置
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# src/ 根作为公共 include 起点（所有 src 下的目标都可以 #include "core/math/Vector3.hpp"）
add_subdirectory(src)

# 打印配置信息（可选）
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")