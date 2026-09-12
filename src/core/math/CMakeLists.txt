# src/core/math/CMakeLists.txt
# 纯头文件数学库
add_library(Math INTERFACE)
target_include_directories(Math INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)