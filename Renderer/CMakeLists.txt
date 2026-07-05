# Renderer/CMakeLists.txt
# 渲染器：整合所有模块进行渲染

cmake_minimum_required(VERSION 3.16)

add_library(Renderer Renderer.cpp)

target_include_directories(Renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Renderer 依赖其他核心模块
target_link_libraries(Renderer PUBLIC Buffer Geometry Transform Camera Pipeline)
