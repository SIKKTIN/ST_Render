# src/renderer/geometry/CMakeLists.txt
# 几何图元：顶点、三角形、网格
add_library(Geometry Mesh.cpp)
target_include_directories(Geometry PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
target_link_libraries(Geometry PUBLIC Math)