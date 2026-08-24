# X-COM HD 版构建指纹（Build Fingerprint）

> 记录日期：2026-08-22 | 关联：`HD_UI_Plan.md`（P0–P5）、规划阻塞项 B1/B2
> 目的：固定可运行的 HD 构建产物身份与编译宏状态，供构建回放 / 复现比对。

## 1. 交付构建（运行入口）

| 项 | 值 |
|---|---|
| 启动入口 | `E:\Code\XCOM\out\run_hd.bat` |
| 二进制 | `E:\Code\XCOM\tmp\build\bin\openxcom.exe` |
| 数据目录 | `-data E:\Code\XCOM\out\oxce` |
| 用户目录 | `-user E:\Code\XCOM\out\oxce-user` |

> 注意：`E:\Code\XCOM\out\oxce\OpenXcomEx.exe`（2026-04-26）为上游原始分发体，
> **非 HD 构建**。真正的 HD 构建是 `tmp/build/bin/openxcom.exe`。

## 2. 产物指纹

| 属性 | 值 |
|---|---|
| 文件 | `E:\Code\XCOM\tmp\build\bin\openxcom.exe` |
| SHA-256 | `42C057417E9751E0…`（2026-08-24 重建：P8 非整数缩放分辨率下 HD 文字错位修复，见 `P8_NonInteger_Scale_Fix_Report.md`） |
| 大小 | 23,238,070 字节 |
| 修改时间 | 2026-08-24（P8 修复构建） |

> 历史：
> - 首次 `4DA9D832...`（05:07:53）：B1 核验时的非密闭构建。
> - `5B03D6BE...`（15:30:06）：B2 树脂化默认打包字体。
> - `777C1A1B...`（15:41:38）：修复“HD 覆盖层从不绘制”的门控 bug（见 5.2），并切软件渲染。
> - `A247E0CD...`（重建）：修复“全部文字消失、只剩空白按钮”的回归（见 5.3）。
> - `7B81E7CB...`（重建）：修复“HD 字形绘制为空操作”的 ShaderSurface 偏移 bug（见 5.4）。
> - `8D3A4B97...`（重建）：加入 5.5 按钮标签修复（TextButton 关闭 HD 路径）。
> - `31A44081...`（2026-08-22）：加入 5.6 合成调色板修复；P5 游戏内验证通过。
> - `04355242...`（2026-08-23）：分辨率箭头不可见修复（SDL_ListModes 失败回退，commit 8d300cf）。
> - `42C05741...`（2026-08-24 最终）：P8 非整数缩放修复——内容缩放全链路 int→double（Screen::baseToDisplay 按真实分数缩放映射、Font TTF 字号/字形格按真实比例取整）。

## 3. 构建宏状态（B1 核验结论：✅ HD 已编译）

`__HDFONTS__` 生效证据（`objdump -p` 导入表）：`openxcom.exe` 从 `SDL_ttf.dll` 导入
`TTF_Init`、`TTF_OpenFontIndex`、`TTF_CloseFont`、`TTF_FontHeight`、`TTF_FontAscent`、
`TTF_GlyphIsProvided`、`TTF_RenderUTF8_Shaded`、`TTF_SetFontHinting`；并导出
`OpenXcom::Font::rasterizeHdChar(DII, TTF_Font*)`。
以上 TTF 调用在源码中全部位于 `#ifdef __HDFONTS__` 分支内，因此导入即证明**以 `__HDFONTS__` 编译、HD 路径生效**。
B2 重建后已复核，上述导入项仍存在。

## 4. 构建环境与依赖

| 项 | 值 |
|---|---|
| 编译器 | MinGW g++（`D:/Tools/msys64/mingw64/bin`） |
| 生成器 / 构建类型 | MinGW Makefiles / Release |
| 源码树 | `tmp/OpenXcom-oxce-plus`（⚠️ 非 git 仓，无源 rev 可钉） |
| SDL_ttf | `SDL_ttf.dll v2.0.11`（运行时 dll，由 `run_hd.bat` 的 mingw PATH 提供） |
| 打包字体 | `out/oxce/fonts/ark-pixel-16px-monospaced-latin.ttf`（OFL）、`out/oxce/fonts/wqy-16px.ttf`（GPL+嵌入例外） |
| SDL | `SDL.dll v1.2.16` |
| 其他运行依赖 | `libSDL_gfx/image/mixer`、`libgcc_s_seh`、`libstdc++-6` 等（mingw64） |

## 5. 已知可复现性缺口

1. **无源版本钉**：`OpenXcom-oxce-plus` 未纳入 git，指纹仅能以 mtime/hash 兜底。
2. **工具链未钉**：依赖环境中随机的 MSYS2 mingw64，未固定 g++/SDL/FreeType 版本。

### 5.1 B2：“字体资源固化”已落地（2026-08-22）
- 打包字体进数据目录：`out/oxce/fonts/`（Ark Pixel 16px 拉丁 + 文泉驿正黑）。
- 引擎改动（`OpenXcom-oxce-plus/src/Mod/Mod.cpp`）：
  - `resolveHdFontPath` 新增**数据目录相对解析**（`<data>/fonts/...`），不再依赖进程 cwd。
  - 默认拉丁/等宽字体 → `fonts/ark-pixel-16px-monospaced-latin.ttf`；默认 CJK → `fonts/wqy-16px.ttf`。
  - 用户仍可用 `hdFontTtf` / `hdFontTtfGeo` / `hdFontTtfCJK` 覆盖。
- 已消除运行时对 `C:/Windows/Fonts`（arial/consola/msyh）的依赖。
- 度量说明：目标字号 = 位图行高 × `hdFontScale`（[Font.cpp ensureHdFont](file:///E:\Code\XCOM\tmp\OpenXcom-oxce-plus\src\Engine\Font.cpp#L238-L279)），基线继承约 1/4 行高的下降部；墨迹经 50% 覆盖阈值二值化（rasterizeHdChar）。字号 ≥ 位图行高×缩放 时笔画实心、不再缺失。

### 5.2 HD 覆盖层从不绘制的门控 bug 修复（2026-08-22，新指纹 777C1A1B）

**根因**：覆盖层绘制被 `Options::hdFontTtf` 非空 + `useOpenGL` 双重拦截，导致用户看到“无差别”：
1. [Text.cpp](file:///E:\Code\XCOM\tmp\OpenXcom-oxce-plus\src\Interface\Text.cpp#L597) 与 [Screen.cpp](file:///E:\Code\XCOM\tmp\OpenXcom-oxce-plus\src\Engine\Screen.cpp#L240) 的启用条件都要求 `!Options::hdFontTtf.empty()`，但默认字体是 Mod 里解析后赋给 Font 且**不回写该选项**，故选项为空即永不绘制。
2. `useOpenGL: true` 时 Screen 合成处 `!useOpenGL()` 按设计跳过 HD 覆盖层。

**修复**：
- 移除 `Options::hdFontTtf` 门控，改为逐字形用 `font->isHdFont()` 守卫。
- 本机运行配置 `out/oxce-user/options.cfg` 设 `useOpenGL: false`（切软件渲染；OpenGL 下覆盖层暂按原设计回退位图，属未实现的后续项）。
- 需 `hdFonts:true`（已开）且在**软件渲染**下运行才显示 HD。

### 5.3 “全部文字消失、只剩空白按钮”回归修复（2026-08-22，新指纹 A247E0CD）

**根因**：Text 与 Screen 的 HD 判定不一致，造成**防止 overlay 永远不呈现时被激发的空载路径**。
- [Text.cpp](file:///E:\Code/XCOM/tmp/OpenXcom-oxce-plus/src/Interface/Text.cpp#L601) 激活 HD 绘制**未检查 OpenGL**：只要 `Options::hdFonts` 就进入 HD 分支并 `continue` 跳过位图字体（本机把 `Options::hdFontTtf` 门控移除后即触发）。
- [Screen.cpp](file:///E:\Code/XCOM/tmp/OpenXcom-oxce-plus/src/Engine/Screen.cpp#L240) 合成 HD 覆盖层**在 OpenGL 下按设计跳过**（`!useOpenGL()`）。
- 结果：OpenGL 运行到位图文字被跳过、HD 文字又被 Screen 拒绝合成 → 所有文字全部不绘制，只剩按钮像素图。

**修复**：在 Text.cpp 的 HD 激活条件追加 `!Options::useOpenGL`，与 Screen 合成条件保持一致。OpenGL 下回到位图字体路径（文字恢复可见、暂无 HD）；软件渲染下覆盖层正常合成、显示 HD 文字。

### 5.4 “HD 字形绘制为空操作（覆盖层墨迹为 0）”修复（2026-08-22，新指纹 7B81E7CB）

**现象**：此前日志只能证明“字形被生成 + draw 分支被执行”，但 `Screen::flip` 合成时覆盖层墨迹始终为 0（`HD-DIAG-FLIP nonzero=0`），文本未真正显示。加探针后确诊：`HD-DIAG-DST` 显示第一帧 write 后 `inkOnOverlay=0`。

**根因**：本引擎 `ShaderDraw` 的坐标约定是——偏移放在源 `ShaderCrop(src, x, y)` 上，目标必须用 `ShaderSurface(dst, 0, 0)`。原代码把偏移误放到目标：
```cpp
// 错误：目标 range 被移到 (dx..dx+W, dy..dy+H)，而源 crop range 仍是 (0..W, 0..H)，
// 两者求交为空 → ShaderDrawImpl 直接 return，每个字形 write 都是空操作。
ShaderDraw<PaletteShift>(
    ShaderSurface(hdScreen->getTextOverlaySurface(), dx, dy),
    ShaderCrop(SurfaceCrop(glyph)),
    ...);
```
`ShaderSurface(target, dx, dy)` 的 `get_range()` 返回被偏移的图像域（`dx..dx+size`），而 `ShaderCrop(glyph)` 的图像域仍是 `0..size`，二者坐标系不一致（可见代码 [ShaderMove.h](file:///E:\Code\XCOM\tmp\OpenXcom-oxce-plus\src\Engine\ShaderMove.h#L124-L147) / [ShaderDrawHelper.h](file:///E:\Code\XCOM\tmp\OpenXcom-oxce-plus\src\Engine\ShaderDrawHelper.h#L348-L356)）。空交集使 `ShaderDrawImpl` 未做任何像素写入。

**修复**：偏移改放到源 crop：
```cpp
ShaderDraw<PaletteShift>(
    ShaderSurface(hdScreen->getTextOverlaySurface(), 0, 0),
    ShaderCrop(SurfaceCrop(glyph), dx, dy),
    ShaderScalar(color), ShaderScalar(mul), ShaderScalar(mid));
```
（与位图路径 [Text.cpp](file:///E:\Code\XCOM\tmp\OpenXcom-oxce-plus\src\Interface\Text.cpp#L754) 一致：目标用 `(dst,0,0)`、偏移交给 `...chr.setX/setY` 源头。）

**验证**（软件渲染、3840×2160×8bpp）：
- `HD-DIAG-DST: dx=310 dy=60 overlay=3840x2160 glyphbox=108x48 clip=in inkOnOverlay=260`：字形墨迹已落入覆盖层。
- `HD-DIAG-FLIP: blit=0 overlay=3840x2160 bpp=8 nonzero=74059 pal[134]=(100,204,188)`：合成把整屏 HD 文字（74,059 个非零墨迹像素）真正 blit 到位，且调色板显示为可见的青绿色文本色。

至此 HD 文本在软件渲染下**已端到端可见**，后续进入 P5 视觉验证（截图对比 + 中文溢出检查）。

### 5.5 “选项页按钮空白、悬停时底部闪过解释条目”修复（2026-08-22，新指纹 8D3A4B97）

**现象**：Options 页面的按钮标签全部空白；鼠标悬停部分位置时底部会闪一下解释条目，但选项行没有文字。

**根因**：按钮标签是 `TextButton`，内部含一个相对坐标 `(0,0)` 的嵌套 `Text`，随后被 blit 进按钮表面。HD 覆盖层路径按**绝对屏幕坐标**计算（`baseToDisplay(getX()+x, getY()+y)`），嵌套 Text 的 `getX()/getY()` 为 0 → 字形被画到覆盖层错误位置，`continue` 又跳过位图绘制 → 按钮表面一个字都没有。底部解释条是顶层 Text（绝对坐标正确）所以正常——与用户所见完全吻合。

**修复**（`TextButton.cpp` / `Text.h` / `Text.cpp`）：
- `Text` 新增逐实例开关 `bool _hdEnabled`（默认 true）+ `setHdEnabled(bool)`；
- HD 激活条件追加 `_hdEnabled &&`；
- `TextButton` 构造中对内部标签调用 `_text->setHdEnabled(false)` → 按钮标签回位图路径（恢复可见），顶层文本仍走 HD。

**验证**：主菜单 6 按钮（新游戏/战斗/读取游戏/续/选项/退出）与选项页分类（视频/音频/控制/全局选项/鼠标/高级/文件）全部有文字；Advanced 页 20 行选项正常 addRow（旧 bad_alloc 崩溃亦确认修复）。

### 5.6 “战斗底栏数字/位图元素颜色错乱”修复（2026-08-22，新指纹 31A44081）

**现象**：HD 模式下战斗底栏状态数字（TU/能量/生命/士气）在屏幕上呈黑色不可见，或颜色与 legacy 模式不一致（旧构建曾映射成 (8,24,80) 暗蓝）。

**根因**（探针定位）：`Screen::flip()` 用 `SDL_BlitSurface(overlay → _screen)` 做 8-bit→8-bit 合成。SDL 1.2 的 8→8 blit 会按**目标调色板做颜色翻译**并传播源表面调色板状态；而 overlay 调色板在 `clear()` 里用 `deferredPalette` 同步，**早于**本帧调色板推入（战斗中会拿到上一帧/菜单的陈旧调色板）。合成 blit 的翻译/传播把 `_screen` 调色板污染成陈旧状态 → 下一帧上采样（`Zoom::flipWithZoom` 内的 8→8 blit）按被污染的调色板重映射位图像素 → 数字索引落到黑色。

**修复**（`Screen.cpp`）：
- 合成前用当前帧 `deferredPalette` 重新同步 overlay 调色板；
- **合成改为手动索引复制**（跳过索引 0=透明，其余原样拷贝），彻底绕开 SDL 8→8 的调色板翻译/传播副作用；
- 上采样保持原版 `SDL_BlitSurface`（其颜色翻译是原版位图渲染所依赖的，索引原样拷贝反而会让本调色板中偏暗的颜色直接不可见）。

**验证**：战斗底栏四组状态数字以正确 per-stat 颜色渲染（TU 绿 / 能量黄 / 生命红 / 士气蓝，接口 `numTUs color:64`、`numEnergy color:16`、`numHealth color:32`、`numMorale color:192`）；HD 与 legacy 模式位图元素像素一致；overlay 合成墨迹正常（战斗帧 nonzero≈2926）。

### 5.7 P5 游戏内验证（2026-08-22，最终构建）

- **自动读档钩子**：`-hdshot:<save>` 启动参数（`src/HdValidationHook.h`，`#ifdef __HDFONTS__`）：同步加载数据 → 读档 → 进入 geoscape/battle → 外部 F12 截图。legacy 对照 = `hdFonts:false` 再跑一遍。
- **截图证据**（`HD_feature/screenshots/p5_in_game/`）：
  - `p5_menu.png`：主菜单 6 按钮全部有文字（位图路径，可见）；
  - `p5_options3.png`：选项页分类与按钮文字完整（5.5 修复）；
  - `p5_final_battle.png`：战斗地图 + 底栏（状态数字四色正常，弹药数字蓝色）；
  - `p5_final_geo.png`：地理球 + 右侧菜单（数据/基地/工表/UFO百科/装备/状态）与时间控制（五秒/5分/一小时/一分/三十分/1天），中文无乱码。

**现象**：此前日志只能证明“字形被生成 + draw 分支被执行”，但 `Screen::flip` 合成时覆盖层墨迹始终为 0（`HD-DIAG-FLIP nonzero=0`），文本未真正显示。加探针后确诊：`HD-DIAG-DST` 显示第一帧 write 后 `inkOnOverlay=0`。

**根因**：本引擎 `ShaderDraw` 的坐标约定是——**偏移放在源 `ShaderCrop(src, x, y)` 上，目标必须用 `ShaderSurface(dst, 0, 0)`**。原代码把偏移误放到目标：
```cpp
// 错误：目标 range 被移到 (dx..dx+W, dy..dy+H)，而源 crop range 仍是 (0..W, 0..H)，
// 两者求交为空 → ShaderDrawImpl 直接 return，每个字形 write 都是空操作。
ShaderDraw<PaletteShift>(
    ShaderSurface(hdScreen->getTextOverlaySurface(), dx, dy),
    ShaderCrop(SurfaceCrop(glyph)),
    ...);
```
`ShaderSurface(target, dx, dy)` 的 `get_range()` 返回被偏移的图像域（`dx..dx+size`），而 `ShaderCrop(glyph)` 的图像域仍是 `0..size`，二者坐标系不一致（可见代码 [ShaderMove.h](file:///E:\Code\XCOM\tmp\OpenXcom-oxce-plus\src\Engine\ShaderMove.h#L124-L147) / [ShaderDrawHelper.h](file:///E:\Code\XCOM\tmp\OpenXcom-oxce-plus\src\Engine\ShaderDrawHelper.h#L348-L356)）。空交集使 `ShaderDrawImpl` 未做任何像素写入。

**修复**：偏移改放到源 crop：
```cpp
ShaderDraw<PaletteShift>(
    ShaderSurface(hdScreen->getTextOverlaySurface(), 0, 0),
    ShaderCrop(SurfaceCrop(glyph), dx, dy),
    ShaderScalar(color), ShaderScalar(mul), ShaderScalar(mid));
```
（与位图路径 [Text.cpp](file:///E:\Code\XCOM\tmp\OpenXcom-oxce-plus\src\Interface\Text.cpp#L754) 一致：目标用 `(dst,0,0)`、偏移交给 `...chr.setX/setY` 源头。）

**验证**（软件渲染、3840×2160×8bpp）：
- `HD-DIAG-DST: dx=310 dy=60 overlay=3840x2160 glyphbox=108x48 clip=in inkOnOverlay=260`：字形墨迹已落入覆盖层。
- `HD-DIAG-FLIP: blit=0 overlay=3840x2160 bpp=8 nonzero=74059 pal[134]=(100,204,188)`：合成把整屏 HD 文字（74,059 个非零墨迹像素）真正 blit 到位，且调色板显示为可见的青绿色文本色。

至此 HD 文本在软件渲染下**已端到端可见**，后续进入 P5 视觉验证（截图对比 + 中文溢出检查）。

## 6. 复现 / 回放建议

- 重建前记录源码 mtime 与依赖 dll 版本，与本表第 2、4 节比对。
- 加固方向：见 `HD_UI_Plan.md` 规划阻塞项 B2（打包 TTF 进数据目录并直接配置路径）。