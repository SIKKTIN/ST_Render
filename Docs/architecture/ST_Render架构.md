# ST_Render 架构

ST_Render 是 C++17 CPU 软件渲染器，配有用于观察渲染结果和修改参数的 ImGui 测试界面。

## 模块边界

| 目录 | 职责 |
|---|---|
| src/core/math | 向量、矩阵、颜色和数学工具 |
| src/core/camera | 透视与正交相机 |
| src/core/texture | 图像解码 |
| src/core/data | 场景序列化 |
| src/renderer | 几何、变换、顶点处理、光栅化、着色和缓冲区 |
| src/engine/editor | 场景对象、纹理管理、脚本和编辑器 UI |
| src/app | 应用入口、渲染示例和 Review 测试 |

场景数据位于 Data/ScenePrefab，纹理位于 Resource。SceneData 当前依赖编辑器的 Scene/GameObject 类型，core 目录并非完全无上层依赖。

## 渲染流程

```text
Mesh / Vertex
  → 模型、观察、投影变换
  → 裁剪与透视除法
  → 三角形光栅化与深度测试
  → 属性插值与片元着色
  → CPU FrameBuffer
  → SDL 纹理上传和 ImGui 显示
```

Renderer 类封装基本管线；3D 演示模块也直接组合 VertexShader、Rasterizer 和 FragmentShader，包含自己的裁剪与剔除调用。不能将演示模块的全部行为等同于 Renderer::render。

SDL2 承担窗口、输入和最终呈现。场景中的几何与像素计算由软件管线完成。

## 应用生命周期

主入口为 src/app/main/test_manager_main.cpp：

1. 初始化 SDL 视频子系统，扫描纹理。
2. 创建窗口、SDL 渲染器、画布以及测试模块。
3. 初始化 ImGui，向模块提供 SDL 渲染器。
4. 处理输入，更新当前模块，按需或实时渲染画布。
5. 绘制模块列表、创建面板、画布、控制台和参数面板。
6. 删除模块，释放 SDL/ImGui 资源并退出。

IModule 提供 getName/getCategory、update、render、renderControls、runConsole 和输入回调。render 的 void* 参数目前由主程序传入 SDL_Renderer*；参数名 canvasTexture 是历史命名。

## 保留的测试页面

- FrameBuffer：像素与上传方式。
- Rasterizer：三角形光栅化。
- 3D Render：立方体、相机和光照。
- Texture：加载、采样和纹理变换。
- Review/Math：数学回归测试。
- Review/Rasterizer：光栅化与深度测试。

## 场景与编辑器

Scene 使用 unique_ptr 持有 GameObject，提供父子层级、Sprite2D、Cinemachine，以及 startRuntime/updateRuntime/stopRuntime。场景、对象变换、脚本和纹理控件用于构造和调试渲染场景。

这部分保留为渲染实验辅助设施。新增功能优先围绕相机、材质、光照、纹理、几何和渲染验证。

## 构建

ST_Render_Manager 提供交互窗口；ST_Render_Dump 提供离屏诊断。依赖包括 SDL2、Dear ImGui、nlohmann/json 和源码内的 stb_image。

构建步骤见 [BUILD.md](../BUILD.md)。纹理管理细节见 [TextureManager 架构](TextureManager架构.md)。
