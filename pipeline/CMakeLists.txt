# Pipeline/CMakeLists.txt
# 渲染管线：顶点着色器、片段着色器、光栅化器

cmake_minimum_required(VERSION 3.16)

# VertexShader 和 Rasterizer 是纯头文件
# FragmentShader 有实际实现，需要编译
add_library(Pipeline FragmentShader.cpp)

target_include_directories(Pipeline PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接依赖
target_link_libraries(Pipeline PUBLIC Math Buffer Geometry)
