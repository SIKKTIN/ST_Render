# 软件渲染器学习路线

## 项目目标

构建一个功能完整的软件渲染器，用于个人学习渲染技术。核心特点是**完全使用 CPU 计算模拟 GPU 渲染管线**，不依赖任何图形 API。

### 长期目标
- ✅ 基础三角形渲染
- ✅ PBR（基于物理的渲染）
- 🔲 光线追踪扩展
- 🔲 路径追踪扩展

---

## 第一阶段：运行第一个三角形

**目标**：搭建最小可运行程序，渲染屏幕上一个彩色三角形。

### 1.1 项目结构设计

```
src/
├── math/                   # 数学基础库
│   ├── MathUtils.hpp/.cpp  # 数学工具（三角函数、夹具等）
│   ├── Vector2.hpp         # 2D 向量
│   ├── Vector3.hpp/.cpp   # 3D 向量
│   ├── Vector4.hpp/.cpp   # 4D 向量（齐次坐标）
│   └── Matrix4x4.hpp/.cpp  # 4x4 矩阵
│
├── geometry/               # 几何图元
│   ├── Vertex.hpp/.cpp    # 顶点结构
│   └── Triangle.hpp       # 三角形结构
│
├── buffers/                # 缓冲区
│   ├── FrameBuffer.hpp/.cpp   # 颜色缓冲
│   └── DepthBuffer.hpp/.cpp    # 深度缓冲（Z-Buffer）
│
├── pipeline/               # 渲染管线
│   ├── VertexShader.hpp/.cpp  # 顶点着色器
│   ├── Rasterizer.hpp/.cpp    # 光栅化器
│   └── FragmentShader.hpp/.cpp # 片段着色器
│
├── transforms/             # 坐标变换
│   └── Transform.hpp/.cpp # MVP 矩阵变换
│
├── utils/                  # 工具类
│   └── ImageWriter.hpp/.cpp # 输出 PNG/BMP 图片
│
├── Camera.hpp/.cpp        # 相机
├── Renderer.hpp/.cpp      # 主渲染器
└── main.cpp               # 程序入口
```

### 1.2 核心模块说明

#### 数学库（math/）

**MathUtils** - 数学工具函数
```cpp
float clamp(float value, float min, float max);
float lerp(float a, float b, float t);
float degreesToRadians(float degrees);
```

**Vector3** - 3D 向量运算
```cpp
class Vector3 {
    float x, y, z;
    Vector3 operator+(const Vector3& v);
    Vector3 operator-(const Vector3& v);
    Vector3 operator*(float scalar);
    float dot(const Vector3& v);
    Vector3 cross(const Vector3& v);
    Vector3 normalized();
    float length();
};
```

**Vector4** - 4D 向量（齐次坐标）
```cpp
class Vector4 {
    float x, y, z, w;
    Vector3 toVector3();  // 透视除法后转为3D
};
```

**Matrix4x4** - 4x4 矩阵
```cpp
class Matrix4x4 {
    float m[4][4];
    Matrix4x4 operator*(const Matrix4x4& mat);
    Vector4 operator*(const Vector4& v);
    static Matrix4x4 identity();
    static Matrix4x4 translation(float x, float y, float z);
    static Matrix4x4 scale(float x, float y, float z);
    static Matrix4x4 rotationX/Y/Z(float angle);
    static Matrix4x4 perspective(float fov, float aspect, float near, float far);
    static Matrix4x4 lookAt(const Vector3& eye, const Vector3& target, const Vector3& up);
};
```

#### 缓冲区（buffers/）

**FrameBuffer** - 像素颜色缓冲
```cpp
class FrameBuffer {
    int width, height;
    std::vector<Color> pixels;  // RGBA

    void setPixel(int x, int y, const Color& color);
    void clear(const Color& color);
    void save(const std::string& filename);  // 输出图片
};
```

**DepthBuffer** - 深度缓冲
```cpp
class DepthBuffer {
    int width, height;
    std::vector<float> depths;  // 0.0 ~ 1.0

    bool testAndSet(int x, int y, float depth);
    void clear();
};
```

#### 几何图元（geometry/）

**Vertex** - 顶点结构
```cpp
struct Vertex {
    Vector3 position;    // 局部坐标
    Vector3 normal;       // 法线
    Vector2 texCoord;     // UV 坐标
    Color color;          // 顶点颜色
};
```

**Triangle** - 三角形
```cpp
struct Triangle {
    Vertex v0, v1, v2;
};
```

#### 渲染管线（pipeline/）

**VertexShader** - 顶点着色器
```cpp
// 输入：局部空间的顶点
// 输出：裁剪空间的顶点
Vertex clip(const Vertex& vertex, const Uniforms& uniforms);
```

**Rasterizer** - 光栅化器
```cpp
// 重心坐标光栅化
// 对三角形内的每个像素调用片段着色器
void rasterizeTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2,
                       FrameBuffer& frameBuffer, DepthBuffer& depthBuffer,
                       FragmentShader& fragmentShader);
```

**FragmentShader** - 片段着色器
```cpp
// 输入：插值后的片段数据
// 输出：最终颜色
Color shade(const Fragment& fragment);
```

#### 变换（transforms/）

**Transform** - MVP 矩阵生成
```cpp
Matrix4x4 getModelMatrix();
Matrix4x4 getViewMatrix(const Camera& camera);
Matrix4x4 getProjectionMatrix(float fov, float aspect, float near, float far);
Matrix4x4 getViewportMatrix(int width, int height);
```

#### 相机（Camera）

```cpp
class Camera {
    Vector3 position;
    Vector3 target;
    Vector3 up;
    float fov;
    float aspectRatio;
    float nearPlane;
    float farPlane;

    Matrix4x4 getViewMatrix();
    Matrix4x4 getProjectionMatrix();
};
```

#### 主渲染器（Renderer）

```cpp
class Renderer {
    int width, height;
    FrameBuffer frameBuffer;
    DepthBuffer depthBuffer;
    Camera camera;

public:
    void initialize(int width, int height);
    void render(const Mesh& mesh);
    void clear();
    void saveImage(const std::string& filename);
};
```

### 1.3 程序入口设计

**main.cpp**
```cpp
int main() {
    // 1. 初始化渲染器
    Renderer renderer(800, 600);

    // 2. 创建三角形数据
    std::vector<Triangle> triangles;
    triangles.push_back(createTriangle());

    // 3. 创建相机
    Camera camera(Vector3(0, 0, 5), Vector3(0, 0, 0));

    // 4. 渲染
    renderer.setCamera(camera);
    renderer.render(triangles);

    // 5. 保存图片
    renderer.saveImage("output.png");

    return 0;
}
```

### 1.4 输出方案

由于是纯 CPU 渲染，没有窗口系统，输出方式：

1. **输出为图片文件**（PPM/BMP/PNG）
   - 最简单，直接保存到文件查看
   - 使用 stb_image_write 库写 PNG

2. **可选：SDL2/GLFW 显示窗口**
   - 学习阶段用图片输出即可
   - 后续扩展再加窗口显示

### 1.5 编译配置

**CMakeLists.txt**
```cmake
cmake_minimum_required(VERSION 3.15)
project(SoftwareRenderer)

set(CMAKE_CXX_STANDARD 17)

# 依赖：stb_image_write（单头文件库，写PNG用）
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE third_party/stb)

add_executable(software-renderer
    src/main.cpp
    src/Renderer.cpp
    src/Camera.cpp
    src/math/MathUtils.cpp
    src/math/Vector3.cpp
    src/math/Vector4.cpp
    src/math/Matrix4x4.cpp
    src/geometry/Vertex.cpp
    src/buffers/FrameBuffer.cpp
    src/buffers/DepthBuffer.cpp
    src/transforms/Transform.cpp
    src/pipeline/VertexShader.cpp
    src/pipeline/Rasterizer.cpp
    src/pipeline/FragmentShader.cpp
    src/utils/ImageWriter.cpp
)

target_link_libraries(software-renderer stb)
```

### 1.6 第一阶段验收标准

| 检查项 | 说明 |
|--------|------|
| ✅ 程序可编译运行 | 无编译错误，正常退出 |
| ✅ 输出图片文件 | 生成 output.png |
| ✅ 可见三角形 | 图片中能看到彩色三角形 |
| ✅ 正确深度处理 | 多个三角形时远近正确 |

---

## 第二阶段：增加 Mesh 和立方体

**目标**：渲染一个立方体，理解 MVP 变换。

### 新增功能
- Mesh 类管理三角形列表
- 立方体生成器
- 相机控制（键盘/鼠标）
- MVP 变换可视化

### 验收标准
- 能渲染立方体的 12 个三角形
- 立方体旋转时背面正确剔除

---

## 第三阶段：深度测试与背面剔除

**目标**：正确的 3D 渲染。

### 实现要点
- Z-Buffer 深度测试
- 右手坐标系 → NDC → 屏幕坐标
- 背面剔除（根据法线方向）

---

## 第四阶段：纹理映射

**目标**：给立方体贴上纹理。

### 实现要点
- 双线性插值采样
- UV 坐标插值
- 纹理环绕模式

---

## 第五阶段：光照模型

**目标**：从 Flat Shading 到 Blinn-Phong。

### 学习路径
1. Flat Shading（面着色）
2. Gouraud Shading（顶点着色）
3. Phong Shading（像素着色）
4. Blinn-Phong 改进

---

## 第六阶段：PBR 渲染

**目标**：基于物理的材质渲染。

### 核心概念
- 能量守恒
- 漫反射（Lambertian）
- 镜面反射（Cook-Torrance）
- BRDF 双向反射分布函数
- PBR 材质参数（金属度、粗糙度）

### 实现内容
- 纹理 mipmap
- 法线贴图
- IBL 环境光照
- HDR 与色调映射

---

## 后续扩展方向

### 方向 A：光线追踪
- 光线-球体求交
- 光线-三角形求交
- 阴影光线
- 反射与折射

### 方向 B：路径追踪
- 蒙特卡洛积分
- 重要性采样
- Russian Roulette 终止
- Cornell Box 场景

### 方向 C：性能优化
- SIMD 指令集优化
- 多线程并行
- KD-Tree 加速结构
- 扫描线算法

---

## 学习资源推荐

### 书籍
1. 《Real-Time Rendering 4th》- 渲染管线权威参考
2. 《Physically Based Rendering 3rd》- PBR 理论圣经
3. 《Fundamentals of Computer Graphics 4th》- 图形学基础

### 在线资源
1. scratchapixel.com - 从零实现渲染器教程
2. learnopengl.com - OpenGL/PBR 学习
3. 闫令琪 GAMES101/GAMES202 - 图形学课程

---

## 文件清单（第一阶段）

```
src/
├── MathUtils.hpp
├── MathUtils.cpp
├── Vector3.hpp
├── Vector3.cpp
├── Vector4.hpp
├── Vector4.cpp
├── Matrix4x4.hpp
├── Matrix4x4.cpp
├── Vertex.hpp
├── Vertex.cpp
├── Triangle.hpp
├── FrameBuffer.hpp
├── FrameBuffer.cpp
├── DepthBuffer.hpp
├── DepthBuffer.cpp
├── VertexShader.hpp
├── VertexShader.cpp
├── Rasterizer.hpp
├── Rasterizer.cpp
├── FragmentShader.hpp
├── FragmentShader.cpp
├── Transform.hpp
├── Transform.cpp
├── Camera.hpp
├── Camera.cpp
├── Renderer.hpp
├── Renderer.cpp
├── ImageWriter.hpp
├── ImageWriter.cpp
└── main.cpp

third_party/
└── stb/
    └── stb_image_write.h
```

---

## 下一步行动

1. **确认文档是否满足需求**
2. **如需调整，继续补充细节**
3. **确认后开始实现第一阶段代码**

---

*文档版本：v1.0*
*更新日期：2026-05-02*
