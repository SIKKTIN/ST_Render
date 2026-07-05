# src/renderer/buffer/CMakeLists.txt
# 帧缓冲和深度缓冲
add_library(Buffer INTERFACE)
target_include_directories(Buffer INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)
target_link_libraries(Buffer INTERFACE Math Texture)