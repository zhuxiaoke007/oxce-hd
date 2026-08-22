# P0 构建环境 — 完成报告

> 日期：2026-08-21 | 状态：**P0 完成**
> 源码：`tmp/OpenXcom-oxce-plus/` | 构建输出：`tmp/build/bin/openxcom.exe`

## 1. 工具链

| 组件 | 版本 | 路径 |
|------|------|------|
| CMake | 3.x | `D:/Tools/cmake/bin/cmake.exe` |
| MinGW-w64 GCC | 16.2.0 | `D:/Tools/msys64/mingw64/bin/g++.exe` |
| mingw32-make | — | `D:/Tools/msys64/mingw64/bin/mingw32-make.exe` |
| pkg-config | 3.0.5 | `D:/Tools/msys64/mingw64/bin/pkg-config.exe` |

## 2. 依赖库（全部通过 MSYS2 pkg-config 发现）

| 库 | 版本 | 用途 |
|----|------|------|
| SDL | 1.2.16 | 核心渲染/事件 |
| SDL_image | 1.2.12 | 图像加载（PNG/JPEG/TIFF/WebP） |
| SDL_gfx | 2.0.26 | 图形原语 |
| SDL_mixer | 1.2.12 | 音频（FLAC/MAD/OGG/Vorbis） |
| zlib | 1.3.2 | 压缩 |
| OpenGL | opengl32/glu32 | OpenGL 后端 |

## 3. 构建命令

```bash
# 1. 配置（一次）
cmake -S "E:/Code/XCOM/tmp/OpenXcom-oxce-plus" -B "E:/Code/XCOM/tmp/build" \
  -G "MinGW Makefiles" \
  -DCMAKE_C_COMPILER="D:/Tools/msys64/mingw64/bin/gcc.exe" \
  -DCMAKE_CXX_COMPILER="D:/Tools/msys64/mingw64/bin/g++.exe" \
  -DCMAKE_MAKE_PROGRAM="D:/Tools/msys64/mingw64/bin/mingw32-make.exe" \
  -DDEPS_DIR=nonexistent \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PACKAGE=OFF

# 2. 编译
cmake --build "E:/Code/XCOM/tmp/build" -- -j$(nproc)
```

## 4. 源码修改（1 处）

**文件**：`src/CMakeLists.txt` 第 607-616 行

**问题**：原 CMakeLists 对 MinGW 使用 `-static` 强制全静态链接 + `.lib` 后缀系统库名。
- `-static` 与 MSYS2 的 dllimport 头文件冲突（FLAC 等 `__imp_` 前缀符号在静态库中不存在）
- `.lib` 后缀在 MinGW 中不匹配（应为 bare name，链接器自动加 `lib` 前缀 + `.a` 后缀）

**修复**：
```cmake
# 修改前
set ( basic_windows_libs advapi32.lib shell32.lib shlwapi.lib wininet.lib urlmon.lib )
if ( MINGW )
  set ( basic_windows_libs ${basic_windows_libs} mingw32 -mwindows )
  set ( static_flags  -static )
  set ( SDLMIXER_LIBRARY "${SDLMIXER_LIBRARY} -lwinmm" )
endif ()
set ( system_libs ${basic_windows_libs} SDLmain ${static_flags} )

# 修改后
if ( WIN32 )
  if ( MINGW )
    set ( basic_windows_libs advapi32 shell32 shlwapi wininet urlmon mingw32 -mwindows )
  else ()
    set ( basic_windows_libs advapi32.lib shell32.lib shlwapi.lib wininet.lib urlmon.lib )
  endif ()
  set ( system_libs ${basic_windows_libs} SDLmain )
endif ()
```

## 5. 产物

- **可执行文件**：`tmp/build/bin/openxcom.exe`（23MB，动态链接）
- **数据目录**：`tmp/build/bin/{common,standard,TFTD,UFO}/`（自动复制）
- **运行时 DLL**：全部来自 `D:/Tools/msys64/mingw64/bin/`（需将该目录加入 PATH）

## 6. VSCode 配置

已创建 `.vscode/settings.json` 和 `.vscode/cmake-kits.json`，VSCode 中打开 `tmp/OpenXcom-oxce-plus/` 文件夹即可使用 CMake Tools 构建。

## 7. 下一步（P1）

- 字体子系统增加 HD/TTF 后端（`Font` 类扩展）
- Screen 新增 32-bit text overlay 表面
- 集成 SDL_ttf（MSYS2 已安装 `mingw-w64-x86_64-SDL_ttf`）
