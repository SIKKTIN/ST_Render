# TextureManager 架构文档

## 一、定位与职责

TextureManager 是引擎的**全局纹理资源中心**，采用单例模式，在程序启动时自动扫描 `Resource/` 目录，将所有图片文件加载为 `Texture2D` 存于内存，供整个引擎随时查询和复用。

核心职责只有三件事：
1. **启动时扫描** `Resource/` 文件夹
2. **按需加载**图片为 `Texture2D`
3. **按索引 / 按文件名** 查找纹理

---

## 二、核心数据结构

```cpp
struct TextureInfo {
    std::string filename;    // 文件名（不含路径），如 "player.png"
    std::string fullPath;    // 完整路径，如 "E:/.../Resource/player.png"
    Texture2D   texture;     // 实际纹理数据（CPU 端 RGBA 像素数组）
};

class TextureManager {
    std::vector<TextureInfo>              m_textures;       // 按加载顺序排列
    std::unordered_map<std::string, int> m_nameToIndex;    // 文件名 → 索引的哈希表
};
```

两个容器各司其职：
- `m_textures` — 主存储，按索引随机访问 O(1)
- `m_nameToIndex` — 哈希索引，按文件名查找纹理 O(1)，避免字符串比较遍历

---

## 三、扫描流程（`scanResourceFolder`）

```
程序启动
  └─ TextureManager::getInstance().scanResourceFolder()
        │
        ├─ SDL_GetBasePath()  →  获取 exe 所在目录
        │     └─ basePath + "Resource"  →  资源目录
        │
        ├─ Win32 FindFirstFileA("Resource/*")  →  枚举目录
        │
        ├─ 过滤扩展名 { .jpg, .jpeg, .png, .bmp, .tga }
        │
        ├─ ST::Image::load(fullPath)  →  stb_image 读取
        │     ├─ stbi_load()  →  原始字节 (R,G,B unsigned char)
        │     ├─ 字节 → float Color 归一化（÷255）
        │     └─ 上下翻转（stb_image 默认左下角原点，转 SDL/ImGui 坐标系）
        │
        ├─ Texture2D::setPixels(pixels, w, h)  →  存入 m_textures
        ├─ m_nameToIndex[fname] = index
        └─ 输出到 OutputDebugString / cout
```

**关于上下翻转：** `stb_image` 输出像素以左上角为原点（标准图片格式约定），而 SDL 渲染以左下角为原点。`ST_Image::load` 在加载时主动做了 Y 轴翻转，保证纹理采样方向和屏幕空间一致：

```cpp
int dst = y * w;          // 目标行（从上往下写）
int src = (h - 1 - y) * w; // 源行（从下往上取）
m_pixels[dst + x] = Color(data[(src + x)*3 + 0] / 255.0f, ...);
```

---

## 四、公开接口

| 方法 | 返回值 | 说明 |
|---|---|---|
| `getInstance()` | `TextureManager&` | 获取单例（内部 `static` 构造） |
| `scanResourceFolder()` | `void` | 重新扫描 `Resource/` 目录，清空旧数据后重新加载 |
| `getTextureCount()` | `int` | 已加载纹理数量 |
| `getTexture(idx)` | `Texture2D*` | 按索引获取纹理指针，超出范围返回 `nullptr` |
| `getTextureName(idx)` | `const std::string&` | 按索引获取文件名，超出范围返回空字符串 |
| `findTexture(filename)` | `int` | 按文件名查找索引，未找到返回 `-1` |
| `getAllTextures()` | `const vector<TextureInfo>&` | 获取全部纹理信息（用于构建纹理浏览器缩略图） |

---

## 五、与引擎各部分的交互

### 5.1 程序启动 — `test_manager_main.cpp`

```cpp
// 仅调用一次，之后全局可用
ST::TextureManager::getInstance().scanResourceFolder();
```

### 5.2 Sprite2D 绑定纹理 — `Sprite2D.cpp`

```cpp
void Sprite2D::setTextureIndex(int idx) {
    Texture2D* src = TextureManager::getInstance().getTexture(idx);
    if (src) {
        m_texture = *src;                        // 复制像素数据
        m_textureName = TextureManager::getInstance().getTextureName(idx);
    }
    m_textureIndex = idx;
}
```

注意这里是**值复制**（`m_texture = *src`），`Sprite2D` 持有一份独立的纹理副本，修改不影响 `TextureManager` 中的原始数据。这允许同一个纹理被多个精灵复用。

### 5.3 纹理浏览器缩略图 — `TestModule_2D_Scene.cpp` / `TestModule_Sprite2D.cpp`

```cpp
const auto& textures = ST::TextureManager::getInstance().getAllTextures();
for (const auto& texInfo : textures) {
    if (!texInfo.texture.isValid()) {
        m_thumbnails.push_back(nullptr);
        continue;
    }
    // 构建 SDL_Texture 缩略图（96x96），存入 m_thumbnails
    // 用于纹理浏览器弹出窗口
}
```

### 5.4 纹理采样 — 软件渲染管线

渲染时 `FragmentShader` 持有 `std::vector<Color> m_texture`，片段着色器中通过：

```cpp
Color sample = m_fragmentShader.m_texture[idx];
```

来访问纹理像素，由 `Texture2D::sample(u, v)` 执行实际的 UV 查找和滤波。

---

## 六、图像加载后端 — `ST_Image`

```cpp
class Image {
    std::vector<Color> m_pixels;  // float RGBA，归一化到 [0,1]
    int m_width, m_height, m_channels;

    bool load(const char* path) {
        stbi_set_flip_vertically_on_load(true);  // 第二次翻转，但被手动翻转覆盖了
        unsigned char* data = stbi_load(path, &w, &h, &ch, 3);  // 强制 3 通道
        // ... 手动 Y 轴翻转 + uchar→float 转换
    }
};
```

**注意：** `stbi_load(path, &w, &h, &ch, 3)` 强制只读取 RGB（丢弃 Alpha），这意味着所有加载的纹理 Alpha 通道固定为 1.0。若需要透明度支持，应改为 `stbi_load(path, &w, &h, &ch, 4)` 以读取 RGBA。

---

## 七、设计要点

1. **单例模式** — `static TextureManager instance` 在首次调用 `getInstance()` 时才构造，懒加载
2. **值复制语义** — `Sprite2D` 复制 `Texture2D` 而非共享指针，允许同名纹理被多个精灵独立使用
3. **哈希表加速查找** — `m_nameToIndex` 将文件名查找从 O(n) 降到 O(1)
4. **平台特定实现** — 使用 Win32 API `FindFirstFileA` 枚举目录（仅限 Windows），加载使用跨平台的 `stb_image`
5. **两次翻转** — `stbi_set_flip_vertically_on_load(true)` 后手动翻转是冗余的，实际效果为未翻转，需注意未来 stb_image 的行为变化可能破坏此逻辑

---

## 八、相关文件

| 文件 | 作用 |
|---|---|
| `Editor/TextureManager.hpp` | 接口定义 + `TextureInfo` 结构体 |
| `Editor/TextureManager.cpp` | 单例实现 + 目录扫描 + 加载逻辑 |
| `Texture/ST_Image.hpp` | `Image` 类：`stb_image` 封装，字节→Color 转换 |
| `Texture/ST_Image.cpp` | `#define STB_IMAGE_IMPLEMENTATION` 一次性实现 |
| `Buffer/Texture2D.hpp` | CPU 端纹理：像素存储 + `sample()` 采样 |
| `Buffer/Texture2D.hpp` | 同上 |
