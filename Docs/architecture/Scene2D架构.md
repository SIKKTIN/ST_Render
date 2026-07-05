# Scene2D 架构文档

## 一、定位与职责

Scene2D 是引擎的**场景对象管理层**，负责管理场景中的所有可见对象（GameObject / Sprite2D），提供创建、销毁、查找、排序等生命周期管理，同时封装 2D 视口的平移、缩放和世界坐标↔屏幕坐标转换。

Scene2D 本身不负责渲染（渲染由 `TestModule_2D_Scene` 驱动），只负责组织和查询对象数据。

---

## 二、核心数据结构

### 2.1 Scene

```cpp
class Scene {
    std::string name;                              // 场景名称
    Camera camera;                                 // 3D 相机（正交/透视）
    Vector3 viewportOffset;                        // 2D 视口平移偏移
    float viewportZoom;                            // 2D 视口缩放
    std::vector<std::unique_ptr<GameObject>> objects;  // 场景对象（独占拥有权）
    ITransformUI* m_transformUI = nullptr;         // 变换编辑 UI（可插拔策略）
};
```

- `viewportOffset` + `viewportZoom` 构成一套独立的 2D 视口系统，与 `Camera` 的 3D 投影分离
- `objects` 是 `unique_ptr` 向量，Scene 持有独占所有权，`GameObject*` 指针仅作为查找/操作句柄
- `ITransformUI` 是策略模式：不同 UI 框架（ImGui / 自研）可以替换实现

### 2.2 GameObject

```cpp
class GameObject {
    std::string  name;           // 对象名称（用于 findGameObject）
    Vector3      position;       // 世界坐标位置
    Vector3      rotation;       // 弧度绕 X/Y/Z 轴旋转（Z 轴优先用于 2D 平面）
    Vector3      scale;          // X/Y/Z 缩放，Z 通常为 1.0
    ShapeType   shapeType;      // None / Quad / Cube / Circle / Triangle / Line
    Color       color;           // 基础颜色（RGBA float）
    Color       tint;            // 叠加染色（与 color 相乘）
    int         sortingOrder;    // 渲染优先级：越大越靠前（后画，覆盖前面的）
    bool        showAABB;        // 是否绘制轴对齐包围盒（调试用）
    Mesh        m_mesh;          // 网格数据（顶点 + 索引）
    Texture2D*  m_texture;       // 可选纹理指针（不拥有）
    bool        m_dirty;         // dirty flag：颜色改变后标记，下次渲染前重建网格
};
```

默认值：
```
position = (0, 0, 0)
rotation = (0, 0, 0)
scale    = (1, 1, 1)
color    = (1, 1, 1, 1)（白，不着色）
tint     = (1, 1, 1, 1)
sortingOrder = 0
showAABB = false
shapeType = None
m_texture = nullptr
m_dirty   = false
```

### 2.3 Sprite2D — 继承 GameObject

```cpp
class Sprite2D : public GameObject {
    Texture2D   m_texture;       // 拥有的纹理数据副本（独立于 TextureManager）
    std::string m_textureName;   // 纹理文件名
    int         m_textureIndex;  // 在 TextureManager 中的索引
    bool        m_flipX = false; // UV 水平翻转
    bool        m_flipY = false; // UV 垂直翻转
};
```

与 `GameObject` 的 `m_texture`（指针，不拥有）不同，`Sprite2D` 持有一份 `Texture2D` **值副本**，允许多个精灵独立修改同一纹理的局部状态（如 flip）而不互相影响。

---

## 三、生命周期管理

### 3.1 创建

```cpp
GameObject* Scene::createGameObject(name) {
    auto obj = std::make_unique<GameObject>(name);
    GameObject* ptr = obj.get();
    objects.push_back(std::move(obj));  // unique_ptr 移交所有权
    return ptr;                         // 返回原始指针作为句柄
}

Sprite2D* Scene::createSprite2D(name) {
    auto obj = std::make_unique<Sprite2D>(name);
    Sprite2D* ptr = obj.get();
    objects.push_back(std::move(obj));
    return ptr;
}
```

返回裸指针作为操作句柄，但不转移所有权。调用者通过 `Scene::destroyGameObject()` 归还所有权。

### 3.2 销毁

```cpp
void Scene::destroyGameObject(GameObject* obj) {
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        if (it->get() == obj) {
            objects.erase(it);  // unique_ptr 自动析构
            return;
        }
    }
}
```

O(n) 线性查找，按指针值比较。找到后 `erase` 触发 `unique_ptr` 自动析构。

### 3.3 查找

```cpp
GameObject* Scene::findGameObject(name) {
    for (const auto& obj : objects) {
        if (obj->name == name) return obj.get();
    }
    return nullptr;
}
```

按名称查找，同样是 O(n)。无哈希索引，适合对象数量较少的场景。

---

## 四、渲染排序 — `getSortedObjects`

渲染顺序由 `sortingOrder` 决定：

```cpp
void Scene::getSortedObjects(vector<GameObject*>& out) const {
    out.clear();
    for (const auto& obj : objects)
        out.push_back(obj.get());
    std::stable_sort(out.begin(), out.end(),
        [](const GameObject* a, const GameObject* b) {
            return a->sortingOrder < b->sortingOrder;  // 升序：小在前（被大的覆盖）
        });
}
```

| sortingOrder | 含义 | 渲染时机 |
|---|---|---|
| 小（负数或 0） | 背景层 | 先画 |
| 大（正数） | 前景层 | 后画，叠在前面 |

使用 `stable_sort` 保持相同 `sortingOrder` 对象之间的原始相对顺序。

点击拾取时**逆序遍历** `getSortedObjects` 的结果，先找到最上层物体。

---

## 五、形状系统 — 网格工厂

所有形状以**局部坐标系**生成，中心在 `(0, 0, 0)`，后续通过 `position` / `rotation` / `scale` 应用模型矩阵变换。

### 5.1 Quad（矩形精灵）

```cpp
void GameObject::createQuad(width, height) {
    scale = (width, height, 1);
    rebuildMesh();  // 生成单位矩形再由 scale 缩放
}

// 网格顶点（4 个，2 个三角形 CCW）：
// v0(-0.5,-0.5) UV(0,0)  v1( 0.5,-0.5) UV(1,0)
// v2( 0.5, 0.5) UV(1,1)  v3(-0.5, 0.5) UV(0,1)
// 三角形：v0-v1-v2, v0-v2-v3
// 法线固定 (0,0,1)，适配 2D 平面
```

### 5.2 Cube（3D 立方体）

```cpp
// 6 个面，每面 4 顶点 × 2 三角形 = 24 索引 / 12 三角形
// 每个面有独立法线（-Z, +Z, -X, +X, +Y, -Y）
// UV 按四象限排列：(0,0)(1,0)(1,1)(0,1)
```

### 5.3 Circle（2D 圆）

```cpp
// 扇形三角化：中心 + 32 等分点
// 中心 UV = (0.5, 0.5)
// 边缘点 UV = (cos/sin/scale.x + 0.5, sin/scale.y + 0.5)
// 注意：Y 轴 UV 与屏幕相反（Y 向下），需注意纹理映射方向
```

### 5.4 Triangle（2D 三角形）

```cpp
// 3 个顶点：上顶点(0,0.5)，左下(-0.5,-0.5)，右下(0.5,-0.5)
// UV 对应：(0.5,1)，(0,0)，(1,0)
// 颜色默认红色（由 GameObject::color 决定）
```

### 5.5 Dirty Flag

```cpp
void GameObject::setColor(float r, float g, float b, float a) {
    color = ...;
    m_dirty = true;  // 标记需重建
}
```

`rebuildMesh()` 在网格重建后设置 `m_dirty = false`。当前实现中 dirty flag 在 `createQuad/Cube/Circle/Triangle` 后直接调用 `rebuildMesh()` 立即重建，`setColor` 仅标记但不自动重建——这是潜在的使用陷阱：修改颜色后若没有手动触发重建，旧颜色顶点会保留。

---

## 六、视口系统

Scene 维护一套独立的 2D 视口，与 Camera 的 3D 投影平行：

```
世界坐标系 ──(viewportOffset + viewportZoom)──→ 屏幕像素坐标系
```

### 6.1 世界坐标 → 屏幕坐标

```cpp
Vector3 Scene::worldToScreen2D(worldPos, screenW, screenH) {
    x = (worldPos.x - viewportOffset.x) * viewportZoom + screenW * 0.5f;
    y = (worldPos.y - viewportOffset.y) * viewportZoom + screenH * 0.5f;
    return Vector3(x, y, worldPos.z);
}
```

等价于：先将世界坐标减去视口偏移（平移），再乘以缩放（缩放），最后加上屏幕半宽/半高（居中）。

### 6.2 2D 平移

```cpp
void Scene::pan2D(dx, dy) {
    viewportOffset.x -= dx / viewportZoom;  // 像素位移 ÷ zoom = 世界空间位移
    viewportOffset.y -= dy / viewportZoom;
}
```

### 6.3 中心缩放

```cpp
void Scene::zoom2D(factor) {
    viewportZoom *= factor;
}
```

### 6.4 鼠标位置缩放（保支点）

```cpp
void Scene::zoom2DAt(factor, screenX, screenY, screenW, screenH) {
    // 缩放前：鼠标位置对应的世界坐标
    worldBefore.x = (screenX - screenW*0.5f) / viewportZoom + viewportOffset.x;
    worldBefore.y = (screenY - screenH*0.5f) / viewportZoom + viewportOffset.y;

    viewportZoom *= factor;  // 应用缩放

    // 缩放后：同一鼠标位置对应的世界坐标（已被视口偏移偏移）
    worldAfter.x = (screenX - screenW*0.5f) / viewportZoom + viewportOffset.x;
    worldAfter.y = (screenY - screenH*0.5f) / viewportZoom + viewportOffset.y;

    // 补偿偏移差，使世界点在缩放后仍位于鼠标下
    viewportOffset.x += worldBefore.x - worldAfter.x;
    viewportOffset.y += worldBefore.y - worldAfter.y;
}
```

---

## 七、双视口系统 — Scene vs TestModule_2D_Scene

当前代码中存在**两套并行的视口系统**，设计上有重复：

| 属性 | `Scene` | `TestModule_2D_Scene` |
|---|---|---|
| 平移 | `viewportOffset` (Vector3) | `m_panX`, `m_panY` (float) |
| 缩放 | `viewportZoom` (float) | `m_zoom` (float) |
| 坐标转换 | `worldToScreen2D()` | `screenToWorld()` |
| 保支点缩放 | `zoom2DAt()` | `onWheel()` |
| 平移 | `pan2D()` | `onMouseMove` (drag) |

**实际使用情况：**
- `TestModule_2D_Scene` 的渲染管线完全使用自己的 `m_panX/Y` + `m_zoom`，**不使用** `Scene::viewportOffset/Zoom`
- `Scene::pan2D` / `zoom2D` 系列方法**未被任何模块调用**
- `Scene::worldToScreen2D` 同样**未被使用**

`Scene` 的视口系统目前是**存量的未使用代码**，仅 `Camera` 被渲染管线引用。

---

## 八、变换 UI — 策略模式

```cpp
class ITransformUI {
    virtual bool renderControls(GameObject* obj) = 0;
};

class TransformUI_ImGui : public ITransformUI {
    bool renderControls(GameObject* obj) override;
        ImGui::DragFloat3("Position", ...)
        ImGui::DragFloat3("Rotation", ..., 1.0f, ..., 360.0f)  // 弧度→度数显示
        ImGui::DragFloat3("Scale", ...)
        ImGui::InputInt("Sorting Order", ...)
        ImGui::Checkbox("Show AABB", ...)
};

class Sprite2DUI : public TransformUI_ImGui {
    // 额外添加 Flip X/Y 控件 + 纹理浏览器按钮
};
```

Scene 通过 `setTransformUI(ITransformUI*)` 注入 UI 策略，实现编辑器 UI 与场景数据的解耦。

```cpp
void Scene::renderTransformControls(GameObject* obj) {
    if (m_transformUI) m_transformUI->renderControls(obj);
}
```

---

## 九、与渲染管线的连接

```
Scene / GameObject
  │
  ├─ Scene::getSortedObjects() → 渲染遍历顺序
  ├─ GameObject::getMesh()     → Vertex 数组 → VertexShader
  ├─ GameObject::m_texture     → FragmentShader 纹理查找
  ├─ Sprite2D::m_texture       → 自持纹理副本，优先级更高
  ├─ GameObject::position/rotation/scale
  │      └─ buildModelMatrix() → ModelMatrix → MVP → VertexShader
  │
  └─ Camera::getViewMatrix() / getProjectionMatrix()
         └─ TestModule_2D_Scene::renderScene()
```

渲染时由 `TestModule_2D_Scene` 调用 `buildModelMatrix(obj)` 将 GameObject 的变换应用到顶点：

```cpp
Matrix4x4 buildModelMatrix(const GameObject* obj) {
    T = translation(obj->position.x, obj->position.y, obj->position.z);
    R = rotationZ(obj->rotation.z);  // 2D 主要用 Z 轴旋转
    S = scale(obj->scale.x, obj->scale.y, obj->scale.z);
    return T * R * S;
}
```

---

## 十、场景数据序列化 — SceneData

使用 `nlohmann/json` 库（通过 vcpkg 管理）实现 `.scene` 文件的读写。

### 10.1 依赖

- **vcpkg 包**: `nlohmann-json:x64-windows`
- **CMake**: `find_package(nlohmann_json CONFIG REQUIRED)`
- **链接**: `DataBase` 模块链接 `nlohmann_json::nlohmann_json`

### 10.2 数据流

```
.scene 文件 (JSON)
    ↓ (std::ifstream → json::parse)
nlohmann::json j
    ↓ (j["objects"] 遍历)
Scene.createGameObject / createSprite2D
    ↓ (GameObject::rebuildMesh)
Mesh 数据就绪
```

### 10.3 保存 — saveScene

逐字段手写 JSON 字符串到文件流：

```cpp
fout << "    \"objects\": [\n";
for (size_t i = 0; i < objs.size(); ++i) {
    const GameObject* obj = objs[i].get();
    fout << "        {\n";
    fout << "            \"name\": \"" << escapeString(obj->name) << "\",\n";
    fout << "            \"position\": [" << obj->position.x << ", " << obj->position.y << ", " << obj->position.z << "],\n";
    // ... rotation, scale, shapeType, color, tint, sortingOrder, showAABB
    if (sp) {
        fout << "            \"isSprite\": true,\n";
        fout << "            \"textureIndex\": " << sp->getTextureIndex() << ",\n";
        fout << "            \"flipX\": " << (sp->getFlipX() ? "true" : "false") << ",\n";
        fout << "            \"flipY\": " << (sp->getFlipY() ? "true" : "false") << "\n";
    } else {
        fout << "            \"isSprite\": false\n";
    }
    fout << "        }" << (i + 1 < objs.size() ? "," : "") << "\n";
}
```

### 10.4 加载 — loadScene

用 `nlohmann/json` 的 `>>` 运算符直接解析，然后遍历 `objects` 数组：

```cpp
std::ifstream fin(filePath);
json j;
fin >> j;

scene.name = j["sceneName"].get<std::string>();

for (auto& item : j["objects"]) {
    std::string objName = item["name"].get<std::string>();
    bool isSprite = item["isSprite"].get<bool>();
    GameObject* obj = isSprite ? scene.createSprite2D(objName)
                               : scene.createGameObject(objName);

    // transform
    auto posArr = item["position"];
    obj->position = Vector3(posArr[0], posArr[1], posArr[2]);

    auto rotArr = item["rotation"];
    obj->rotation = Vector3(rotArr[0], rotArr[1], rotArr[2]);

    auto scaleArr = item["scale"];
    obj->scale = Vector3(scaleArr[0], scaleArr[1], scaleArr[2]);

    // shape
    std::string shapeName = item["shapeType"].get<std::string>();
    if (shapeName == "Quad")      obj->shapeType = ShapeType::Quad;
    else if (shapeName == "Cube")  obj->shapeType = ShapeType::Cube;
    else if (shapeName == "Circle") obj->shapeType = ShapeType::Circle;
    else if (shapeName == "Triangle") obj->shapeType = ShapeType::Triangle;
    obj->rebuildMesh();

    // color / tint
    auto colArr = item["color"];
    obj->color = Color(colArr[0], colArr[1], colArr[2], colArr[3]);
    auto tintArr = item["tint"];
    obj->tint = Color(tintArr[0], tintArr[1], tintArr[2], tintArr[3]);

    obj->sortingOrder = item["sortingOrder"].get<int>();
    obj->showAABB = item["showAABB"].get<bool>();

    if (isSprite) {
        Sprite2D* sp = static_cast<Sprite2D*>(obj);
        sp->setTextureIndex(item["textureIndex"].get<int>());
        sp->setFlipX(item["flipX"].get<bool>());
        sp->setFlipY(item["flipY"].get<bool>());
    }
}
```

相比手写解析的优势：

- 零边界处理 bug（括号/字符串转义由库处理）
- 代码量大幅减少，可读性高
- 支持嵌套数组、Unicode 字符串等复杂结构

### 10.5 .scene 文件格式

```json
{
    "sceneName": "MyScene",
    "objects": [
        {
            "name": "YellowQuad",
            "position": [-166, -180, 0],
            "rotation": [0, 0, 0],
            "scale": [200, 200, 1],
            "shapeType": "Quad",
            "color": [1, 1, 0, 1],
            "tint": [1, 1, 1, 1],
            "sortingOrder": 0,
            "showAABB": false,
            "isSprite": false
        },
        {
            "name": "RedCircle",
            "position": [100, 50, 0],
            "rotation": [0, 0, 0],
            "scale": [80, 80, 1],
            "shapeType": "Circle",
            "color": [1, 0, 0, 1],
            "tint": [1, 1, 1, 1],
            "sortingOrder": 1,
            "showAABB": false,
            "isSprite": false
        }
    ]
}
```

---

## 十一、相关文件索引

| 文件 | 作用 |
|---|---|
| `Data/DataBase/SceneData.hpp/.cpp` | .scene 文件序列化（保存/加载） |
| `Editor/Scene.hpp/.cpp` | 场景容器：对象管理 + 视口系统 |
| `Editor/GameObject.hpp/.cpp` | 对象基类：变换、颜色、形状工厂 |
| `Editor/Sprite2D.hpp/.cpp` | 精灵类：继承 GameObject + 自持纹理 |
| `Editor/ITransformUI.hpp` | 变换 UI 抽象接口 |
| `Editor/TransformUI_ImGui.hpp` | ImGui 实现的变换面板 |
| `Editor/Sprite2DUI.hpp` | 精灵专有 UI + 纹理浏览器 |
| `Geometry/Mesh.hpp/.cpp` | 顶点缓冲 + 索引缓冲 |
| `Buffer/Texture2D.hpp` | CPU 端纹理（像素存储 + 采样） |
