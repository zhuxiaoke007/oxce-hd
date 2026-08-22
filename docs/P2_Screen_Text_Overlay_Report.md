# P2 实现报告：Screen 文字覆盖层

**日期**：2026-08-21
**阶段目标**：在 `Screen` 增加与显示分辨率同尺寸的 **8-bit** 文字覆盖层；`Text` 在 HD 字体启用时把字形以显示分辨率绘制到该覆盖层（复用 `PaletteShift` 着色）；`flip()` 在放大像素 buffer 之后把覆盖层叠加到屏幕。低分辨率 buffer 路径与既有行为完全保留。
**状态：✅ 编译验证通过**（`mingw32-make` 100% built，`openxcom.exe` 23,173,096 字节，链接 `SDL_ttf.dll`）。

---

## 1. 设计要点

| 决策 | 理由 |
|------|------|
| overlay 用 **8-bit**（非计划初稿的 32-bit） | 默认 `_screen` 为 8-bit（无 hqx/xbrz/OpenGL 时 `_bpp=8`）；8-bit 覆盖层 + colorkey 可直接 `SDL_BlitSurface` 叠加，且能完美复用 `PaletteShift`（写调色板索引）。32-bit 叠加在 8-bit 屏上反而难处理。 |
| overlay 调色板 = 屏幕调色板，colorkey = 索引 0 | 字形墨迹经 `PaletteShift` 写入“着色后索引”，透明像素为 0；叠加时 0 被 colorkey 跳过，墨迹索引在同源调色板下颜色一致。 |
| 坐标映射 `dx = leftBand + bx*scale` | `Zoom::flipWithZoom` 把 `_surface`（`_baseWidth×_baseHeight`）放大到 `_screen`，内容区落在黑边内、缩放 `scale`；buffer 像素 `(bx,by)` 映射到 display 即用此式。 |
| `scale` 取内容缩放整数（四舍五入） | 绝大多数用户跑整数倍（2x/3x/4x）；字形 cell 宽=advance×scale、高=cellHeight×scale，与 P1 栅格化一致，故排版逐像素对齐。 |
| 复用 `ShaderDraw<PaletteShift>` | 与 `Text` 原低位绘制**完全相同的调用**，只是目标换成 overlay、源换成 HD 字形 `Surface`；`contrast`(mul=3)、`invert`(mid=3)、`_color2` 翻转全部自动生效。 |

**帧时序（关键，3 个钩子点）**：
1. `Screen::clear()`（每帧一次，清 buffer+display）→ 同时清 overlay 并同步调色板。
2. `State::blit()` → `Text::draw()` → HD 启用时把字形绘入 overlay（绝对坐标 `getX()+x`）。
3. `Screen::flip()` → 放大 buffer 到 `_screen` → **非 OpenGL** 模式下 `SDL_BlitSurface(overlay, _screen)` 叠加。

`Surface::draw()` 在每次 `Text::draw` 开头会 `clear()` `this`，因此 HD 分支 `continue` 跳过低位绘制后，`this` 保持空白，不会把模糊低位文字漏进 buffer（无重影）。

---

## 2. 改动文件清单

### 2.1 `src/Engine/Screen.h`
- 私有成员：`Surface *_textOverlay;`（#ifdef __HDFONTS__）。
- 静态成员 + 访问器：`static Screen *_activeScreen;`、`static Screen *getActiveScreen()`、`static void setActiveScreen(Screen*)`（供 `Text` 取 overlay 与变换）。
- 公共方法：`Surface *getTextOverlaySurface() const;`、`int getHdScale() const;`、`void baseToDisplay(int bx,int by,int&dx,int&dy) const;`（均 #ifdef __HDFONTS__）。

### 2.2 `src/Engine/Screen.cpp`
- 静态定义 `Screen *_activeScreen = 0;`
- 构造：初始化 `_textOverlay=0`、`setActiveScreen(this)`。
- `resetDisplay()`：显示尺寸变化（或首次）时 `new Surface(ow,oh)` 重建 overlay，设 `SDL_SRCCOLORKEY`/0，并把 deferredPalette 写入 overlay 调色板。
- `clear()`：清 overlay 并每帧同步调色板（捕获调色板切换）。
- `~Screen()`：delete overlay。
- `flip()`：在 `SDL_Flip` 前，条件 `Options::hdFonts && !hdFontTtf.empty() && !useOpenGL()` 下叠加 overlay。
- 实现 `getHdScale()`（内容缩放整数）、`baseToDisplay()`、`getTextOverlaySurface()`。

### 2.3 `src/Interface/Text.cpp`
- 包含 `../Engine/Screen.h`。
- `draw()` 开头计算 `hdActive`（选项开启且 `_activeScreen` 有 overlay）、`hdScale`。
- 逐字 `else` 分支：若 `hdActive && font->isHdFont()`，取 `font->getHdChar(*c, hdScale)`，用 `ShaderDraw<PaletteShift>(ShaderSurface(overlay,dx,dy), ShaderCrop(SurfaceCrop(glyph)), color, mul, mid)` 绘到 overlay 并 `continue`；若字形不可用则**回退**到原低位绘制（保证缺字形也有显示）。

### 2.4 `src/Engine/Font.cpp`（健壮性）
- `invalidateHdCache()` 整体包入 `#ifdef __HDFONTS__`（原先函数体缺保护，无 SDL_ttf 构建会引用未定义成员）；无 `__HDFONTS__` 时函数体为空，链接安全。

---

## 3. 编译验证

```
mingw32-make -> [100%] Built target openxcom
objdump -p bin/openxcom.exe | grep -ci sdl_ttf  -> 1   (仍链接 SDL_ttf.dll)
```
无 error / undefined reference。

---

## 4. 已知限制 / 下一步

1. **OpenGL 模式不叠加**：`flip()` 在 `useOpenGL()` 时走 GL 后端，overlay 不显示；该模式下 HD 文字暂时回退为位图字体（保留现有行为）。后续可在 GL 路径里把 overlay 作为纹理合成，留待 P5 之后。
2. **非整数显示倍率**：`scale` 四舍五入为整数。1.5x 等分数倍下 HD 文字可能与放大后的像素 UI 有亚像素错位；整数倍（2x/3x/4x）下像素对齐。
3. **窗口/容器相对定位**：依赖 `getX()/getY()` 为绝对 buffer 坐标（OpenXcom 标准做法，`State::blit` 直接以此 blit）。若某 UI 对子控件做了额外偏移，需另作处理（当前覆盖全部标准 UI）。
4. **需 TTF 文件**：运行时 `hdFonts: true` 且 `hdFontTtf: <绝对路径>` 才激活；缺省关闭，改动对默认行为零影响。
5. **着色 100% 复用**：`PaletteShift`（`contrast`/`invert`/次色翻转）在 overlay 上表现与低位一致。

---

## 5. 手动验证步骤（P5 将自动化）

```bash
# 1) 放一个 TTF（如 DejaVuSans.ttf）到已知路径
# 2) 在 options 文件写入：
#      hdFonts: true
#      hdFontTtf: "C:/path/to/DejaVuSans.ttf"
# 3) 以 PATH 含 mingw64 bin 启动（解析 SDL_ttf.dll）：
export PATH="/d/Tools/msys64/mingw64/bin:$PATH"
cd "E:/Code/XCOM/tmp/build/bin" && ./openxcom.exe
# 4) 观察：主菜单/基地/地球层/战斗底栏文字应清晰锐利（非模糊），
#    且与原版布局一致；关闭 hdFonts 应与未改前完全一致。
# 5) P5 用 tools/run_checks.py --screenshots 做 headless 冒烟 + 截图对比。
```
