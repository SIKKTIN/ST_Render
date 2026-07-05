# ST_Render Documentation

构建与开发文档。

## 构建

- [BUILD.md](./BUILD.md) — 快速上手 / 日常编译
- [TROUBLESHOOTING.md](./TROUBLESHOOTING.md) — 常见错误排查
- [BUILD_TOOLS_REFERENCE.md](./BUILD_TOOLS_REFERENCE.md) — 工具链版本与路径

## 架构

- [architecture/ST_Render架构.md](./architecture/ST_Render架构.md)
- [architecture/Scene2D架构.md](./architecture/Scene2D架构.md)
- [architecture/TextureManager架构.md](./architecture/TextureManager架构.md)

## 推荐路径

第一次拉代码：
```cmd
scripts\configure.bat
scripts\build.bat
```

增量开发：
```cmd
scripts\build.bat
```

## 主要工具链

| 工具 | 版本 |
|---|---|
| MSVC | 14.44.35207 |
| CMake | 4.3.2 |
| vcpkg | latest |
| Windows SDK | 10.0.26100 |

完整说明见 [BUILD.md](./BUILD.md) 与 [BUILD_TOOLS_REFERENCE.md](./BUILD_TOOLS_REFERENCE.md)。