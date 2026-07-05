# ST Render - Test Manager & Renderer Plan

## 项目目标

构建一个可视化测试管理器，支持 **2D 渲染器** 和 **3D 渲染器** 的模块化测试。所有渲染结果通过 Dear ImGui 界面展示，支持模块切换。

---

## 当前实现情况

### 已完成

| 模块 | 状态 | 说明 |
|------|------|------|
| Math | ✅ | Vector2/3/4、MathUtils |
| Buffer | ✅ | FrameBuffer、DepthBuffer |
| Geometry | ✅ | Vertex、Triangle、Mesh、MeshFactory |
| Transform | ✅ | MVP 矩阵、Viewport 矩阵 |
| Camera | ✅ | 透视/正交相机 |
| Pipeline | ✅ | VertexShader、FragmentShader、Rasterizer、LightAndMaterial |
| Renderer | ✅ | 主渲染器 |
| SDL2 窗口测试 | ✅ | 独立窗口测试 |
| Test Manager | ✅ | ImGui 界面，模块可切换 |
| ITestModule 接口 | ✅ | 解耦渲染模块 |
| TestModule_Circle | ✅ | 画圆示例 |
| TestModule_Rectangle | ✅ | 画方块示例 |

### 项目结构

```
Main/
├── test_manager_main.cpp          # Test Manager 主程序
├── ITestModule.hpp               # 渲染模块接口
├── TestModule_Circle.hpp         # 画圆模块
├── TestModule_Rectangle.hpp      # 画方块模块
├── imgui_impl_sdl2.h/cpp         # ImGui 平台层 backend
├── imgui_impl_sdlrenderer2.h/cpp # ImGui 渲染层 backend
├── test_math.cpp
├── test_buffer.cpp
├── test_geometry.cpp
├── test_transform.cpp
├── test_camera.cpp
├── test_pipeline.cpp
├── test_renderer.cpp
├── test_sdl_window.cpp
└── test_sdl_circle.cpp

Math/                            # 数学库
Buffer/                          # 缓冲区
Geometry/                        # 几何图元
Transform/                       # 坐标变换
Camera/                          # 相机
Pipeline/                        # 渲染管线
Renderer/                        # 主渲染器
```

### Test Manager 架构

```
ITestModule (接口)
    ├── getName()  → 返回模块名称
    └── render()   → 渲染到离屏纹理

TestManager
    ├── 模块注册
    ├── 模块列表 UI
    └── 切换时重新渲染
```

---

## 下一阶段计划

### 第一步：完善 Test Manager UI

**目标**：把现有的控制台输出测试也集成到 ImGui 界面中。

```
┌──────────────────────────────────────────────────┐
│ ST Render - Test Manager                         │
├────────────┬─────────────────────────────────────┤
│ Tests      │ Output                              │
│            │                                     │
│ ▼ ST_Test  │ [ST_Test] Math Utils               │
│    SDL圆   │ =================================   │
│    SDL方块 │ Running: Math Utils...              │
│            │ [PASS] Vector3 addition             │
│ ▼ Console  │ [PASS] Vector3 dot product          │
│    Math    │ [PASS] Matrix multiplication        │
│    Buffer  │                                     │
│    Geo...  │ [Run Again] [Clear]                 │
└────────────┴─────────────────────────────────────┘
```

**新增模块类型**：
- `ITestModule_Render` — 渲染结果到 ImGui Image（已有基础）
- `ITestModule_Console` — 控制台输出到 ImGui Text（待实现）

---

### 第二步：2D 渲染器测试模块

**目标**：使用已有的 Math/Buffer/Pipeline 模块，实现 2D 渲染测试。

| 模块名 | 功能 |
|--------|------|
| `TestModule_2D_Lines` | 绘制 2D 线条（ Bresenham / DDA） |
| `TestModule_2D_Triangle` | 填充三角形（扫描线算法） |
| `TestModule_2D_Transform` | 2D 变换（旋转、缩放、平移） |
| `TestModule_2D_Buffer` | FrameBuffer 操作演示 |
| `TestModule_2D_Pipeline` | 完整 2D 渲染管线流程 |

---

### 第三步：3D 渲染器测试模块

**目标**：使用已有的 Transform/Camera/Pipeline 模块，实现 3D 渲染测试。

| 模块名 | 功能 |
|--------|------|
| `TestModule_3D_Cube` | 渲染立方体（12 三角形） |
| `TestModule_3D_MVP` | MVP 变换可视化 |
| `TestModule_3D_Camera` | 相机控制（旋转、缩放） |
| `TestModule_3D_DepthTest` | 深度测试演示 |
| `TestModule_3D_BackfaceCull` | 背面剔除演示 |
| `TestModule_3D_Pipeline` | 完整 3D 渲染管线流程 |

---

## 2D 渲染器详细设计

### 渲染管线

```
2D 渲染管线：
顶点数据 → 局部坐标 → 世界坐标 → 视口坐标 → 光栅化 → 片段着色 → 颜色缓冲
```

### 核心类

```cpp
// 2D 渲染器
class Renderer2D {
    FrameBuffer frameBuffer;
    Transform2D transform;

    void clear(Color color);
    void drawLine(int x0, int y0, int x1, int y1, Color color);
    void drawTriangle(const Vertex2D& v0, const Vertex2D& v1, const Vertex2D& v2, Color color);
    void fillTriangle(const Vertex2D& v0, const Vertex2D& v1, const Vertex2D& v2, Color color);
};
```

### 光栅化算法

| 算法 | 说明 |
|------|------|
| DDA | 数值微分分析器，基础直线绘制 |
| Bresenham | 整数运算，效率高，商用级别 |
| 扫描线填充 | 三角形填充，扫描线与边相交 |

---

## 3D 渲染器详细设计

### 渲染管线

```
3D 渲染管线：
局部空间 → 世界空间 → 观察空间 → 裁剪空间 → NDC → 屏幕空间 → 光栅化 → 片段着色
```

### 核心流程

```
Mesh (顶点数据)
    ↓
VertexShader (MVP 变换)
    ↓
Clip (裁剪)
    ↓
NDC (归一化设备坐标)
    ↓
Viewport (视口变换)
    ↓
Rasterizer (光栅化)
    ↓
FragmentShader (片段着色)
    ↓
DepthTest (深度测试)
    ↓
FrameBuffer (颜色输出)
```

---

## 文件结构规划

```
Main/
├── test_manager_main.cpp           # 主程序
├── ITestModule.hpp                # 渲染模块接口
│
├── // 2D 模块
├── TestModule_2D.hpp              # 2D 模块基类
├── TestModule_2D_Lines.hpp
├── TestModule_2D_Triangle.hpp
├── TestModule_2D_Transform.hpp
│
├── // 3D 模块
├── TestModule_3D.hpp              # 3D 模块基类
├── TestModule_3D_Cube.hpp
├── TestModule_3D_MVP.hpp
├── TestModule_3D_Camera.hpp
├── TestModule_3D_Pipeline.hpp
│
├── // 原有测试（控制台输出）
├── test_math.cpp
├── test_buffer.cpp
├── ...
│
├── // ImGui backend
├── imgui_impl_sdl2.h/cpp
├── imgui_impl_sdlrenderer2.h/cpp
```

---

## 实现顺序

### 阶段 1：完善 UI（1-2 天）

- [ ] Console 输出模块（控制台文本显示在 ImGui）
- [ ] 左面板分组（ST_Render / ST_Test / Console）
- [ ] Run Again / Clear 按钮功能
- [ ] 滚动条支持

### 阶段 2：2D 渲染测试（2-3 天）

- [ ] `Renderer2D` 类
- [ ] Bresenham 直线绘制
- [ ] 三角形填充
- [ ] `TestModule_2D_Lines`
- [ ] `TestModule_2D_Triangle`

### 阶段 3：3D 渲染测试（3-5 天）

- [ ] 立方体渲染
- [ ] MVP 变换可视化
- [ ] 相机控制
- [ ] 深度测试演示
- [ ] 背面剔除演示
- [ ] `TestModule_3D_*`

### 阶段 4：扩展功能（可选）

- [ ] 纹理映射
- [ ] 光照模型（Flat / Gouraud / Phong）
- [ ] 动画支持（旋转立方体）

---

## 技术栈

| 组件 | 技术 |
|------|------|
| 窗口/输入 | SDL2 |
| UI 界面 | Dear ImGui |
| 2D/3D 渲染 | 自研软件渲染器 |
| 构建系统 | CMake + vcpkg |
| 编译器 | MSVC (Visual Studio 2022) |

---

## 验收标准

### 当前版本
- [x] Test Manager 界面可运行
- [x] 模块可切换，渲染同步更新
- [x] SDL Circle / Rectangle 示例正常

### 下一版本（阶段 1 完成后）
- [ ] Console 测试可显示在 ImGui 界面
- [ ] 所有模块可切换
- [ ] Run Again / Clear 功能正常

### 最终版本
- [ ] 2D 渲染测试模块完整
- [ ] 3D 渲染测试模块完整
- [ ] 所有测试可在管理器中运行

---

*文档版本：v1.0*
*更新日期：2026-05-28*
