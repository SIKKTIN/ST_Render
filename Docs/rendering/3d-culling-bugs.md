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

### 性能补救：trivial-reject

为了让相机走到 cube 表面 / 进入 cube 内部时仍然能跑得动，加一层 **trivial-reject frustum culling**：

```cpp
// 三个顶点都"完全在视锥外"才剔掉。
// 单顶点 `w <= 0`（在相机后面）视为"穿越视锥" → 保留，由 rasterizer 自己处理。
bool triviallyOutside = true;
for (auto* vs : {&v0, &v1, &v2}) {
    float w = vs->position.w;
    if (w <= 0.0f) { triviallyOutside = false; break; }
    if (vs->position.x > -w && vs->position.x < w &&
        vs->position.y > -w && vs->position.y < w &&
        vs->position.z > -w && vs->position.z < w) {
        triviallyOutside = false;
        break;
    }
}
if (triviallyOutside) continue;
```

为什么这样能省事：
- 进入 cube 内部时，背向相机的 3 个三角形顶点都在 near plane 外侧 → trivial reject 干掉
- "贴脸" 时三角形顶点 x/y 跨度极大、`w` 接近 0 或翻转 → 大多数三角形 trivially outside → rasterizer 不再扫整张屏

和之前"Bug B" 的区别：
- Bug B：要求 3 顶点**全在内**才保留 → 把可见三角形误剔
- 这里：要求 3 顶点**全在外**才剔 → 只剔绝对不可见的，不会误剔可见三角形

---

## 进入 mesh 内部该怎么显示

`createCube()` 的 mesh 只有 6 个面、12 个三角形，**都是单面的外表面**。从外面看是正常的实心 cube；但从里面看，**所有可见三角形都被 back-face culling 干掉了**——屏幕全空。

### 两种通用方案

| 方案 | 实现 | 优点 | 缺点 |
|---|---|---|---|
| **(A) 每个面建双面三角形**（cube → 24 tri） | mesh 加内壁三角形，法线反向 | 物理上正确；两个方向都正确 cull | 三角形数 × 2；需要按材质 / 纹理区分内外 |
| **(B) 动态切换 culling 方向**（当前实现） | 检测 camera 是否在 mesh 内部，是则翻转 dot 判断 | mesh 不变；外看内看都用同一份三角形 | 只对**凸** mesh 严格正确；mesh 中心 / 包围球要算对 |

### 实现（方案 B）

```cpp
// 一次算出 mesh center + 包围球半径
ST::Vector3 meshCenter(0, 0, 0);
for (auto& v : verts) meshCenter = meshCenter + v.position;
meshCenter = meshCenter * (1.0f / verts.size());

ST::Vector3 meshCenterWorld = (model * ST::Vector4(meshCenter, 1.0f)).toVector3();
float boundingRadius = 0.0f;
for (auto& v : verts) boundingRadius = std::max(boundingRadius, (v.position - meshCenter).length());

// camera 在 mesh 内部 ↔ |camera - center| < radius
bool cameraInside = (meshCenterWorld - m_eye).length() < boundingRadius;

// culling 时翻转 dot 判断
float visibilityDot = faceNormal.dot(toEye);
if (cameraInside) visibilityDot = -visibilityDot;
if (visibilityDot <= 0.0f) continue;
```

为什么翻转：
- 相机在 cube 外：faceNormal 朝外，`toEye` 朝相机 → dot > 0（面朝相机）保留；< 0 剔
- 相机在 cube 内：faceNormal 仍然朝外（mesh 没改），但 `toEye` 现在指向**内部** → dot 全部 < 0 → 全剔
- 把 dot 翻转 → dot > 0 变成 dot < 0 保留（这些是面"最不朝外"的面 —— 即离相机最近的内壁）

### 局限性

- **只对凸 mesh 严格成立**。凹 mesh（带洞、互相穿入）会有"相机在 mesh 内的局部空洞"——包围球判定说"在内"，但其实相机在凸包内部 + 实际 mesh 的空洞里。处理这种需要空间分割或 per-mesh-region 判定，超出本仓库范围。
- 用包围球判断 inside/outside 比 AABB 准，但对很扁的 mesh（圆盘）会出现"包围球判定说在内、实际在外"。当前 cube 是 unit cube，球 / AABB 等价，没问题。
- 对**会变形**（骨骼动画、morph target）的 mesh，center / radius 需要每帧重算。本仓库不涉及。

### 另一个备选方案：Sutherland-Hodgman near-plane clipper

要彻底解决"贴脸 + 内部"的画面瑕疵，**真正的修复**是在 `Rasterizer::rasterizeTriangle()` 入口加 **Sutherland-Hodgman clipper**，把穿越 near plane 的三角形 split 成最多 2 个三角形，让所有要画的三角形都在视锥内。这样：

- 进入 cube 内部时不会出现"三角形一部分在 near plane 内、一部分在外"的诡异穿帮
- trivial-reject 退化成"完全在视锥外的三角形剔掉"（已经做到）

本仓库当前**不实现** —— 12 个三角形的小 demo 视觉上看不出区别，工作量大。

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
| `src/renderer/geometry/Mesh.cpp` | `Mesh::createCube()` | 顶点顺序 CCW——已修正（见后文） |

---

## 后续修复：near-plane clipper + cube winding

修完 Bug A / B 之后，还观察到一组**新症状**：

| 症状 | 触发条件 |
|---|---|
| 鼠标拖动 yaw 经过某些角度时，整屏几乎被同一个 cube 颜色覆盖（"看向后方看到一个正方形"） | yaw ≈ 0.67π, 1.67π（eye 朝向接近 cube 背面） |
| 鼠标横向 / 纵向操作"看起来反了" | 同样的角度区间 |
| 在 canvas 中旋转 90° 前后，cube 突然消失 | yaw ≈ ±π/2 |

### 真凶

这两组症状实际上由**两个独立的根因**叠加而成，写完 Bug A / B 之后用 `ST_Render_Dump` 离线扫描 yaw sweep 才暴露出来。

#### 1. 没有近裁面 clipper

`drawMesh` 通过 back-face culling + trivial-reject 后直接调 `Rasterizer::rasterizeTriangle(v0, v1, v2)`。当三角形部分顶点在 near plane 后（`position.w <= 0`）、部分顶点在 near plane 前（`w > 0`）时：

- 透视除法 `position.x / w` 在 `w <= 0` 的顶点上**反号**，三角形在屏幕上映射到错误位置。
- **bounding box** 计算（min/max of `s0.x, s1.x, s2.x`）会包含 ±∞ 量级的 NaN/Inf 数值，被 `std::max(0, ...)` / `std::min(W-1, ...)` clamp 成 **整个 viewport**——rasterizer 扫遍整屏。
- 透视校正深度（`1/w`）也 invalid，深度测试时会出现诡异的"提前挡住"或"挡不住"。

结果：**整个 viewport 会被一两个坏三角形覆盖成一个色块**——这正是用户描述的"看向后方看到一个正方形"。"操作颠倒"也是同一回事：cube 被错误地镜像绘制到屏幕的另一侧，**看起来**是 WASD 转错了方向。

**修复**：在 `src/renderer/pipeline/VertexShader.hpp` 加 `clipTriangleAgainstNearPlane(v0, v1, v2, emitBuf, emitCount)`，Sutherland-Hodgman 单边裁剪。`drawMesh` 在 back-face culling 之后调用它，分裂出 1-2 个全 `w > 0` 的子三角形，再分别 rasterize。clipper 输出 `emitCount == 0`（全剔）/`3`（单三角形）/`4`（quad → 拆成 2 个三角形）。

#### 2. Cube 顶点 winding 是 CW 不是 CCW

`Mesh::createCube` 的 face indices 是按**CW from outside view** 排序的，结果 `(v1 - v0) × (v2 - v0)` 算出来的"法线"全部指向 cube **内部**。

为什么 yaw=0 时还能看见 cube？—— back-face culling 用 `dot(faceNormal, toEye) > 0` 保留；当 faceNormal 朝内、camera 朝 -Z 时，`toEye` 朝 +Z，`dot(+Z, +Z) = +1 > 0` 错误地保留 back-facing 三角形。Bug A 修过之后（用 `faceNormal.dot(toEye)`），这个隐藏的"巧合抵消"就不存在了——结果就是某些 yaw 角度 cube 整面不见。

**修复**：把 12 个 face 的 indices 全部改成 CCW from outside view，让计算出的 `faceNormal` 真的是外法线。`Mesh::createCube` 加注释指明每个面顶点的 CW/CCW 约定。`ST_Render_Dump` 离线工具的 "Reference face normals" 输出可以用来验证。

### 修复后 sweep 结果

`ST_Render_Dump --sweep --sweep-n=12 --w=120 --h=90`（默认相机在 `(1.6, 1, 2.5)`，pitch=0.35）：

| yaw (rad) | 修复前 cube 像素 | 修复后 |
|---|---|---|
| 0.00π | 1215 (11%) | 1215 (11%) ✓ |
| 0.50π | **0** (cube 在视野外) | 0 ✓ |
| 0.67π | **9725 (90% 全屏覆盖)** | **0** ✓ |
| 1.67π | **10800 (100% 全屏覆盖)** | **0** ✓ |

**"整屏被填满" 的两个异常角度都被消除**，但 yaw ∈ [0.5π, 1.83π] 仍然是 0 像素——这是因为 fly camera 加上 cube 在固定 origin、相机在原点外时，yaw 转过 90° 后 cube 就几何上跑出水平 FOV 外（60° 垂直 × 4:3 aspect → 水平半 FOV ≈ 37.5°）。这不是 bug，**是 fly camera + 固定目标的几何特性**。如果想让 cube 始终在视野里，可以：
- 加大 `near` 附近的 FOV（比如 90°）
- 缩短相机距 cube 距离
- 切回 orbit camera（自动跟随 yaw 调整目标点）

### 新增的离线工具

`src/app/main/test_3d_dump.cpp`（target `ST_Render_Dump`）：

- `--sweep --sweep-n=N`：从 yaw=0 扫到 yaw=2π，每隔 2π/N 取一个角度，跑 cube 渲染，打印每帧 cube 像素数 + 屏幕 bbox + 屏幕中心。
- `--yaw=... --out=path.ppm`：单帧渲染，存为 PPM，方便离线看 image 工具检查。
- `--w=W --h=H`：覆盖默认 640x480，可用来在不同 aspect ratio 下复测。
- 每次 dump 都附一个 "Triangle visibility audit"：12 个三角形每个打印 back-face culling + near-plane clipper 的取舍（`K` = kept，`c` = culled，`X` = clip drop，`Q` = quad split），用来快速看出"三角形被什么步骤干掉"。

### 修改清单

| 文件 | 改动 |
|---|---|
| `src/renderer/pipeline/VertexShader.hpp` | 新增 `lerpVertexOut` + `clipTriangleAgainstNearPlane` |
| `src/renderer/geometry/Mesh.cpp` | `Mesh::createCube` 12 个 face indices 改为 CCW from outside |
| `src/app/main/TestModule_3DRender.cpp::drawMesh` | trivial-reject 改为"全外才剔"；back-face culling 之后调 near-plane clipper；每个 clip 子三角形单独 rasterize |
| `src/app/main/CMakeLists.txt` | 新增 `ST_Render_Dump` 离线诊断 target |
| `src/app/main/test_3d_dump.cpp` | 新增离线 dump + sweep 工具 |
| `docs/rendering/3d-culling-bugs.md` | 本节新增 |

---

## 教训 / 备忘

- **Back-face culling 不要在屏幕 / NDC 空间做 2D 叉乘**。一来 viewport / Y-flip 让符号不直观，二来透视投影的非线性变换让"屏幕 CCW ≠ 几何 CCW"在边界处不一致。用世界空间面法线 vs `toEye` 是最不依赖约定的写法。
- **顶点级 frustum culling 反模式**：要求"3 顶点全在视锥内才保留"——会把跨越视锥边界的可见三角形误剔。这是过去 Bug B 的根因。
- **Trivial-reject frustum culling 是反模式的修正版**：要求"3 顶点全在视锥外才剔"——只剔绝对不可见的，不误剔可见三角形。**且这是性能优化的合理手段**：对贴脸 / 进入 mesh 内部这种三角形 `w` 翻转 / 跨越 near plane 的情况，能跳过大量 rasterize 调用。
- 写 culling 时**先彻底关闭它**验证物体本身能渲染，再"加一层看是否还能渲染"，可以快速二分定位。