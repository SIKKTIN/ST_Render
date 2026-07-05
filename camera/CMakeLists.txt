# Camera/CMakeLists.txt
# 相机：视图矩阵和投影矩阵

cmake_minimum_required(VERSION 3.16)

add_library(Camera INTERFACE)

target_include_directories(Camera INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Camera 依赖 Math
target_link_libraries(Camera INTERFACE Math)
