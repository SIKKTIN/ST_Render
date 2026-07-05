# src/renderer/transform/CMakeLists.txt
# 变换：位置、旋转、缩放
add_library(Transform INTERFACE)
target_include_directories(Transform INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
target_link_libraries(Transform INTERFACE Math)