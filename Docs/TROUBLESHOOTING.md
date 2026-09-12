# 构建与启动排错

## 找不到依赖

确认 CMakePresets.json 的 vcpkg 工具链路径存在，SDL2、imgui、nlohmann-json 已安装为 x64-windows，并使用预设配置工程。不要混用 x86 与 x64。

## 生成器不匹配

旧 build/ 可能使用 NMake。直接运行 cmake --preset debug，使用新的 build/debug 目录。不要覆盖为 -B build。Debug 和 release 各有自己的缓存。

## 找不到 Visual Studio 或 Windows SDK

在 Visual Studio Installer 中确认安装了“使用 C++ 的桌面开发”和 Windows SDK。当前预设需要 Visual Studio 2022；仅安装其他版本时需要相应修改生成器。

若 MSBuild 报用户目录下 Microsoft SDKs 的访问被拒绝，应检查执行环境的目录权限。这与 C++ 源码编译错误不同。

## 找不到 DLL 或启动即退出

从最新的 build/debug/bin/Debug 或 build/release/bin/RelWithDebInfo 运行程序。重新构建会复制 SDL2 对应配置的 DLL；Debug 使用 SDL2d.dll 时，不要替换成 Release 版本。

在终端中运行可执行文件查看标准输出与标准错误。SDL 初始化、窗口创建或渲染器创建失败时会输出错误。

## 纹理为空或场景资源缺失

程序旁应有 Resource/ 和 Data/ScenePrefab/。纹理管理器按可执行文件所在目录查找 Resource/。不要只复制 exe 而遗漏 DLL 和资源。

## 3D 渲染异常

先运行 ST_Render_Dump.exe --sweep，检查相机朝向变化时的像素统计；再查看 Review 中的数学、光栅化测试。历史问题分析见 [3D 剔除问题](rendering/3d-culling-bugs.md)。

构建命令见 [BUILD.md](BUILD.md)。
