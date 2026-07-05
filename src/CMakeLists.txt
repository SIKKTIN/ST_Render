# src/CMakeLists.txt - ST_Render 源码根
#
# 目录约定:
#   src/core/       - 跨模块基础（数学、相机、数据、纹理）
#   src/renderer/   - 软件渲染管线
#   src/engine/     - 编辑器运行时（GameObject、Scene、UI...）
#   src/modules/    - 独立功能模块（音乐、网络...）
#   src/app/        - 主程序入口 + 测试

# 为所有 src/ 子目录暴露公共 include 路径（"core/math/Vector3.hpp"）
include_directories(${CMAKE_SOURCE_DIR}/src)

add_subdirectory(core)
add_subdirectory(renderer)
add_subdirectory(engine)
add_subdirectory(modules)
add_subdirectory(app)