# 构建工具链参考 / Build Tools Reference

本文档详细列出 ST_Render 项目的所有构建相关工具及其版本、路径、作用。

---

## 1. 工具链一览

| 工具 | 版本 | 安装路径 | 角色 |
|---|---|---|---|
| Visual Studio Build Tools | 17.x | `D:\IDE\Vis2022\` | 提供 MSVC 编译器 |
| MSVC（cl.exe / link.exe） | 14.44.35207 | `D:\IDE\Vis2022\VC\Tools\MSVC\14.44.35207\` | C++17 编译器 |
| Windows SDK | 10.0.26100 | `C:\Program Files (x86)\Windows Kits\10\` | 系统头文件 / 库 |
| CMake | 4.3.2 | 系统 PATH | 构建系统生成器 |
| NMake | MSVC 自带 | MSVC bin 目录 | Make 工具 |
| vcpkg | latest | `C:\vcpkg\` | C++ 包管理器 |
| Ninja（未启用）| — | — | 可选高速 make |

---

## 2. Visual Studio Build Tools 17.x

### 安装路径
```
D:\IDE\Vis2022\
├── VC\                    MSVC 编译器 + 工具
├── MSBuild\               MSBuild 引擎
├── Installer\             ⚠️ 本机不存在（不是标准 Installer 安装）
├── Common7\               IDE 共享组件（devenv.exe 等，本机可能没有）
└── VC\Auxiliary\Build\    vcvars*.bat 环境激活脚本
```

### vcvars 脚本

| 脚本 | 用途 |
|---|---|
| `vcvars32.bat` | 32 位 x86 环境 |
| `vcvars64.bat` | 64 位 x64 环境（**本项目用这个**） |
| `vcvarsall.bat` | 多架构（参数：`x64` / `x86` / `arm64`） |
| `vcvarsx86_amd64.bat` | 交叉编译：32 位 host 编 64 位 |
| `vcvarsamd64_x86.bat` | 交叉编译：64 位 host 编 32 位 |

### 本机 MSVC 编译器详情

```
D:\IDE\Vis2022\VC\Tools\MSVC\14.44.35207\
├── bin\Hostx64\x64\cl.exe           C++ 编译器
├── bin\Hostx64\x64\link.exe         链接器
├── bin\Hostx64\x64\lib.exe          静态库工具
├── include\                         C/C++ 头文件（CRT、STL、intrinsics）
├── lib\x64\                         链接时用的 .lib
└── ...
```

> `14.44.35207` 是 MSVC 内部的工具集版本号，对应 VS17 的某个更新（VS 17.x 的 update 版次不同，工具版本会变）。
> 本机这是 **VS 17.10 或 17.11 阶段** 的工具集（BuildTools 22H2 系列）。

---

## 3. CMake

- **版本**：4.3.2（系统安装，可在 cmd 任意目录调 `cmake`）
- **最低要求**：3.16（项目 `cmake_minimum_required(VERSION 3.16)`）
- **关键能力**：
  - 支持 `CMakePresets.json`（v3）
  - 支持 JOB_SERVER 并行（v3.12+）
  - 支持 Unity Build（v3.16+）

### 常用命令

```cmd
cmake -S <src> -B <build>                  :: 配置
cmake --build <build>                      :: 编译
cmake --build <build> --target <tgt>       :: 编译单个 target
cmake --build <build> --parallel 4         :: 并行
cmake --build <build> --target clean      :: 清理
cmake --preset <name>                      :: 用 preset 配置
cmake --build --preset <name>              :: 用 preset 编译
```

---

## 4. NMake

- **类型**：make 工具（Makefile 解释器）
- **来自**：MSVC 自带，被 `vcvars64.bat` 加到 PATH
- **Generator 名**：`NMake Makefiles`
- **特点**：
  - 单进程递归调度（不支持原生并行 make）
  - 通过 cmake JOB_SERVER 实现并行
  - 支持多文件并行编译单个 cl 调用
  - 不会生成 .sln / .vcxproj

### NMake vs Ninja vs MSBuild

| 特性 | NMake | Ninja | MSBuild |
|---|---|---|---|
| 速度 | 中（单进程调度）| 快（专为速度设计）| 慢 |
| 并行 | 通过 cmake | 原生支持 | 原生 `/m:N` |
| IDE 集成 | ❌ | VS / VSCode | VS 原生 |
| 易安装 | ✅（MSVC 自带）| 需单独装 | ✅（VS 自带） |
| 依赖文件追踪 | ✅ | ✅（增量更快）| ✅ |

**未来建议**：装 Ninja 提速。
```cmd
C:\vcpkg\vcpkg install ninja:x64-windows
```
然后 configure 加 `-G Ninja` 即可。

---

## 5. vcpkg

- **路径**：`C:\vcpkg\`
- **入口**：`vcpkg.exe`
- **Triplet**：`x64-windows`（默认）
- **集成方式**：`vcpkg integrate install`（已集成）

### 已装包

| 包 | 用途 | 在项目中的引用 |
|---|---|---|
| `sdl2` | 窗口 / 输入 / 渲染后端 | `find_package(SDL2)`、`target_link_libraries(... SDL2::SDL2)` |
| `sdl2-mixer` | 音频播放（MP3 / OGG） | `find_package(SDL2_mixer)` |
| `imgui` | 即时模式 GUI | `find_package(imgui)` |
| `nlohmann-json` | JSON 序列化 | `find_package(nlohmann_json)` |

### 包安装位置

```
C:\vcpkg\installed\x64-windows\
├── include\        头文件
├── lib\            .lib
├── bin\            .dll（运行时需拷贝到可执行文件目录）
├── share\<pkg>\    CMake config 文件
└── ...
```

### 装新包

```cmd
C:\vcpkg\vcpkg install <pkg>:x64-windows
```

如项目需要，请同步修改根 `CMakeLists.txt` 的 `find_package(...)` 和 `target_link_libraries(...)`。

---

## 6. Windows SDK

- **版本**：10.0.26100.0
- **位置**：`C:\Program Files (x86)\Windows Kits\10\`

```
Windows Kits\10\
├── Include\10.0.26100.0\
│   ├── ucrt\        C 标准库
│   ├── um\          Windows User-Mode API（windows.h, shobjidl.h, etc.）
│   ├── shared\      UM/KM 共享头
│   └── winrt\       WinRT 头
├── Lib\10.0.26100.0\
│   ├── ucrt\x64\
│   └── um\x64\
└── ...
```

vcvars64.bat 自动配置：
- `INCLUDE` += `Windows Kits\10\Include\10.0.26100.0\...`
- `LIB` += `Windows Kits\10\Lib\10.0.26100.0\um\x64`, `ucrt\x64`

---

## 7. 项目结构（与构建相关）

```
ST_Render/
├── CMakeLists.txt                  根配置（find_package、add_subdirectory）
├── CMakePresets.json               Preset 配置（debug / release）
├── .gitignore
├── buffer\CMakeLists.txt
├── Math\CMakeLists.txt
├── Texture\CMakeLists.txt
├── Geometry\CMakeLists.txt
├── Transform\CMakeLists.txt
├── Camera\CMakeLists.txt
├── Pipeline\CMakeLists.txt
├── Data\CMakeLists.txt
├── Data\DataBase\CMakeLists.txt
├── FucModule\Music\CMakeLists.txt
├── Renderer\CMakeLists.txt
├── Editor\CMakeLists.txt
├── Main\CMakeLists.txt
├── Docs\                           本目录
│   ├── BUILD.md
│   ├── TROUBLESHOOTING.md
│   └── BUILD_TOOLS_REFERENCE.md
├── scripts\                        构建脚本（建议新建）
│   ├── configure.bat
│   └── build.bat
└── build\                          构建产物（不入 git）
    ├── bin\ST_Render_Manager.exe
    ├── lib\*.lib
    ├── CMakeFiles\
    ├── CMakeCache.txt
    └── Makefile
```

---

## 8. 版本对应关系

### VS ↔ MSVC ↔ cl.exe

| VS 大版本 | MSVC 工具集版本 | cl.exe 报告版本 | 年份 |
|---|---|---|---|
| VS 2017 | 14.1x | 19.1x | 2017 |
| VS 2019 | 14.2x | 19.2x | 2019 |
| VS 2022 | 14.3x → 14.4x | 19.3x → 19.4x | 2022 起 |
| **本机** | **14.44.35207** | **19.44.x** | **VS 2022 (17.10~17.11 阶段)** |

### vcpkg ↔ Visual Studio

| vcpkg baseline | 推荐 VS |
|---|---|
| 2024.x | VS 2022 17.x |
| 2025.x | VS 2022 17.10+ |

本项目无 baseline 约束（根 CMakeLists 未指定 `VCPKG_INSTALLED_DIR` 或 baseline），用 vcpkg 默认 latest。

---

## 9. 编译命令缓存对照

| 想做的 | 命令 |
|---|---|
| 配置 + 编译（一次性）| `scripts/configure.bat && scripts/build.bat` |
| 仅配置 | `cmake -S . -B build --preset debug` |
| 仅编译 | `cmake --build build --parallel 4` |
| 编译单个文件 | `cmake --build build --target Editor`（依赖）|
| 链接主程序 | `cmake --build build --target ST_Render_Manager` |
| 清理 | `rmdir /s /q build` 或 `cmake --build build --target clean` |
| Release | `rmdir /s /q build && cmake --preset release && cmake --build --preset release --parallel 4` |

---

## 10. 升级指引

### 升级 MSVC
1. 跑 VS Installer（或 winget）装新版
2. 不需要改 `vcvars64.bat` 路径（脚本按相对路径找最新版）
3. 重跑 configure 即可

### 升级 CMake
- 系统级升级或下个 portable 版，cmake 命令不挑路径
- 注意 `cmake_minimum_required(VERSION 3.16)` 保持兼容

### 升级 vcpkg
```cmd
cd C:\vcpkg
git pull
.\vcpkg upgrade
```
重跑项目 configure。

### 切换到 Ninja（推荐提速）
1. `C:\vcpkg\vcpkg install ninja:x64-windows`
2. 修改 `scripts/configure.bat`：
   ```bat
   cmake -S . -B build -G "Ninja" ...
   ```
3. 改用 `cmake --build build`（nmake 不再用，无需透传参数）

### 切换到 VS IDE 调试
参见 [TROUBLESHOOTING.md §5](./TROUBLESHOOTING.md#5-ide-集成)。

---

## 11. License / 引用

- **MSVC**：Microsoft Visual Studio License
- **vcpkg**：MIT License
- **CMake**：BSD 3-Clause
- **SDL2**：zlib License
- **SDL2_mixer**：zlib License
- **imgui**：MIT License
- **nlohmann_json**：MIT License