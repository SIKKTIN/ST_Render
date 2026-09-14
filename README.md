# ST_Render

ST_Render 是一个以 CPU 软件管线为核心的 C++ 渲染器实验项目。项目重点是几何变换、三角形光栅化、深度测试、纹理采样、材质和 PBR 光照，并提供一个基于 SDL2 + Dear ImGui 的交互式测试管理器。

## 当前能力

- CPU 端顶点处理、裁剪、透视插值、三角形光栅化和深度缓冲。
- OBJ、FBX 模型加载；FBX 使用 Assimp。
- BaseColor、Roughness、Metallic、Normal 贴图绑定。
- GGX PBR、环境光照、环境贴图过滤和色调映射。
- 场景 Prefab 的加载与保存。
- 3D Render、Texture、Shader、FrameBuffer、Rasterizer 和 Math Review 页面。
- `ST_Render_Dump` 无窗口诊断程序，用于批量检查 3D 渲染结果。

项目只围绕渲染器及其调试工具展开，音频、网络和音乐功能不属于项目范围。

## 目录结构

| 路径 | 内容 |
| --- | --- |
| `src/core` | 数学、相机和纹理等基础模块 |
| `src/renderer` | 几何、资源加载、着色器、光栅化和渲染器 |
| `src/engine` | 场景对象、场景序列化、编辑器数据和辅助 UI |
| `src/app/main` | SDL2/ImGui 测试管理器与离屏诊断入口 |
| `Data/Models` | OBJ 模型和模型纹理 |
| `Data/M1911` | M1911 FBX 模型及 PBR 贴图 |
| `Data/ScenePrefab` | 可加载的场景文件 |
| `Data/Shaders` | 脚本着色器示例 |
| `Docs` | 构建、架构和问题排查文档 |

## 环境要求

- Windows 10/11 x64
- Visual Studio 2022，包含“使用 C++ 的桌面开发”和 Windows SDK
- CMake 3.21 或更新版本
- vcpkg x64-windows：`SDL2`、`imgui`、`nlohmann-json`、`assimp`

默认预设使用 `C:/vcpkg/scripts/buildsystems/vcpkg.cmake`。如果 vcpkg 安装在其他位置，请修改 `CMakePresets.json` 中的工具链路径。

## 构建

在仓库根目录执行：

```powershell
cmake --preset debug
cmake --build --preset debug --parallel 4
```

也可以使用脚本：

```cmd
scripts\build.bat
scripts\build.bat release
```

Debug 构建输出到 `build/debug/bin/Debug`，Release 构建输出到 `build/release/bin/RelWithDebInfo`。构建过程会自动复制运行所需的 `Resource`、`Data` 和 SDL2 DLL。

## 运行

启动交互式测试管理器：

```powershell
& .\build\debug\bin\Debug\ST_Render_Manager.exe
```

启动无窗口诊断程序：

```powershell
& .\build\debug\bin\Debug\ST_Render_Dump.exe --sweep
```

在 3D Render 页面中，按住鼠标右键拖动可观察场景；按住右键并使用 `W/A/S/D` 移动，`Q/E` 上下移动。场景可以从 `Data/ScenePrefab` 加载，M1911 示例场景为 `m1911_pbr.scene.json`。

## 文档

- [构建与启动](Docs/BUILD.md)
- [常见问题排查](Docs/TROUBLESHOOTING.md)
- [项目架构](Docs/architecture/ST_Render架构.md)
- [渲染器设计计划](Docs/plan/SOFTWARE_RENDERER_PLAN.md)
- [M1911 资源说明](Data/M1911/README.md)

## 开发提示

渲染计算在 CPU 上执行，复杂模型和高分辨率贴图会增加光栅化耗时。M1911 的原始 2048px PBR 贴图在导入时会缩小为 512px 工作副本，原始资源保持不变。
