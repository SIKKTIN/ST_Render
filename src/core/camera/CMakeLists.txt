# src/core/camera/CMakeLists.txt
# 相机：视图矩阵和投影矩阵
add_library(Camera INTERFACE)
target_include_directories(Camera INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
target_link_libraries(Camera INTERFACE Math)