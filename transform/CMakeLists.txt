# Transform/CMakeLists.txt
# 变换：位置、旋转、缩放

cmake_minimum_required(VERSION 3.16)

add_library(Transform INTERFACE)

target_include_directories(Transform INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Transform 依赖 Math
target_link_libraries(Transform INTERFACE Math)
