# src/renderer/pipeline/CMakeLists.txt
# 渲染管线：顶点着色器、片段着色器、光栅化器
add_library(Pipeline FragmentShader.cpp)
target_include_directories(Pipeline PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
target_link_libraries(Pipeline PUBLIC Math Buffer Geometry)