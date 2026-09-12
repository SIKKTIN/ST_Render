# 构建工具参考

## 工具链

| 工具 | 用途 |
|---|---|
| Visual Studio 2022 / MSVC x64 | C++17 编译和 MSBuild |
| Windows SDK | Windows 平台头文件和库 |
| CMake | 配置工程和构建预设 |
| vcpkg | 提供 SDL2、imgui、nlohmann-json |

预设使用 Visual Studio 17 2022 生成器。CMake 自动定位已安装的 Visual Studio，不需要将 cl.exe 或 NMake 加入 PATH。

本机验证过的环境：MSVC 19.44、Windows SDK 10.0.26100.0、CMake 4.3.2。版本信息是验证记录，不是必须完全匹配的限制。

## 依赖

- SDL2：窗口、输入、纹理显示。
- Dear ImGui：测试面板，SDL2 和 SDLRenderer2 后端源码位于 src/app/main。
- nlohmann/json：场景保存和加载。
- stb_image：源码内的图像解码实现，位于 src/core/texture。

默认 vcpkg 工具链：C:/vcpkg/scripts/buildsystems/vcpkg.cmake。
目标 triplet：x64-windows。
工具链或 triplet 改变时，使用独立构建目录。

## CMake 目标

源码按 src/core、src/renderer、src/engine、src/app 组织。
构建目标包括 Math、Camera、Texture、Buffer、Geometry、Transform、Pipeline、Renderer、Editor、DataBase，以及 ST_Render_Manager、ST_Render_Dump。

Manager 链接编辑器和渲染库；Dump 仅使用渲染相关库。Editor 的静态脚本注册通过 /WHOLEARCHIVE 保留。

完整命令及输出路径见 [BUILD.md](BUILD.md)。第三方许可证以依赖包或内置源码的许可证为准。
