# 构建指南 / Build Guide

本文档说明 **ST_Render** 项目的构建流程，包括工具链选型、一次性配置和日常编译命令。

---

## 1. 快速开始

### 1.1 一键脚本（推荐）

仓库根目录提供两个 Windows 批处理脚本，可直接双击或从 PowerShell/cmd 调用：

| 脚本 | 作用 |
|---|---|
| `scripts/configure.bat` | 配置 build 目录（首次运行或切配置时执行） |
| `scripts/build.bat`     | 增量编译可执行文件 `ST_Render_Manager.exe` |

```cmd
:: 首次：配置 Debug 版本
scripts\configure.bat

:: 增量编译
scripts\build.bat
```

可执行文件位置：
```
E:\Project\C++\ST_Render\build\bin\ST_Render_Manager.exe
```

### 1.2 直接调 cmake

如果不想用脚本，可手动执行：

```cmd
:: 1) 激活 MSVC 环境
call "D:\IDE\Vis2022\VC\Auxiliary\Build\vcvars64.bat"

:: 2) 配置
cmake -S . -B build --preset debug

:: 3) 编译
cmake --build build --target ST_Render_Manager --parallel 4
```

> ⚠️ 当前 generator 是 `NMake Makefiles`，**会忽略** `--parallel N` 选项（NMake 不支持原生并行）。如要真正并行编译，请先安装 Ninja 并切 generator，详见 [BUILD_TOOLS_REFERENCE.md §10](./BUILD_TOOLS_REFERENCE.md#10-升级指引)。

---

## 2. 工具链

### 2.1 编译器：MSVC（Visual Studio 2022 Build Tools 17.x）

- **安装路径**：`D:\IDE\Vis2022\`
- **MSVC 版本**：14.44.35207（VS17 的 C++ 编译器，遵循 MSVC `v14.x` 命名）
- **环境激活脚本**：`D:\IDE\Vis2022\VC\Auxiliary\Build\vcvars64.bat`
- **激活方式**：所有 build 命令前**必须**先 `call vcvars64.bat`，否则 `cl.exe` / `link.exe` 不在 PATH 中
- **注意**：本机安装的是 **VS Build Tools**（无 IDE、无 Installer 注册表项），见 §4 排错

### 2.2 包管理器：vcpkg

- **路径**：`C:\vcpkg\`
- **target triplet**：`x64-windows`
- **已安装依赖**（自动在 CMake `find_package` 中被找到）：
  - SDL2（含 main / image / ttf 等子库）
  - SDL2_mixer
  - imgui（带 SDL2 + SDL_Renderer 绑定）
  - nlohmann_json
- **CMake 集成**：根 `CMakeLists.txt` 第 13 行已设置
  ```cmake
  set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
  ```
- **要装新包**：`C:\vcpkg\vcpkg install <pkg>:x64-windows`（管理员权限），然后 `vcpkg integrate install`

### 2.3 构建系统：CMake + NMake

- **CMake 版本**：≥ 3.16（项目最低要求 3.16，本机使用 4.3.2）
- **Generator 选择**：`NMake Makefiles`（**不是** `Visual Studio 17 2022`）
  - 原因见 §4.1
- **NMake**：MSVC 自带，被 vcvars64.bat 加入 PATH
- **并行编译**：通过 `cmake --build --parallel N` 触发 CMake 的 JOB_SERVER，NMake 实际是单进程递归调度，但单次 cl 调用内部多线程

### 2.4 系统 SDK：Windows 10/11 SDK 10.0.26100

- 路径：`C:\Program Files (x86)\Windows Kits\10\`
- vcvars64.bat 自动配置 INCLUDE / LIB 指向此 SDK
- 项目用到 `windows.h`、`shobjidl.h`（IFileDialog，TestModule_2D_Scene 的 COM 文件对话框）

---

## 3. CMake Presets

仓库根目录已有 `CMakePresets.json`，提供两个预设：

| 预设名 | 类型 | 说明 |
|---|---|---|
| `vcpkg` | configurePreset (hidden) | 继承工具链配置，不直接使用 |
| `debug` | configurePreset + buildPreset | Debug 构建，输出 `build/` |
| `release` | configurePreset + buildPreset | RelWithDebInfo，输出 `build/` |

> ⚠️ `CMakePresets.json` **不显式指定 generator**，原因详见 §4.1。如要显式控制，可在 preset 的 `generator` 字段写 `"NMake"`。

### 用 preset 编译

```cmd
cmake --preset debug
cmake --build --preset debug --parallel 4
```

### 切换到 Release

```cmd
cmake --preset release
cmake --build --preset release --parallel 4
```

> ⚠️ **debug 和 release 共用同一 `binaryDir`**，切换预设前必须先清空 build 目录（详见 §5）。

---

## 4. 常见问题

### 4.1 `could not find specified instance of Visual Studio`

**症状**：
```
CMake Error at CMakeLists.txt:5 (project):
  Generator
    Visual Studio 17 2022
  could not find specified instance of Visual Studio:
    D:/IDE/Vis2022
  The directory exists, but the instance is not known to the Visual Studio
  Installer, and no 'version=' field was given.
```

**根因**：
- 本机 VS 是 **Build Tools**（轻量安装），不是通过 VS Installer 安装的 Community/Professional/Enterprise
- CMake 4.x 的 VS generator 通过 `vswhere.exe` + 注册表读取 `installationVersion`（如 `17.11.35208.52`）
- 路径 `D:\IDE\Vis2022\Installer\vswhere.exe` 不存在，CMake 找不到任何 VS 实例

**解决**：使用 `NMake Makefiles` generator，**它不依赖 vswhere**，直接用 `cl.exe` 编译器。

### 4.2 中文路径/参数乱码

**症状**：PowerShell 调 `cmd /c "..."` 时引号被吃掉，或中文字符在终端输出乱码

**解决**：
1. **不要**在 PowerShell 里用 `cmd /c "..."` 嵌套 cmake
2. 改成写 `.bat` 文件，再 `cmd /c xxx.bat` 或直接在 cmd.exe 里运行
3. 终端编码：`chcp 65001` 切 UTF-8，或保持 cmd 默认 GBK

### 4.3 切换配置后 `Generator ... Does not match`

**症状**：
```
CMake Error: Error: generator : NMake Makefiles
Does not match the generator used previously: Visual Studio 17 2022
Either remove the CMakeCache.txt file and CMakeFiles directory or choose a different binary directory.
```

**解决**：清空 build 目录后重新 configure：
```cmd
rmdir /s /q build
cmake -S . -B build --preset debug
```

### 4.4 `find_package(SDL2)` 失败

**症状**：
```
Could not find a package configuration file provided by "SDL2"
```

**排查**：
1. 确认 `C:\vcpkg\installed\x64-windows\share\SDL2\SDL2Config.cmake` 存在
2. 确认 configure 时传了 `-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake`
3. 或确认 `CMakePresets.json` 的 `vcpkg` 预设被 inherit

### 4.5 `NMAKE : fatal error U1065: 无效的选项 'M'`

**症状**：用 `cmake --build build -- /M:4` 透传参数给 NMake

**根因**：NMake 不支持 `/M:N`（那是 MSBuild 的语法）

**解决**：用 cmake 自带的并行选项：
```cmd
cmake --build build --parallel 4
```

---

## 5. 清理 build

### 完全清理
```cmd
rmdir /s /q build
```

### 切换 Debug ↔ Release
```cmd
rmdir /s /q build
cmake --preset release   :: 或 debug
cmake --build --preset release --parallel 4
```

### 只清理某个 target
```cmd
cmake --build build --target clean
```
（clean 是 NMake generator 自带的目标）

---

## 6. 输出位置

```
build/
├── bin/
│   └── ST_Render_Manager.exe       # 主可执行文件
├── lib/                            # 各 .lib 静态库
├── CMakeFiles/                     # cmake 内部文件
├── CMakeCache.txt                  # configure 缓存（删 build 即清空）
└── Makefile                        # NMake 的入口
```

运行 ST_Render_Manager.exe 需保留同级目录下的：
- `SDL2.dll`（vcpkg 自动 copy）
- `SDL2_mixer.dll`
- `mpg123.dll`
- `Data/`（资源目录）
- `Resource/`（字体等）

cmake 的 `add_custom_command(POST_BUILD ...)` 已经自动 copy，运行前确认 `build/bin/` 下文件齐全即可。

---

## 7. 开发流程建议

### Include 风格约定

项目混用了三种 include 风格，按"作用域"区分使用：

| 风格 | 适用场景 | 例 |
|---|---|---|
| `#include <...>` | 仅用于**系统 / 第三方库**头文件 | `<SDL2/SDL.h>`, `<imgui.h>`, `<vector>` |
| `#include "..."` 同目录或隐式 | 同一 CMake target 内部头文件 | `#include "Vector3.hpp"`（Pipeline 内） |
| `#include "../Xxx/Yyy.hpp"` 相对路径 | **跨模块**引用 | `#include "../Math/Vector3.hpp"` |

⚠️ **不要**用 `<...>` 引用项目内头文件（如 `<Vector4.hpp>`），会与系统头文件混淆。

> 如果以后想统一，可以把所有"短名 include"改为相对路径，全局搜替换即可。

### 日常开发

1. 第一次拉代码：跑 `scripts/configure.bat`
2. 日常：改完代码跑 `scripts/build.bat`
3. 新加 .cpp/.hpp：修改对应 `CMakeLists.txt` 的 `target_sources(...)` 或 `add_executable(...)` 中的文件列表，重新跑 configure（即再跑一次 configure.bat）
4. 新加 vcpkg 依赖：`vcpkg install xxx:x64-windows` 后重跑 configure
5. Debug：在 Visual Studio Code / Visual Studio 中打开 `build/ST_Render.sln`（如使用 VS generator），或在 VSCode 中配置 `launch.json` 直接指向 `build/bin/ST_Render_Manager.exe`

---

## 8. 相关文档

- [TROUBLESHOOTING.md](./TROUBLESHOOTING.md) — 更多错误排查
- [BUILD_TOOLS_REFERENCE.md](./BUILD_TOOLS_REFERENCE.md) — 工具链清单与版本