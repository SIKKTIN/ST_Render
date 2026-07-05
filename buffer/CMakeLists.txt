# Buffer/CMakeLists.txt
# 帧缓冲和深度缓冲

cmake_minimum_required(VERSION 3.16)

add_library(Buffer INTERFACE)

target_include_directories(Buffer INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Buffer 依赖 Math 和 Texture
target_link_libraries(Buffer INTERFACE Math Texture)
