# 3D 剔除（Culling）两个连环 Bug

记录 `TestModule_3DRender` 中两个相互独立的剔除错误，它们叠加产生了"移动相机后整个方块消失"和"近看时整面丢失"两种症状。

---

## 现象

打开 **3D Render** 模块，看到的不是 cube，而是 clear color（深蓝灰）背景：

- **远看**：cube 大致轮廓可见（其实只是 clear color，cube 完全没渲染出来）
- **走近 / 转动视角**：cube 直接消失
- **贴近 cube 表面 / 走进 cube 内部**：某些面突然整面不见，另一些角度又"看见"部分

两条症状背后是两个不同机制：

| 症状 | 触发条件 | 真凶 |
|---|---|---|
| 任何视角都看不到 cube | 永远发生 | Bug A — back-face culling 符号反了 |
| 走近时整面消失、看不见 | 相机到 cube 距离小 / 进入 cube 内部 | Bug B — frustum culling 用 3 顶点全内测 |

---

## Bug A — Back-face culling 的符号反了

### 原始代码

`src/app/main/TestModule_3DRender.cpp::drawMesh()`：

```cpp
ST::Vector3 n0 = v0.toNDC();
ST::Vector3 n1 = v1.toNDC();
ST::Vector3 n2 = v2.toNDC();
float cross = (n1.x - n0.x) * (n2.y - n0.y) - (n2.x - n0.x) * (n1.y - n0.y);
if (cross >= 0.0f) continue; // back-facing or degenerate
```

### 为什么是错的

`toNDC()` 返回数学 NDC（+Y 向上）。`Rasterizer::toScreenSpace()` 在写入 framebuffer 前翻转 Y：

```cpp
return Vector2(
    m_viewportX + (ndc.x + 1.0f) * 0.5f * m_viewportWidth,
    m_viewportY + (1.0f - ndc.y) * 0.5f * m_viewportHeight  // ← Y 翻转
);
```

在 +Y 向上的 NDC 空间里：

- CCW 几何 → 屏幕 CCW 投影 → NDC 2D 叉乘 `cross > 0`
- CW 几何 → 屏幕 CW 投影 → `cross < 0`

OpenGL 约定 **前向三角形 = CCW**。所以"该剔除"的 CW 三角形对应 `cross < 0`，**`cross >= 0`** 是**前向**三角形。

代码保留 `cross < 0`（背面），剔除 `cross >= 0`（正面）—— 符号反了。结果：

- 渲染出来的全是**背向相机的面**
- 走 depth test 时，背面被前面的背面/clear color 覆盖，屏幕上**完全看不见 cube**
- 看到的"cube 轮廓"其实是 clear color 背景，不是 cube

### 修复

不依赖屏幕 / NDC 约定，改用世界空间 dot product：

```cpp
ST::Vector3 faceNormal = (v1.worldPosition - v0.worldPosition)
                            .cross(v2.worldPosition - v0.worldPosition);
ST::Vector3 toEye = m_eye - v0.worldPosition;
if (faceNormal.dot(toEye) <= 0.0f) continue; // back-facing → skip
```

### 为什么用世界空间而不是物体空间

- `modelMatrix` 可能是任意旋转/平移/缩放。物体空间的法线和物体空间的 `toEye` 不一定对应正确的世界几何关系。
- `VertexOut.worldPosition` 已经由 `VertexShader::transformVertex()` 算好（`model * position`），零额外开销。
- 法线方向 `(v1-v0) × (v2-v0)` 在顶点已变换到世界空间后也是世界空间向量。

### 调试期间临时禁用

调试期把整个 back-face culling 块注释掉（保留 TODO 注释便于回滚）：

```cpp
// [TEMP] Back-face culling disabled for debugging.
// ST::Vector3 faceNormal = (v1.worldPosition - v0.worldPosition)
//                             .cross(v2.worldPosition - v0.worldPosition);
// ST::Vector3 toEye = m_eye - v0.worldPosition;
// if (faceNormal.dot(toEye) <= 0.0f) continue;
```

---

## Bug B — Frustum culling 用了"3 顶点全在内"的粗筛

### 原始代码

紧跟 Bug A 之后：

```cpp
if (!m_vertexShader.isInFrustum(v0)) continue;
if (!m_vertexShader.isInFrustum(v1)) continue;
if (!m_vertexShader.isInFrustum(v2)) continue;
```

`isInFrustum`（`src/renderer/pipeline/VertexShader.hpp`）做严格 clip-space 内测：

```cpp
bool isInFrustum(const VertexOut& v) {
    float w = v.position.w;
    return (
        v.position.x >= -w && v.position.x <= w &&
        v.position.y >= -w && v.position.y <= w &&
        v.position.z >= -w && v.position.z <= w
    );
}
```

### 为什么是错的

"3 顶点全在内"是**三角形级**判断被错用成**单顶点级**。

- 三角形只要**任意一个**顶点在视锥外就**整面剔掉**
- 实际正确的"粗筛"是"3 顶点**全部**在外面"才剔除（看三角形 AABB）
- 真实工程里 80% 的情形用 **per-pixel depth test** + 屏幕边界 clamp 即可，**根本不写顶点级 frustum culling**

### 为什么"走近才坏"

| 相机位置 | 三角形顶点的 clip-space 分布 | 后果 |
|---|---|---|
| 远（距离 ~3） | 整面都在视锥内 | 12 面都过 |
| 近（紧贴 cube） | 一个三角形 1-2 顶点 w 接近 0，z 越过 -w | 一个顶点 fail → 整面 fail |
| 进入 cube 内部 | 顶点 w 正负号不一致 | 几乎所有三角形 fail → 全空 |

这正是"近看消失 / 走到里面消失"的来源。

### 修复

**移除顶点级 frustum culling**。让 Rasterizer 自己的屏幕边界 clamp + per-pixel depth test 处理。

```cpp
// No per-triangle frustum culling here. Earlier we used
// vertexShader.isInFrustum on all 3 vertices (kept only if all 3 inside),
// which silently dropped triangles that straddled the near plane or the
// edges of the viewport while the camera was close to the cube -- the
// whole face disappeared even though it should have been partially
// visible. Without a real Sutherland-Hodgman clipper in the rasterizer
// the cheapest fix is to skip the coarse reject and let the per-pixel
// depth test + the rasterizer's screen-bounds clamp handle boundary
// cases.
```

### 已知遗留问题：近裁面穿帮

删除顶点 frustum culling 后，**穿越 near plane 的三角形** 会画过近裁面区域 —— 即三角形一半在屏幕内、一半在 near plane 外，rasterizer 不做 clip，会画出**残余像素**。

对于 12 个三角形的小 demo cube 这基本看不出。如果后续渲染复杂 mesh，需要在 `Rasterizer::rasterizeTriangle()` 之前加 **Sutherland-Hodgman clipper**（沿 near plane 把三角形 split 成最多 2 个）。本仓库当前不实现。

---

## 调试步骤（如果再发生类似症状）

1. **彻底关掉 back-face culling**，看物体是否可见
   - 不可见 → bug 在 frustum culling 或更上游（vertex / projection）
   - 可见 → 继续 2
2. **彻底关掉 frustum culling**（`isInFrustum` 那段删掉）
   - 还不可见 → bug 在 vertex shader、projection matrix、framebuffer clear、depth buffer clear
   - 可见 → 就是两个 culling 中至少一个坏了
3. **单独启用 back-face culling**（用世界空间 dot product 版本），看面是否正确被剔
4. **单独启用 frustum culling**，但改成 "全外才剔除"（trivially-reject）
   - 仍能用 → 问题就是"全内才保留"的反向逻辑
5. **加 Sutherland-Hodgman near-plane clipper**，彻底根治近裁面穿帮

---

## 相关代码位置

| 文件 | 行 / 函数 | 角色 |
|---|---|---|
| `src/app/main/TestModule_3DRender.cpp` | `drawMesh()` | Bug A + Bug B 都在这里 |
| `src/renderer/pipeline/VertexShader.hpp` | `isInFrustum()` | Bug B 的判断函数本身没动，但调用方式错 |
| `src/renderer/pipeline/Rasterizer.hpp` | `toScreenSpace()` | 解释了为什么 NDC 叉乘符号错了（Y 翻转） |
| `src/renderer/geometry/Mesh.cpp` | `Mesh::createCube()` | 顶点顺序 CCW，正确，没动 |

---

## 教训 / 备忘

- **Back-face culling 不要在屏幕 / NDC 空间做 2D 叉乘**。一来 viewport / Y-flip 让符号不直观，二来透视投影的非线性变换让"屏幕 CCW ≠ 几何 CCW"在边界处不一致。用世界空间面法线 vs `toEye` 是最不依赖约定的写法。
- **顶点级 frustum culling 是反模式**，除非三角形数量巨大（如粒子、植被）否则不写。Per-pixel depth + 屏幕 clamp 已经够。
- 写 culling 时**先彻底关闭它**验证物体本身能渲染，再"加一层看是否还能渲染"，可以快速二分定位。