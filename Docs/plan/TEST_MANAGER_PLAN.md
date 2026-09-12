# 渲染测试管理器

测试管理器用于验证 CPU 渲染器。当前模块为 FrameBuffer、Rasterizer、3D Render、Texture，以及 Review 下的 Math 和 Rasterizer。

## 新增模块

1. 在 src/app 中实现 app/module/IModule.hpp。
2. 实现渲染、参数面板及必要的输入回调。
3. 在 test_manager_main.cpp 的 allLeaves 注册实例。
4. 将新增 .cpp 加入 src/app/main/CMakeLists.txt。
5. 复用数学、帧缓冲和光栅化组件，验证新增行为。

页面应提供明确的渲染示例或测试结果。

## 后续方向

- 加强裁剪、深度测试、插值和纹理采样的回归验证。
- 为相机、光照和材质提供可重复的对比场景。
- 逐步统一 3D 演示与 Renderer 的管线调用。

实际模块边界和启动流程见 [架构说明](../architecture/ST_Render架构.md)。
