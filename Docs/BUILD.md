# 构建与启动

ST_Render 使用 C++17、CMake 和 Visual Studio 2022 的 x64 工具链。渲染计算运行在 CPU 上，SDL2 负责窗口、输入和最终显示。

## 前置条件

- 安装 Visual Studio 2022 的“使用 C++ 的桌面开发”和 Windows SDK。
- 安装 CMake 3.21 或更新版本（命令行预设所需；项目本身最低为 3.16）。
- 使用 vcpkg 的 x64-windows 包：SDL2、imgui、nlohmann-json。图像解码使用源码内的 stb_image。
- CMakePresets.json 默认工具链路径是 C:/vcpkg/scripts/buildsystems/vcpkg.cmake；安装在其他位置时修改该路径。

## 编译

在仓库根目录执行：

```powershell
cmake --preset debug
cmake --build --preset debug --parallel 4
```

或者运行：

```cmd
scripts\build.bat
```

脚本使用仓库相对路径，不依赖固定的工程目录或手动激活 MSVC。需要单独配置时运行 scripts\configure.bat。

Debug 和 RelWithDebInfo 使用独立缓存：

| 预设 | 目录 | 可执行文件 |
|---|---|---|
| debug | build/debug | build/debug/bin/Debug/ST_Render_Manager.exe |
| release | build/release | build/release/bin/RelWithDebInfo/ST_Render_Manager.exe |

构建 release：

```powershell
cmake --preset release
cmake --build --preset release --parallel 4
```

也可以运行 scripts\build.bat release。两个预设均使用 Visual Studio 17 2022 生成器，build preset 已设置实际编译配置。

## 启动与诊断

```powershell
& ./build/debug/bin/Debug/ST_Render_Manager.exe
& ./build/debug/bin/Debug/ST_Render_Dump.exe --sweep
```

Manager 提供帧缓冲、三角形光栅化、3D、纹理和 Review 测试页。退出时关闭窗口或按 Esc。

Dump 在无窗口环境中执行 3D 渲染，输出每个视角的像素统计。单帧导出示例：

```powershell
& ./build/debug/bin/Debug/ST_Render_Dump.exe --yaw=0.5 --out=build/cube.ppm
```

构建会将 Resource/ 和 Data/ScenePrefab/ 放在主程序旁边，并复制所需 SDL2 DLL（Debug 通常为 SDL2d.dll）。场景数据应保留，不能把整个 Data/ 当作无用资源删除。

## 旧缓存

旧的 build/ 根目录可能保存 NMake 缓存。新预设使用 build/debug 和 build/release，不再复用该缓存。不要给上述命令额外传入 -B build，否则会重新触发生成器冲突。

更多环境诊断见 [TROUBLESHOOTING.md](TROUBLESHOOTING.md)。
