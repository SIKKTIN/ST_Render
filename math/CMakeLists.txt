# Math/CMakeLists.txt
# 纯头文件数学库

cmake_minimum_required(VERSION 3.16)

# 创建 INTERFACE 库（纯头文件，不需要编译）
add_library(Math INTERFACE)

# 设置公共包含目录
target_include_directories(Math INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
