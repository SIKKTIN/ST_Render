# 故障排查 / Troubleshooting

本文档按"症状 → 排查 → 解决"的形式整理构建相关的常见错误。

---

## 1. CMake 配置阶段

### 1.1 `Generator Visual Studio 17 2022 could not find specified instance of Visual Studio`

**完整报错**：
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
- CMake 4.x 的 VS generator 通过 `vswhere.exe`（路径 `D:\IDE\Vis2022\Installer\vswhere.exe`）+ 注册表读取 VS 实例元数据
- 本机安装的是 **Build Tools**（不是通过完整 VS Installer），`vswhere.exe` 不在默认路径
- 即便 `CMAKE_GENERATOR_INSTANCE` 显式指定了路径，CMake 也找不到 `installationVersion`

**解决**：换用 `NMake Makefiles` generator：
```cmd
cmake -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

**另一种思路**：用完整 VS Installer 重新安装 Community/Professional（不推荐，浪费磁盘空间）。

### 1.2 `generator ... Does not match the generator used previously`

**症状**：第一次用 VS generator 配过，第二次换 NMake 报错。

**解决**：
```cmd
rmdir /s /q build
cmake -S . -B build -G "NMake Makefiles" ...
```

### 1.3 `find_package(SDL2) failed`

**症状**：
```
Could not find a package configuration file provided by "SDL2" with any of
the following names:
  SDL2Config.cmake
```

**排查步骤**：
1. 确认 vcpkg 安装目录：`C:\vcpkg\installed\x64-windows\share\SDL2\` 应有 `SDL2Config.cmake`
2. 确认 configure 命令带了 toolchain：
   ```cmd
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ...
   ```
3. 或在 `CMakePresets.json` 里 inherit `vcpkg` 预设（项目已经这样做了）
4. 重新装依赖：`C:\vcpkg\vcpkg install sdl2:x64-windows sdl2-mixer:x64-windows imgui:x64-windows nlohmann-json:x64-windows`

### 1.4 `Could NOT find SDL2_mixer (missing: ...)` / `imgui` / `nlohmann_json`

**通用排查**：
- 确认 vcpkg 已装：`C:\vcpkg\vcpkg list | findstr <name>`
- 装包：`C:\vcpkg\vcpkg install <pkg>:x64-windows`
- 装完后**必须**重新 configure，不能只 build

---

## 2. 编译阶段

### 2.1 `cl.exe 不是内部或外部命令`

**症状**：直接 `cmake --build build` 报 cl.exe 找不到

**根因**：没激活 MSVC 环境，vcvars64.bat 没在 PATH 加 `cl.exe`

**解决**：每次开新终端先跑：
```cmd
call "D:\IDE\Vis2022\VC\Auxiliary\Build\vcvars64.bat"
```

或用提供的脚本 `scripts/build.bat`，脚本开头已自动 call。

### 2.2 `NMAKE : fatal error U1065: 无效的选项 'M'`

**症状**：
```cmd
cmake --build build -- /M:4
```

**根因**：`/M:4` 是 MSBuild 语法，NMake 不认

**解决**：
```cmd
cmake --build build --parallel 4
```

### 2.3 `fatal error C1034: ...: 不包括 SDK 头文件`

**症状**：编译时找不到 `windows.h` / `shobjidl.h` 等

**排查**：
1. 确认 `C:\Program Files (x86)\Windows Kits\10\Include\<version>\` 存在
2. 确认 vcvars64.bat 被 call 了（INCLUDE / LIB 环境变量）
3. 在 cmd 里跑：
   ```cmd
   call vcvars64.bat
   echo %INCLUDE%
   ```
   应输出 Windows Kits + MSVC 的 include 路径

### 2.4 `error C2065: 'changed': 未声明的标识符`

**症状**：某次改 ImGui 控制面板代码时漏声明变量

**解决**：在函数开头声明 `bool changed = false;`，后面用 `changed |= ...` 累加。

### 2.5 `error C3668: 'Xxx::onKeyUp': 包含重写说明符 'override' 的方法没有重写任何基类方法`

**症状**：在派生类 override 了基类 `ITestModule` 没声明的虚函数

**解决**：在基类声明对应 `virtual void onKeyUp(int) {}`。

### 2.6 `warning C4819: 该文件包含不能在当前代码页(936)中表示的字符`

**症状**：源文件是 UTF-8 BOM，但 MSVC 默认按系统代码页（中文 936）解析

**影响**：**仅警告**，不影响编译。中文注释会出现乱码但能编过。

**可选优化**（不影响功能）：
- 在 `CMakeLists.txt` 加 `add_compile_options(/utf-8)` 让 cl 把源文件当 UTF-8 读
- 或在每个文件首行加 `#pragma execution_character_set("utf-8")`（旧 MSVC）
- 推荐前者，一劳永逸

---

## 3. 链接阶段

### 3.1 `unresolved external symbol "symbol"`

**症状**：链接时报 `unresolved external symbol SDL_xxx` / `ImGui_xxx`

**排查**：
1. 确认 CMakeLists.txt 中 `target_link_libraries(ST_Render_Manager PRIVATE SDL2::SDL2 SDL2_mixer::SDL2_mixer imgui::imgui ...)` 配齐
2. 重新 configure（改 CMakeLists.txt 后必须）

### 3.2 `LNK1181: cannot open input file 'SDL2.lib'`

**根因**：vcpkg 没装，或 triplet 不对

**解决**：
```cmd
C:\vcpkg\vcpkg install sdl2:x64-windows
```

---

## 4. 运行阶段

### 4.1 启动时弹窗 "缺少 SDL2.dll"

**解决**：从 `C:\vcpkg\installed\x64-windows\bin\` 拷贝到 `build/bin/`
- 或确认 `add_custom_command(POST_BUILD ...)` 自动 copy 配置正确

### 4.2 中文路径下运行异常

**已知问题**：SDL2 在某些 Windows 配置下对中文路径支持不全。
**建议**：保持项目在纯英文路径（如 `E:\Project\C++\ST_Render`）。

---

## 5. IDE 集成

### 5.1 想在 Visual Studio 中打开项目

CMake 生成的是 `Makefile`（NMake），**不是** `.sln`。

**方案 A**：用 VS 的 "Open Folder" 直接打开仓库根目录，VS 会调 CMake。

**方案 B**：先临时生成 .sln：
```cmd
cmake -S . -B build_sln -G "Visual Studio 17 2022" ^
  -DCMAKE_GENERATOR_INSTANCE=D:/IDE/Vis2022 ^
  -DCMAKE_GENERATOR_VERSION=<installationVersion>
```
> ⚠️ 因为没 vswhere，需要从 `vswhere.exe`（如能找到）读 `installationVersion`，或用 VS 2022 的版本号（17.x）。

### 5.2 VSCode

在 `.vscode/settings.json`：
```json
{
  "cmake.configureSettings": {
    "CMAKE_TOOLCHAIN_FILE": "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
  },
  "cmake.generator": "NMake Makefiles"
}
```

`.vscode/launch.json`：
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "ST_Render_Manager",
      "type": "cppvsdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/bin/ST_Render_Manager.exe",
      "args": [],
      "stopAtEntry": false,
      "cwd": "${workspaceFolder}/build/bin",
      "environment": [],
      "console": "integratedTerminal"
    }
  ]
}
```

> ⚠️ `cppvsdbg` 需安装 MSVC 调试器，或改用 `cppdbg`（MinGW）或 `lldb`。

---

## 6. 性能调优

### 6.1 Release 构建

```cmd
cmake -S . -B build_rel --preset release
cmake --build build_rel --parallel 4
```

注意 release preset 输出仍在 `build/`，与 debug 共用 binaryDir，**需要先 rmdir**。

### 6.2 Unity Build（编译加速）

可在 `CMakeLists.txt` 加：
```cmake
set(CMAKE_UNITY_BUILD ON)
set(CMAKE_UNITY_BUILD_BATCH_SIZE 16)
```
大项目可显著减少编译时间。

### 6.3 PCH（预编译头）

对小项目收益有限，当前未启用。如需：
```cmake
target_precompile_headers(ST_Render_Manager PRIVATE
  <SDL2/SDL.h>
  <imgui/imgui.h>
  ...
)
```

---

## 7. 重新生成 build 目录的"万能公式"

```cmd
rmdir /s /q build
call "D:\IDE\Vis2022\VC\Auxiliary\Build\vcvars64.bat"
cmake -S . -B build --preset debug
cmake --build build --target ST_Render_Manager --parallel 4
```

任何诡异问题先跑一遍这条。