# ST_Render 软件架构与生命周期

> 主入口：`src/app/main/test_manager_main.cpp`
> 主可执行文件：`bin/ST_Render_Manager.exe`

ST_Render 是一个**纯 CPU 软件渲染演示引擎**，目标是把现代渲染管线的每个阶段在 C++ 里复现一遍（顶点变换 → 三角形光栅化 → 纹理采样 → 深度测试 → 后处理抗锯齿），并以 ImGui 编辑器承载多个独立测试模块。

---

## 一、技术栈

| 层级 | 技术 |
|---|---|
| 窗口 / 输入 / 音频 | SDL2（+ SDL2_mixer） |
| 编辑器 UI | Dear ImGui（SDL2 + SDLRenderer2 backend） |
| 图像加载 | stb_image |
| 音频解码 | SDL2_mixer（MP3 via mpg123） |
| 场景序列化 | nlohmann/json |
| 渲染 | 全 CPU 软件光栅化，SDL 仅承担最终像素上传 |

---

## 二、目录结构

```
ST_Render/
├── src/
│   ├── engine/                # 引擎库（静态库，被主程序链接）
│   │   ├── math/              # Vector2/3/4、Matrix4x4、Color
│   │   ├── buffer/            # FrameBuffer、DepthBuffer、Texture2D
│   │   ├── geometry/          # Vertex、Mesh、Triangle
│   │   ├── transform/         # 模型矩阵 / MVP 工具
│   │   ├── camera/            # Camera（透视 + 正交）
│   │   ├── pipeline/          # VertexShader、Rasterizer、FragmentShader
│   │   ├── editor/            # Scene、GameObject、Sprite2D、TextureManager、ITransformUI…
│   │   └── dataBase/          # .scene 文件的 JSON 序列化
│   ├── app/
│   │   └── main/              # 可执行主程序 + IModule 接口 + 各 TestModule_*
│   └── imgui/                 # ImGui 源码 + SDL2/SDLRenderer backend
├── bin/                       # 构建产物
├── Resource/                  # 纹理资源（启动时扫描）
├── Data/                      # 音频资源（启动时扫描）
└── scripts/                   # 构建脚本（build.bat 等）
```

---

## 三、构建产物

CMake 生成两个目标（见 `src/app/main/CMakeLists.txt`）：

| Target | 类型 | 作用 |
|---|---|---|
| `ST_Render_Manager` | 可执行 | 主程序：SDL + ImGui 编辑器，加载所有 IModule 测试模块 |
| `ST_Render_Dump` | 可执行 | 离屏诊断工具：固定相机角度扫描 + 像素统计，用于 3D 渲染 bug 复现 |

引擎本身拆分为多个**静态库**：`Math`、`Buffer`、`Geometry`、`Transform`、`Camera`、`Pipeline`、`Renderer`、`Editor`、`DataBase`、`Music`，通过 `/WHOLEARCHIVE` 强制链接（避免静态注册符号被裁掉）。

---

## 四、应用生命周期

```
main()
  │
  ├─ 1. 初始化
  │     ├─ SDL_Init(VIDEO | AUDIO)
  │     ├─ TextureManager::getInstance().scanResourceFolder()   // 扫描 Resource/
  │     ├─ AudioManager::getInstance().scanAudioFolder()       // 扫描 Data/
  │     ├─ SDL_CreateWindow + SDL_CreateRenderer
  │     ├─ ImGui::CreateContext + ImGui_ImplSDL2_InitForSDLRenderer + ImGui_ImplSDLRenderer2_Init
  │     ├─ 创建 canvas SDL_Texture（渲染目标，640x480）
  │     └─ new 所有 IModule 实例，push 到 modules[]
  │
  ├─ 2. 主循环（while running）
  │     │
  │     ├─ 2.1 事件处理
  │     │     ├─ SDL_PollEvent → ImGui_ImplSDL2_ProcessEvent
  │     │     ├─ 鼠标事件 → 命中检测 canvas 区域 → 转发给当前选中模块
  │     │     │     onMouseMove / onMouseDown / onMouseUp / onWheel
  │     │     └─ 键盘事件：ESC 退出、R 触发 rerender
  │     │
  │     ├─ 2.2 帧更新
  │     │     ├─ 若 needsRerender 或 needsRealTimeUpdate()
  │     │     │     └─ modules[sel]->render(canvasTexture, W, H)
  │     │     │           └─ 模块把 SDL_SetRenderTarget 切到 canvas，自行绘制
  │     │     └─ modules[sel]->runConsole(output)  // 更新底部控制台文本
  │     │
  │     ├─ 2.3 ImGui 渲染
  │     │     ├─ 顶栏菜单（主题切换 Dark/Light）
  │     │     ├─ 左栏：模块列表（点击切换 selectedModule）
  │     │     ├─ 中栏：modules[sel]->renderCreatePanel() 创建面板
  │     │     ├─ 中栏：Canvas 区域（直接显示 canvas SDL_Texture）
  │     │     ├─ 中栏：Console 区域（底部，可拖拽调整高度）
  │     │     ├─ 右栏：modules[sel]->renderControls() 模块控制面板
  │     │     └─ modules[sel]->renderOverlays() 弹出窗口（如纹理浏览器）
  │     │
  │     └─ 2.4 呈现
  │           └─ SDL_RenderPresent
  │
  └─ 3. 退出
        ├─ ImGui 析构 + SDL 析构
        ├─ delete 所有 modules[] 实例
        └─ SDL_Quit
```

---

## 五、模块系统（IModule 接口）

每个测试模块实现 `src/app/module/IModule.hpp` 定义的统一接口，关键虚函数：

| 类别 | 方法 | 用途 |
|---|---|---|
| 标识 | `const char* getName()` | 左侧列表显示 |
| 渲染 | `render(canvasTexture, W, H)` | 渲染到 SDL 渲染目标 |
| 渲染 | `renderControls()` | 右栏 ImGui 控制面板 |
| 渲染 | `renderCreatePanel()` | 中栏创建对象面板 |
| 渲染 | `renderOverlays()` | 弹出窗口 |
| 控制台 | `runConsole(output)` | 写入底部控制台文本 |
| 输入 | `onMouseMove / onMouseDown / onMouseUp` | 鼠标事件 |
| 输入 | `onWheel(dx, dy, x, y, W, H)` | 滚轮事件（用于缩放等） |
| 标志 | `needsRealTimeUpdate()` | 是否每帧强制 rerender（动画场景） |
| 标志 | `needsRerender`（成员） | 模块自己标记「下次循环重渲」 |

模块列表（在 `test_manager_main.cpp` 中实例化，注册顺序即显示顺序）：

| 模块 | 用途 |
|---|---|
| `TestModule_FrameBuffer` | FrameBuffer 单元测试（直接像素读写） |
| `TestModule_Rasterizer` | 屏幕空间三角形光栅化 |
| `TestModule_3DRender` | 3D 场景综合演示（相机 + 光照 + 立方体） |
| `TestModule_DragWindow` | ImGui 可拖动弹出窗口演示 |
| `TestModule_Texture` | 纹理加载 + 采样（Repeat/Clamp、Nearest/Bilinear） |
| `TestModule_Music` | 音频播放（SDL2_mixer + mpg123） |
| `TestModule_NetworkTest` | 网络连通性测试 |
| `TestModule_Circle` | 纯 SDL2 圆绘制 |
| `TestModule_Rectangle` | 纯 SDL2 矩形绘制 |

---

## 六、渲染管线（`src/engine/pipeline/`）

每个三角形从顶点到像素的旅程：

```
Mesh (Vertex 数组)
  │
  ▼
VertexShader::process(Vertex) × N
  │   uniform: model / view / projection / viewport
  │   输出 clip 空间坐标 + 透视除法后的 w（用于透视校正插值）
  │
  ▼
Rasterizer::rasterizeTriangle(v0, v1, v2, fragmentShader)
  │   1. 三角形包围盒扫描
  │   2. 重心坐标 (α, β, γ)
  │   3. DepthBuffer::testAndSet(x, y, depth)   ← 深度测试
  │   4. 透视校正插值：α' = α/w0, β' = β/w1, γ' = γ/w2 归一化
  │   5. fragmentShader(Fragment) → Color → setPixel()
  │
  ▼
FragmentShader::shade(Fragment)
  │   支持 shadeFlat / shadeGouraud / shadePhong / shadeBlinnPhong
  │   纹理采样通过 Texture2D::sample(u, v)
  │
  ▼
FrameBuffer (CPU 像素数组)
```

---

## 七、资源管理

| 资源类型 | 加载时机 | 管理器 | 路径 |
|---|---|---|---|
| 图片（jpg/png/bmp/tga） | 启动时扫描 | `TextureManager`（单例） | `Resource/` |
| 音频（mp3/wav/ogg） | 启动时扫描 | `AudioManager`（单例） | `Data/` |

构建后通过 CMake `add_custom_command(POST_BUILD)` 把 `Resource/`、`Data/`、`SDL2.dll`、`SDL2_mixer.dll`、`mpg123.dll` 拷贝到 `bin/` 旁边，保证运行时能找到资源。

---

## 八、关键设计决策

1. **纯 CPU 软件光栅化** — 演示 GPU 流水线每个阶段。最终像素通过 SDL_Texture 上传显示。
2. **IModule 接口解耦** — 每个测试模块独立可运行、无相互依赖，新增模块只需包含头文件并 push 到 `modules[]`。
3. **静态库 + /WHOLEARCHIVE** — 引擎静态注册（单例、宏注册）通过强制链接符号保证不被链接器裁掉。
4. **透视校正插值** — 重心坐标除以 w 后归一化，保证纹理和深度在远近距离上正确插值。
5. **SDL2 仅用于窗口 + ImGui + 最终上传** — 不参与几何 / 光栅化计算，保持软件渲染的纯粹性。