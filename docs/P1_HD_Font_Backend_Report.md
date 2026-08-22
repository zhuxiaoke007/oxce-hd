# P1 实现报告：Font 类 HD/TTF 后端

**日期**：2026-08-21
**阶段目标**：在 `Font` 类中加入可选的 HD/TTF 渲染后端，以显示分辨率栅格化字形并输出与原版位图字体兼容的 8-bit 调色板像素（墨迹=索引 1，透明=索引 0），让 P2 的屏幕文字叠加层无需改动着色逻辑即可直接复用。

**状态：✅ 编译验证通过**（minGW-w64 GCC 16.2.0 + CMake，MSYS2 Makefiles）。`openxcom.exe`（23,168,795 字节）已成功链接 `SDL_ttf.dll`。

---

## 1. 设计决策（关键约束）

| 决策 | 理由 |
|------|------|
| 布局数学保持在低分辨率坐标 | `Text::draw` 用 `font->getCharSize(c).w`（advance = rect.w + spacing）做排版；若 HD 字形 cell 尺寸 = advance×scale、高度 = cellHeight×scale，则所有现有排版逻辑**逐像素不变**，无需改动 `Text`/`Surface` 管线。 |
| 墨迹二值化为调色板索引 1 | 原版 `ShaderDraw<PaletteShift>` 读取的是 8-bit 字形像素索引（墨=1，透明=0）。HD 字形用 50% 阈值 binarize 输出索引 1，即可**原样复用**着色合约。 |
| 关闭 kerning、等宽 advance | 沿用原版位图字体的 advance 宽度，保证中英混排对齐与原版一致。 |
| 字形缓存按 (字符, scale) | 同一 TTF 在不同显示缩放下 cell 尺寸不同；变化时丢弃旧缓存。 |

**TTF 尺寸修正**：以 `ptsize = cellHeight×scale` 打开字体，测量实际 `TTF_FontHeight`，若与目标行高不符，以 `corrected = target*target/height` 重开一次（FreeType 指标随 ptsize 线性缩放，一次修正足够）。

**基线放置**：`cellBaseline = cellH − max(1, cellHeight/4)×scale`；`dstY = cellBaseline − ascent + inkMinY`，将字形 ink 包围盒按基线对齐到 cell 内。

---

## 2. 改动文件清单

### 2.1 `src/Engine/Font.h`
- 新增 `#include <string>`（TTF 路径成员）、按需 `#include <SDL_ttf.h>`（仅 `__HDFONTS__`）。
- 私有成员（全部置于 `#ifdef __HDFONTS__`）：
  ```cpp
  std::string _hdTtfPath;
  mutable TTF_Font *_hdFont;
  mutable int _hdScale;
  mutable std::unordered_map<UCode, Surface*> _hdGlyphs;
  bool ensureHdFont(int scale) const;
  Surface *rasterizeHdChar(UCode c, int scale) const;
  ```
- 公共接口：
  ```cpp
  void loadHdFont(const std::string&);
  bool isHdFont() const;
  void invalidateHdCache() const;        // 已声明为 const，因 _hdGlyphs 为 mutable
  Surface *getHdChar(UCode c, int scale) const;
  ```

### 2.2 `src/Engine/Font.cpp`
- 头文件：`#include "Logger.h"`（**注意**：日志头是 `Logger.h` 而非 `Log.h`，原 `Log.h` 不存在，这是早期编译报错的根因）、按需 `#include <SDL_ttf.h>` + `<algorithm>`。
- 构造/析构：构造初始化 `_hdFont=0, _hdScale=0`；析构调用 `invalidateHdCache()` 后 `TTF_CloseFont`。
- 实现方法：
  - `loadHdFont`：首次静态调用 `TTF_Init()`（失败记日志并返回）；关闭旧句柄、清空缓存、保存路径。
  - `invalidateHdCache() const`：删除所有缓存 `Surface`、清空 map、重置 `_hdScale=0`。
  - `ensureHdFont(scale) const`：句柄有效且 scale 匹配则直接返回；否则重开（含一次尺寸修正），`TTF_SetFontHinting(_hdFont, TTF_HINTING_LIGHT)`。
  - `rasterizeHdChar(c, scale)`：`TTF_RenderUTF8_Shaded` → 扫描覆盖度 ≥128 的 ink 包围盒 → 创建 `Surface(cellW, cellH)`、`clear()`、按基线把覆盖像素 `setPixel(x, dstY+r, 1)` 写入。
  - `getHdChar(c, scale)`：`ensureHdFont` 门控 → 缓存查找 → 栅格化并缓存（**失败亦缓存为 nullptr**，避免每帧重渲染）。

### 2.3 `CMakeLists.txt`
在 `elseif(UNIX OR MINGW OR CYGWIN)` 的 pkg-config 分支内增加可选依赖：
```cmake
pkg_check_modules(PKG_SDLTTF SDL_ttf)
if(PKG_SDLTTF_FOUND)
  include_directories(${PKG_SDLTTF_INCLUDE_DIRS})
  set(PKG_DEPS_LDFLAGS "${PKG_DEPS_LDFLAGS} ${PKG_SDLTTF_LDFLAGS}")
  add_definitions(-D__HDFONTS__)
  message(STATUS "SDL_ttf found - HD font backend enabled")
endif()
```
配置期已确认输出 `Found SDL_ttf, version 2.0.11`，`__HDFONTS__` 被定义，`-lSDL_ttf` 已追加。

### 2.4 `src/Engine/Options.inc.h` + `Options.cpp`
- `inc.h`：`hdFonts`（bool，general 组）、`hdFontTtf`（std::string，OPT 组）。
- `Options.cpp`：注册
  ```cpp
  _info.push_back(OptionInfo(OPTION_OXC, "hdFonts", &hdFonts, false));
  _info.push_back(OptionInfo(OPTION_OXC, "hdFontTtf", &hdFontTtf, ""));
  ```

### 2.5 `src/Mod/Mod.cpp`
`loadExtraResources()` 字体加载循环内，在 `font->load(fontReader)` 之后：
```cpp
if (Options::hdFonts && !Options::hdFontTtf.empty())
    font->loadHdFont(Options::hdFontTtf);
```

---

## 3. 编译验证

```
[ 63%] Linking CXX executable ..\bin\openxcom.exe   →  成功
objdump -p bin/openxcom.exe | grep "DLL Name"       →  含 SDL_ttf.dll
```

- 早期错误：`Font.cpp` 误用 `#include "Log.h"`（无此文件）→ 改为 `Logger.h`。
- const 正确性错误：`ensureHdFont() const` 调用非 const 的 `invalidateHdCache()` → 将 `invalidateHdCache()` 声明/定义为 `const`（因 `_hdGlyphs` 已是 `mutable`）。
- 逻辑自纠（运行前发现并修复）：
  - ink 包围盒扫描提前退出行循环 → 改为全行扫描 + min/max 跟踪。
  - 分辨率切换后旧 scale 字形缓存被复用 → `ensureHdFont` 在重开字体时调用 `invalidateHdCache()`。

---

## 4. 已知限制 / 下一步

1. **HD 字形尚不可见**：P1 只交付了后端与加载钩子。要让字形实际渲染，仍需 **P2（屏幕文字叠加层）** 在 `Screen`/`Text` 绘制路径中调用 `Font::getHdChar()` 并把 8-bit 字形叠加到放大后的画面上。默认 `hdFonts=false`，新代码路径完全休眠，不影响既有行为。
2. **需要 TTF 文件**：运行时需把 `hdFonts=true` 且 `hdFontTtf=<绝对路径>` 写入 options 文件（P4 再做 GUI 开关）。缺省字重为常规；粗体/等宽 CJK 需后续资源方案（见 `HD_Resources.md`）。
3. **基线模型为近似**：用 `descent ≈ cellHeight/4` 估算下伸部，对绝大多数原版字体视觉对齐足够；极端字体可能需按字体微调。
4. **P3（字体资源/度量）**、**P4（Options GUI）**、**P5（验证冒烟+截图）** 待启动。

---

## 5. 手动验证步骤（供 P2/P5 使用）

```bash
# 1) 准备一个 TTF（如 DejaVuSans.ttf）放到 tmp/build/bin/ 或绝对路径
# 2) 在 options 文件（UFO/options.cfg 或对应用户目录）加入：
#      hdFonts: true
#      hdFontTtf: "C:/path/to/DejaVuSans.ttf"
# 3) 启动（PATH 需含 mingw64 bin 以解析 SDL_ttf.dll）：
export PATH="/d/Tools/msys64/mingw64/bin:$PATH"
cd "E:/Code/XCOM/tmp/build/bin" && ./openxcom.exe
# 4) P5 阶段用 tools/run_checks.py --screenshots 做 headless 冒烟+截图对比
```
