# 2D Scene 渲染调试计划

## 问题描述

渲染一个黄色正方形，但屏幕全黑，没有任何显示。

---

## 测试结果

### 1.1 FrameBuffer → SDL 纹理

**结果**：✅ 通过

**说明**：
- FrameBuffer 手动设置像素后，能正确显示到屏幕
- SDL_CreateRGBSurfaceFrom 和 SDL_CreateTextureFromSurface 工作正常
- 像素格式：BGRA (0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000) 是正确的

---

## 调试流程

### 阶段 1：基础测试（跳过渲染管线）

#### 1.1 FrameBuffer → SDL 纹理

**目标**：验证 FrameBuffer 的像素数据能正确显示到屏幕上

**步骤**：
1. 手动在 FrameBuffer 里画几个彩色像素（不用渲染管线）
2. 验证 SDL 纹理能正确读取并显示这些像素

**预期结果**：能看到几个彩色像素点

**验证方法**：
```cpp
// 手动在 FrameBuffer 中心画一个黄色像素
m_frameBuffer->setPixel(320, 240, Color(1, 1, 0, 1));
```

---

#### 1.2 直接画三角形（跳过 VertexShader）

**目标**：验证光栅化器能正确填充三角形

**步骤**：
1. 创建 VertexOut，直接设置屏幕坐标
2. 调用光栅化器
3. 验证颜色写入

**预期结果**：能看到填充的三角形

**验证方法**：
```cpp
// 直接构造屏幕坐标的顶点
VertexOut v0, v1, v2;
v0.position = Vector4(100, 100, 0, 1);
v0.color = Color(1, 1, 0, 1);
v1.position = Vector4(200, 100, 0, 1);
v1.color = Color(1, 1, 0, 1);
v2.position = Vector4(150, 200, 0, 1);
v2.color = Color(1, 1, 0, 1);

m_rasterizer.rasterizeTriangle(v0, v1, v2, ...);
```

---

### 阶段 2：渲染管线测试

#### 2.1 VertexShader 测试

**目标**：验证 MVP 变换正确

**步骤**：
1. 打印顶点变换后的裁剪空间坐标
2. 验证 x,y 在 [-w, w] 范围内
3. 验证 z 在 [-w, w] 范围内

**预期结果**：
```
Local: (-0.5, -0.5, 0)
Clip: (-0.1875, -0.25, -1.002, 1)  // x,y,z 都在 [-1,1] 范围内
```

**检查点**：
- [ ] modelMatrix 是否正确
- [ ] viewMatrix 是否正确
- [ ] projectionMatrix 是否正确

---

#### 2.2 光栅化器测试

**目标**：验证屏幕空间坐标和包围盒计算

**步骤**：
1. 打印 NDC 坐标
2. 打印屏幕空间坐标
3. 验证包围盒 minX/maxX/minY/maxY

**预期结果**：
```
NDC v0: (-0.1875, -0.25, -1.002)
Screen v0: (260, 300)  // 在屏幕范围内
BBox: minX=100, maxX=300, minY=100, maxY=300
```

**检查点**：
- [ ] NDC → Screen 转换是否正确
- [ ] 包围盒是否包含屏幕区域
- [ ] min/max 是否超出屏幕边界

---

#### 2.3 FragmentShader 测试

**目标**：验证灯光计算和颜色输出

**步骤**：
1. 加 directional light
2. 打印片段颜色
3. 验证颜色是否正确

**预期结果**：
```
Fragment color: (1, 1, 0)  // 黄色
```

**检查点**：
- [ ] 灯光是否正确添加
- [ ] 法线是否正确
- [ ] 光照计算是否正确

---

#### 2.4 深度测试

**目标**：验证深度测试不会阻止像素写入

**步骤**：
1. 打印深度缓冲初始值
2. 打印计算出的深度值
3. 禁用深度测试，验证颜色是否写入

**预期结果**：
```
Depth buffer initial: FLT_MAX
Fragment depth: -1.002
Depth test: PASS (depth < FLT_MAX)
```

**检查点**：
- [ ] 深度值是否正确计算
- [ ] testAndSet 是否正确返回 true
- [ ] 禁用深度测试后是否显示

---

### 阶段 3：完整流程

#### 3.1 端到端验证

**目标**：逐步跟踪数据流

```
Vertex.position (本地坐标)
    ↓
MVP * position (裁剪坐标)
    ↓
NDC (归一化设备坐标)
    ↓
Screen (屏幕坐标)
    ↓
BBox (包围盒)
    ↓
Barycentric (重心坐标)
    ↓
Depth Test (深度测试)
    ↓
Fragment Shader (片段着色)
    ↓
FrameBuffer (颜色缓冲)
    ↓
SDL Texture (纹理)
    ↓
Screen (屏幕)
```

---

## 当前已知状态

| 步骤 | 状态 | 说明 |
|------|------|------|
| 顶点数据 | ✅ 正确 | v0=(-0.5,-0.5,0) |
| MVP 变换 | ✅ 正确 | (-0.1875,-0.25,-1.002,1) |
| 颜色 v0 | ✅ 正确 | (1,1,0) 黄色 |
| 屏幕尺寸 | ✅ 正确 | 640x480 |
| Non-black 像素 | ❌ 错误 | 0 / 307200 |

---

## Buffer 渲染流程

### 渲染流程图

```
Vertex 数据
    │
    ▼
┌─────────────────────────┐
│      VertexShader       │  ← MVP 变换，输出 clip 坐标
│  (Pipeline/VertexShader)│
└───────────┬─────────────┘
            │
            ▼ (裁剪坐标)
┌─────────────────────────┐
│       Rasterizer        │  ← 光栅化，填充三角形
│   (Pipeline/Rasterizer) │
│                         │
│  1. NDC → Screen       │
│  2. 计算包围盒          │
│  3. 重心坐标插值         │
│  4. 深度测试            │
└───────────┬─────────────┘
            │
            ▼ (片段着色输入)
┌─────────────────────────┐
│     FragmentShader      │  ← 光照计算，输出颜色
│ (Pipeline/FragmentShader)│
└───────────┬─────────────┘
            │
            ▼ (Color)
┌─────────────────────────┐
│       FrameBuffer       │  ← 颜色缓冲，存储 RGBA 像素
│     (Buffer/FrameBuffer)│
└───────────┬─────────────┘
            │
            ▼ (像素数组)
┌─────────────────────────┐
│    SDL_CreateSurface    │  ← 创建 SDL Surface
└───────────┬─────────────┘
            │
            ▼ (SDL_Surface*)
┌─────────────────────────┐
│  SDL_CreateTextureFrom  │  ← 创建 SDL Texture
└───────────┬─────────────┘
            │
            ▼ (SDL_Texture*)
┌─────────────────────────┐
│   SDL_RenderCopy        │  ← 渲染到屏幕
└─────────────────────────┘
```

### 关键文件

| 模块 | 文件 | 职责 |
|------|------|------|
| VertexShader | `Pipeline/VertexShader.hpp` | MVP 变换 |
| Rasterizer | `Pipeline/Rasterizer.hpp` | 光栅化、深度测试 |
| FragmentShader | `Pipeline/FragmentShader.hpp` | 光照计算 |
| FrameBuffer | `Buffer/FrameBuffer.hpp` | 存储颜色像素 |
| Renderer | `Renderer/Renderer.cpp` | 整合以上模块 |

### Buffer 着色位置

**在 Rasterizer 中着色**，具体在 `Rasterizer::rasterizeTriangle` 函数里：

```cpp
// Pipeline/Rasterizer.hpp
void rasterizeTriangle(const VertexOut& v0, const VertexOut& v1, const VertexOut& v2,
    std::function<Color(const VertexOut&)> fragmentShader) {

    // ... 计算屏幕坐标、包围盒、重心坐标 ...

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            // 1. 计算重心坐标
            Vector3 bary = computeBarycentric(p, s0, s1, s2);

            if (bary.x >= -EPSILON && bary.y >= -EPSILON && bary.z >= -EPSILON) {
                // 2. 插值片段属性
                VertexOut interpolated = ...;

                // 3. 深度测试
                if (m_depthBuffer->testAndSet(x, y, depth)) {
                    // 4. 片段着色
                    Color color = fragmentShader(interpolated);

                    // 5. 写入 FrameBuffer
                    m_frameBuffer->setPixel(x, y, color);
                }
            }
        }
    }
}
```

**着色流程**：
1. 对每个在包围盒内的像素，计算重心坐标
2. 判断像素是否在三角形内
3. 插值片段属性（位置、法线、UV、颜色）
4. 深度测试
5. 调用 FragmentShader 计算颜色
6. 写入 FrameBuffer

---

---

## 结论

目前数据流到 MVP 变换都是正确的，但最终没有像素写入。可能问题在：
1. 光栅化器的屏幕坐标计算
2. 深度测试
3. FragmentShader 的光照计算
4. FrameBuffer → SDL 纹理的转换
