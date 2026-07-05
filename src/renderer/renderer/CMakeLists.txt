# src/renderer/renderer/CMakeLists.txt
# 渲染器：整合所有模块进行渲染
add_library(Renderer Renderer.cpp)
target_include_directories(Renderer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
target_link_libraries(Renderer PUBLIC Buffer Geometry Transform Camera Pipeline)