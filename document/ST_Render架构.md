# ST_Render 软件渲染引擎架构文档

> 主程序入口：`Main/test_manager_main.cpp`

---

## 一、项目概述

ST_Render 是一个**纯软件渲染引擎**，完全在 CPU 上通过 SDL2 + ImGui 实现光栅化，不依赖 GPU 硬件加速。项目目标是从零演示现代渲染管线的各个阶段：顶点变换、三角形光栅化、纹理采样、深度测试、抗锯齿、视口管理、场景管理和编辑器 UI。

### 技术栈

| 层级 | 技术 |
|---|---|
| 窗口 & 输入 | SDL2 |
| 编辑器 UI | ImGui（ImGui_ImplSDL2 + ImGui_ImplSDLRenderer2） |
| 图像加载 | stb_image.h |
| 渲染 | 全 CPU 软件光栅化 |

---

## 二、主程序入口 (`test_manager_main.cpp`)

### 2.1 启动流程

```cpp
int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_CreateWindowAndRenderer(WINDOW_W, WINDOW_H, ...);
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // 扫描纹理资源
    ST::TextureManager::getInstance().scanResourceFolder();

    // 创建所有测试模块
    modules.push_back(new TestModule_2D_Scene());
    modules.push_back(new TestModule_Sprite2D());
    // ... 其他模块

    // 为每个模块设置 SDL_Renderer
    for (auto* m : modules)
        m->setRenderer(renderer);

    // 主循环
    while (running) { /* SDL_PollEvent → 模块事件派发 → ImGui 渲染 → 帧渲染 */ }
}
```

### 2.2 主循环结构

```
┌─────────────────────────────────────────┐
│  SDL_PollEvent                         │
│    └─ 鼠标/键盘事件                     │
│         ├─ → ImGui_ImplSDL2_ProcessEvent│
│         └─ → modules[sel]->onMouse*     │
│              modules[sel]->onWheel      │
├─────────────────────────────────────────┤
│  每帧检测 needsRealTimeUpdate()         │
│    ├─ 模块.render(renderer, W, H)       │
│    │    └─ 渲染到 SDL_Texture* canvas  │
│    └─ modules[sel]->runConsole()        │
├─────────────────────────────────────────┤
│  ImGui NewFrame / Render               │
│    ├─ 左侧面板：模块选择列表             │
│    ├─ 左中：Create Object 面板          │
│    ├─ 右侧：renderControls()           │
│    ├─ 中央：canvas 纹理 + 控制台输出     │
│    │    └─ 垂直/水平分割线             │
│    └─ renderOverlays() (弹出窗口)        │
├─────────────────────────────────────────┤
│  SDL_RenderPresent                     │
└─────────────────────────────────────────┘
```

### 2.3 UI 布局（固定坐标）

```
┌──────────┬────────────┬────────────┬──────────┐
│          │            │            │          │
│  Tests   │   Create   │   Canvas   │ Controls │
│  (220px) │  Object    │  (640x480) │ (220px)  │
│          │  (200px)   │            │          │
│          │            │────────────│          │
│          │            │  Console   │          │
└──────────┴────────────┴────────────┴──────────┘
  x=0        x=220        x=420        x=1060
```

- Canvas 实际屏幕坐标：**x = 420, y = 50**（含 ImGui 标题栏）
- 分割线可拖动调整宽度
- `CANVAS_W = 640`, `CANVAS_H = 480`

---

## 三、模块系统 (`ITestModule.hpp`)

### 3.1 抽象接口

```cpp
class ITestModule {
    virtual const char* getName() const = 0;
    virtual bool hasRenderOutput() const { return true; }
    virtual bool hasConsoleOutput() const { return true; }
    virtual bool needsRealTimeUpdate() const { return false; }
    // 渲染
    virtual void render(void* canvasTexture, int canvasW, int canvasH) {}
    virtual bool renderControls() { return false; }
    virtual void renderOverlays() {}
    virtual void renderCreatePanel() {}
    virtual void setRenderer(void* renderer) {}
    virtual void runConsole(std::string& output) {}
    // 输入
    virtual void onMouseMove(int x, int y) {}
    virtual void onMouseDown(int button, int x, int y) {}
    virtual void onMouseUp(int button) {}
    virtual void onWheel(float dx, float dy, int canvasX, int canvasY, int canvasW, int canvasH) {}
    bool needsRerender = false;  // 标记需要重渲染
};
```

### 3.2 全部测试模块

| 模块类名 | 源文件 | 功能描述 |
|---|---|---|
| `TestModule_2D_Scene` | `TestModule_2D_Scene.hpp/.cpp` | **主要场景编辑器**：对象管理、视口平移/缩放、点击拾取、SSAA/FXAA 抗锯齿、网格、纹理浏览器 |
| `TestModule_Sprite2D` | `TestModule_Sprite2D.hpp/.cpp` | 精灵场景测试，对象生成、选择、拖拽 |
| `TestModule_FrameBuffer` | `TestModule_FrameBuffer.hpp/.cpp` | FrameBuffer 单元测试，像素级操作 |
| `TestModule_Rasterizer` | `TestModule_Rasterizer.hpp/.cpp` | 光栅化器直接测试，屏幕空间三角绘制 |
| `TestModule_VertexShader` | `TestModule_VertexShader.hpp/.cpp` | 顶点着色器测试，带网格渲染和参数控制 |
| `TestModule_Texture` | `TestModule_Texture.hpp/.cpp` | 纹理加载和采样测试 |
| `TestModule_Circle` | `TestModule_Circle.hpp` | 纯 SDL2 圆绘制 |
| `TestModule_Rectangle` | `TestModule_Rectangle.hpp` | 纯 SDL2 矩形绘制 |
| `TestModule_DragWindow` | `TestModule_DragWindow.hpp` | ImGui 可拖动弹出窗口测试 |

---

## 四、核心模块：`TestModule_2D_Scene` 详解

### 4.1 核心成员

```cpp
ST::Scene          m_scene;           // 场景：所有 GameObject 的容器
ST::FrameBuffer*   m_frameBuffer;     // 主渲染目标
ST::FrameBuffer*   m_ssaaBuffer;     // SSAA 超采样缓冲
ST::DepthBuffer*   m_depthBuffer;
ST::DepthBuffer*   m_ssaaDepthBuffer;
ST::VertexShader   m_vertexShader;    // 顶点处理器
ST::Rasterizer     m_rasterizer;      // 光栅化器
ST::Sprite2DUI     m_transformUI;      // ImGui 变换控制面板
ST::GameObject*    m_selected;        // 当前选中对象

// 视口状态
float              m_zoom;             // 缩放比例
float              m_panX, m_panY;     // 平移偏移
int                m_canvasW, m_canvasH;
int                m_canvasTop;         // Canvas 在屏幕上的 Y 起始位置（50）

// 抗锯齿
int                m_aaMode;           // 0=无, 1=SSAA, 2=FXAA
int                m_ssaaLevel;        // SSAA 倍数（默认 2x）
```

### 4.2 渲染管线（`render` 方法）

```
render()
  ├─ rebuildBuffers()            // 按需重建 FrameBuffer
  ├─ renderScene()               // 主场景渲染
  │     ├─ 构建 View 矩阵：T(panX,panY) * S(zoom)
  │     ├─ 构建 Projection 矩阵：orthographic(-halfW/zoom, ...)
  │     ├─ 按 sortingOrder 升序遍历对象
  │     ├─ 对每个三角形：
  │     │     VS.process(vertex) → rasterizeTriangle()
  │     │          └─ 片段着色器：纯色 或 纹理采样
  │     └─ 绘制选中物体 AABB + 高亮
  ├─ applySSAA() 或 applyFXAA()  // 抗锯齿后处理
  └─ FrameBuffer → SDL_Texture → SDL_RenderCopy()
```

### 4.3 视口缩放（`onWheel`）

缩放时保持鼠标下的世界坐标点不变：

```cpp
void onWheel(...) {
    float oldZoom = m_zoom;
    m_zoom *= (dy > 0 ? 1.1f : 0.9f);

    // 鼠标位置的世界坐标（缩放前）
    int cx = toCanvasX(canvasX);
    int cy = canvasY - m_canvasTop;
    float wx = (cx - halfW) / oldZoom - m_panX;
    float wy = (halfH - cy) / oldZoom - m_panY;

    // 调整平移，使该世界点仍在鼠标下（缩放后）
    m_panX = (cx - halfW) / m_zoom - wx;
    m_panY = (halfH - cy) / m_zoom - wy;
}
```

### 4.4 点击拾取算法（屏幕空间 AABB）

将物体顶点投影到**屏幕像素坐标**，在屏幕空间做 AABB 碰撞检测——与渲染使用完全相同的矩阵，绕过了逆投影误差：

```cpp
// 视口矩阵（与 renderScene 完全一致）
Matrix4x4 view    = T(panX,panY) * S(zoom);
Matrix4x4 proj    = orthographic(-hw, hw, -hh, hh);
Matrix4x4 mvp     = proj * view * model;

for (auto& up : m_scene.getObjects()) {
    // 跳过 sortingOrder 小的（被上层物体挡住）
    if (obj->sortingOrder < topSortingOrder) continue;

    // 把顶点投影到屏幕像素
    float minSX=FLT_MAX, maxSX=-FLT_MAX;
    float minSY=FLT_MAX, maxSY=-FLT_MAX;
    for (auto& v : vertices) {
        Vector4 clip = mvp * Vector4(v.position, 1);
        float sx = (clip.x/clip.w + 1) * 0.5 * canvasW;
        float sy = (1 - clip.y/clip.w) * 0.5 * canvasH;
        minSX = min(minSX, sx); maxSX = max(maxSX, sx);
        minSY = min(minSY, sy); maxSY = max(maxSY, sy);
    }

    // 屏幕空间 AABB 判断（逆序找最上层）
    if (cx >= minSX && cx <= maxSX && cy >= minSY && cy <= maxSY) {
        topObj = obj;
        topSortingOrder = obj->sortingOrder;
    }
}
```

---

## 五、渲染管线 (`Pipeline/`)

### 5.1 顶点着色器 (`VertexShader.hpp`)

**输入：** `Vertex { position, normal, texCoord, color }`

**Uniforms：**
```cpp
struct Uniform {
    Matrix4x4 modelMatrix;       // 物体变换
    Matrix4x4 viewMatrix;        // 相机视图
    Matrix4x4 projectionMatrix;  // 投影矩阵
    Matrix4x4 viewportMatrix;    // 视口映射

    Matrix4x4 getMvpMatrix() { return projection * view * model; }
    Matrix4x4 getModelViewMatrix() { return view * model; }
};
```

**输出：** `VertexOut { color, position(clip空间), worldPosition, normal, texCoord }`

关键公式：
```
clipPosition = MVP * localPosition
worldPosition = Model * localPosition
```

### 5.2 光栅化器 (`Rasterizer.hpp`)

**核心方法：`rasterizeTriangle(v0, v1, v2, fragmentShader)`**

```
1. toScreenSpace(v)  →  NDC 转像素坐标
       sx = (ndc.x + 1) * 0.5 * width
       sy = (1 - ndc.y) * 0.5 * height

2. 计算三角形包围盒 [minX,maxX] × [minY,maxY]

3. 对包围盒内每个像素：
   ├─ computeBarycentric(p, a, b, c)
   │     α = ((b.y-c.y)(p.x-c.x) + (c.x-b.x)(p.y-c.y)) /
   │          ((b.y-c.y)(a.x-c.x) + (c.x-b.x)(a.y-c.y))
   │     β  = ((c.y-a.y)(p.x-c.x) + (a.x-c.x)(p.y-c.y)) / denominator
   │     γ  = 1 - α - β
   │
   ├─ 深度测试（通过 DepthBuffer::testAndSet）
   │
   ├─ 透视校正插值（关键！）：
   │     α' = α / v0.pos.w,  β' = β / v1.pos.w,  γ' = γ / v2.pos.w
   │     归一化：α'' = α'/(α'+β'+γ')
   │     属性 = α''*attr0 + β''*attr1 + γ''*attr2
   │
   └─ fragmentShader(Fragment) → Color → setPixel()
```

### 5.3 片段着色器 (`FragmentShader.hpp/.cpp`)

**光照模型：**

| 方法 | 说明 |
|---|---|
| `shadeFlat` | 每个三角形用第一个顶点的法线，全平面着色 |
| `shadeGouraud` | 在顶点着色器计算颜色，片段直接插值（WebGL 1.x 风格） |
| `shadePhong` | 每个片段计算 Phong 反射模型 |
| `shadeBlinnPhong` | 使用半程向量 H 代替反射向量 R（默认） |

**Phong 反射模型：**
```
color = Ka * ambient
      + Σ(Kd * (N·L) * diffuse + Ks * (R·V)^n * specular) / (Kc + Kl*d + Kq*d²)
```

**纹理采样：** 支持 Repeat 和 Clamp 包裹模式，Nearest 和 Bilinear 滤波模式。

---

## 六、编辑器系统 (`Editor/`)

### 6.1 场景 (`Scene.hpp/.cpp`)

```cpp
class Scene {
    std::vector<std::unique_ptr<GameObject>> objects;
    Camera camera;
    Vector3 viewportOffset;    // 视口平移
    float  viewportZoom;       // 视口缩放
    ITransformUI* m_transformUI;  // 变换面板（可插拔）

    GameObject* createGameObject(name);
    Sprite2D*   createSprite2D(name);
    void destroyGameObject(GameObject*);
    void getSortedObjects(vector<GameObject*>& out);  // 按 sortingOrder 排序
};
```

### 6.2 游戏对象 (`GameObject.hpp/.cpp`)

```cpp
class GameObject {
    Vector3 position, rotation, scale;
    Color   color, tint;
    int     sortingOrder;       // 越大越靠前（后绘制，覆盖前面的）
    bool    showAABB;           // 显示包围盒
    ShapeType shapeType;       // Quad / Cube / Circle / Triangle / ...
    Mesh    m_mesh;             // 几何数据
    Texture2D* m_texture;      // 可选纹理
};
```

工厂方法：`createQuad(w, h)`、`createCube()`、`createCircle(radius, segments)`

### 6.3 精灵 (`Sprite2D.hpp/.cpp` — 继承 GameObject)

```cpp
class Sprite2D : public GameObject {
    Texture2D   m_texture;      // 拥有纹理数据（从 TextureManager 复制）
    std::string m_textureName;  // 纹理文件名
    int         m_textureIndex; // 在 TextureManager 中的索引
    bool        m_flipX, m_flipY;
};
```

### 6.4 变换 UI (`TransformUI_ImGui.hpp`)

ImGui 实现，支持 Position (DragFloat3)、Rotation (弧度→度数显示)、Scale (≥1.0 钳制)、Sorting Order、Show AABB 复选框。

### 6.5 精灵 UI (`Sprite2DUI.hpp` — 继承 TransformUI_ImGui)

额外添加精灵特有控制（Flip X/Y）和**纹理浏览器弹出窗口**：

```cpp
// 纹理浏览器（renderOverlays() 中调用）
void renderBrowserOnTop() {
    ImGui::OpenPopup("Texture Browser");
    ImGui::BeginPopupModal("Texture Browser", ...);
    // 5列网格，每格 96x96
    // 点击 → 设置 sprite->setTextureIndex(i)
}
```

### 6.6 纹理管理器 (`TextureManager.hpp/.cpp`)

单例模式，启动时扫描 `Resource/` 文件夹，枚举所有图片文件并加载：

```cpp
ST::TextureManager::getInstance().scanResourceFolder();
// → Win32 FindFirstFileA → stb_image → Texture2D → m_textures[]
```

---

## 七、数学库 (`Math/`)

### 7.1 矩阵 (`Matrix4x4.hpp`)

纯头文件实现，所有矩阵操作按行主序存储：

```
| m[0]  m[1]  m[2]  m[3]  |
| m[4]  m[5]  m[6]  m[7]  |
| m[8]  m[9]  m[10] m[11] |
| m[12] m[13] m[14] m[15] |
```

**工厂方法：**
- `translation(x,y,z)` / `rotationZ(angle)` / `scale(x,y,z)`
- `perspective(fov, aspect, near, far)` — OpenGL 风格
- `orthographic(l, r, b, t, n, f)` — 正交投影
- `lookAt(eye, target, up)` — 视图矩阵

**求逆：** Gauss-Jordan 消元法，复杂度 O(n³)

### 7.2 向量 (`Vector2/3/4.hpp`)

- `Vector3`：点积 `dot()`、叉积 `cross()`、归一化 `normalized()`
- `Vector4`：齐次坐标变换，支持 `toVector3()` 和 `toVector3Perspective()`
- `Color`：`r, g, b, a`（float 0-1），支持 `lerp()` 和 `static` 颜色工厂

---

## 八、缓冲系统 (`Buffer/`)

### `FrameBuffer.hpp`

CPU 端像素缓冲，`std::vector<Color>` 行主序存储。

```cpp
void setPixel(x, y, Color);  // 边界检查
Color getPixel(x, y);
void clear(Color);
```

### `DepthBuffer.hpp`

深度缓冲，初始化为 `FLT_MAX`。Z-Test 算法：

```cpp
bool testAndSet(x, y, depth) {
    if (depth < depthBuffer[y*w+x]) {
        depthBuffer[y*w+x] = depth;
        return true;  // 通过测试，写入
    }
    return false;  // 被遮挡，丢弃
}
```

### `Texture2D.hpp`

CPU 端 2D 纹理。`sample(u, v)` 支持：
- **包裹模式**：Repeat（取小数）、Clamp（钳制到 0-1）
- **滤波模式**：Nearest（最近邻）、Bilinear（4 样本双线性插值）

---

## 九、几何系统 (`Geometry/`)

| 类 | 说明 |
|---|---|
| `Vertex { pos, normal, texCoord, color }` | 单顶点，带线性插值 `lerp()` |
| `Mesh { vertices, indices }` | 顶点缓冲 + 索引缓冲 |
| `Mesh::createQuad(w, h)` | 生成 4 顶点 2 三角形的矩形 |
| `Mesh::createCircle(r, segments)` | 扇形三角化 |
| `Mesh::createCube(s)` | 8 顶点 12 三角形的正方体 |
| `Triangle { v0, v1, v2 }` | 简单三角形封装 |

---

## 十、相机 (`Camera/`)

```cpp
class Camera {
    Vector3 m_eye, m_target, m_up;
    Mode    m_mode;  // Perspective / Orthographic
    float   m_fov, m_aspect, m_near, m_far;

    Matrix4x4 getViewMatrix();      // lookAt()
    Matrix4x4 getProjectionMatrix(); // perspective() 或 orthographic()
};
```

---

## 十一、变换组件 (`Transform/`)

```cpp
Matrix4x4 getModelMatrix(transform);  // T * Rz * Ry * Rx * S
Matrix4x4 getMVPMatrix(t, v, p);      // P * V * M（便捷包装）
Matrix4x4 getViewportMatrix(w, h);    // NDC[-1,1] → 像素坐标
```

---

## 十二、高层渲染器 (`Renderer/`)

组合所有管线组件的高层封装：

```cpp
Renderer {
    FrameBuffer*   m_frameBuffer;
    DepthBuffer*   m_depthBuffer;
    VertexShader   m_vertexShader;
    FragmentShader m_fragmentShader;
    Rasterizer     m_rasterizer;
    Camera*        m_camera;

    void render(Mesh& mesh) {
        for (每个三角形) {
            vOut = m_vertexShader.process(v);  // MVP 变换
            m_rasterizer.rasterizeTriangle(vOut[0], vOut[1], vOut[2],
                [&](Fragment& f) {            // 片段着色
                    return m_fragmentShader.shade(f);
                });
        }
    }
};
```

---

## 十三、数据流总览

```
test_manager_main.cpp
│
├─ TextureManager::scanResourceFolder()
│      └─ stb_image → std::vector<Color> → Texture2D[]
│
├─ modules[i]->setRenderer(SDL_Renderer*)
│
└─ 主循环
    │
    ├─ SDL_PollEvent → modules[i]->onMouse*(x, y)
    │      └─ TestModule_2D_Scene::onMouseDown()
    │            └─ 屏幕空间 AABB 点击拾取
    │
    ├─ modules[i]->render(SDL_Renderer*, W, H)
    │      └─ TestModule_2D_Scene::render()
    │            ├─ rebuildBuffers(W, H)
    │            ├─ renderScene()  ← 核心软件渲染
    │            │      ├─ VertexShader.process() × N vertices
    │            │      ├─ Rasterizer.rasterizeTriangle() × M triangles
    │            │      │      └─ FragmentShader.shade() / Texture.sample()
    │            │      └─ DepthBuffer.testAndSet()
    │            ├─ applySSAA() 或 applyFXAA()
    │            └─ FrameBuffer → SDL_Texture → SDL_RenderCopy()
    │
    ├─ modules[i]->renderControls()   → ImGui 右侧控制面板
    ├─ modules[i]->renderCreatePanel() → ImGui 创建对象面板
    └─ modules[i]->renderOverlays()   → 纹理浏览器弹出窗口
```

---

## 十四、关键设计决策

1. **纯 CPU 软件渲染**：所有光栅化在 CPU 上完成，通过 `SDL_CreateTextureFromSurface` 上传到 GPU 显示。演示了 GPU 光栅化管线的每个阶段。

2. **模块化测试框架**：`ITestModule` 抽象接口 + `TestRegistry` 注册机制，每个功能独立可测，模块间无耦合。

3. **透视校正插值**：`rasterizeTriangle` 中通过 `α/Wa, β/Wb, γ/Wc` 归一化权重实现透视校正，保证纹理和深度在远近距离上正确插值——这是软件光栅化区别于简单线性插值的关键。

4. **屏幕空间点击拾取**：点击时将物体顶点投影到屏幕像素 AABB，与渲染使用完全一致的 MVP 矩阵，绕过了逆投影的累积误差，且自然支持旋转物体。

5. **视口缩放保支点**：`onWheel` 中先计算缩放前鼠标下的世界坐标，再反推新的 `m_panX/Y`，保证缩放时鼠标指向的世界点不变。

6. **抗锯齿策略**：SSAA（先超采样再下采样）和 FXAA（屏幕空间边缘检测 + 像素混合）两种模式，SSAA 质量高但慢，FXAA 质量稍低但快。
