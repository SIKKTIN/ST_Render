# Geometry/CMakeLists.txt
# 几何图元：顶点、三角形、网格

cmake_minimum_required(VERSION 3.16)

add_library(Geometry Mesh.cpp)

target_include_directories(Geometry PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Geometry 依赖 Math
target_link_libraries(Geometry PUBLIC Math)
