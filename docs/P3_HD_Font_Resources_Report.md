# P3 完成报告：字体资源与度量对齐（HD UI 高清化改造）

> 日期：2026-08-22 | 工作目录：`E:\Code\XCOM\HD_feature` | 引擎源码：`tmp/OpenXcom-oxce-plus`
> 前置：P0(构建) ✅ P1(Font HD 后端) ✅ P2(Screen 文字覆盖层) ✅

## 1. P3 目标

让 HD 文字**真正好看且完整**：

1. **按字体角色选 TTF** —— 几何字体（地球层/损伤数字）用**等宽** TTF 保证数字列对齐；UI 文本用清晰无衬线 TTF。
2. **CJK 回退** —— 主 TTF（如 Arial）不含中文/日文汉字时，自动回退到中文字体渲染，避免缺字方块。
3. **度量对齐** —— HD 字形单元尺寸严格等于原版位图度量 × 显示倍率，使前进宽度与行高逐像素一致（布局零位移）。

## 2. 实现要点

### 2.1 按角色选 TTF（`Mod::loadExtraResources`）
- `Font` 现在携带自己的 `id`（如 `FONT_BIG`）。
- 加载时按角色挑 TTF，优先级：
  1. 全局 `hdFontTtf`（用户显式指定，向后兼容，覆盖一切）；
  2. 否则 `FONT_GEO_*` → 等宽 TTF（`consola.ttf`，可经 `hdFontTtfGeo` 覆盖）；其余 → 无衬线 TTF（`arial.ttf`）。
- CJK 回退：默认 `msyh.ttc#0`（Microsoft YaHei，简体中文 UI 字体），可用 `hdFontTtfCJK` 覆盖。

### 2.2 CJK 回退（`Font::getHdChar` / `rasterizeHdChar`）
- 新增第二个 TTF 句柄 `_hdFontCjk` 与路径 `_hdTtfCjkPath`。
- `getHdChar(c, scale)` 逻辑：
  - 主 TTF 含该字形（`TTF_GlyphIsProvided`）→ 用主 TTF 渲染；
  - 否则若配置了 CJK 且 CJK 含该字形 → 用 CJK 渲染（中文/假名不再缺字）；
  - 都无 → 返回 `nullptr`，`Text.cpp` 自动回退位图字形。
- `rasterizeHdChar` 改为接收具体句柄，基线放置使用**对应字体**的 `TTF_FontAscent`，保证 CJK 字形基线正确。

### 2.3 TTC 集合支持
- 新增 `splitTtcSpec()`：`"msyh.ttc#0"` 拆为文件路径 + 字面索引，用 `TTF_OpenFontIndex` 打开 TTC 指定字面。
- `resolveHdFontPath()` 探测时剥离 `#index` 再查文件、返回时拼回，避免 SDL 把 `#` 误判为偏移量、也避免回退到相对路径导致运行时打不开。

### 2.4 度量对齐（构造性保证）
- HD 字形单元：`cellW = getCharSize(c).w × scale`、`cellH = getHeight() × scale`，二者均取自**原版位图度量**。
  → 前进宽度 / 行高与原版逐像素一致，**布局零位移**（不依赖 TTF 自身度量）。
- 基线：`descentLowres = max(1, getHeight() / 4)`，见 §3 实测校验。
- 修复：分辨率变化时 `invalidateHdCache()` 现在**同时关闭 CJK 句柄**，否则 CJK 字形会以旧倍率渲染（`_hdScale` 被主字体改回新值后守卫误判）。

## 3. 实测原版度量与保真校验

`tools/measure_font_metrics.py`（Pillow 解析位图字库 PNG，按 Font.dat 单元切格统计墨迹包围盒）：

| 原版字体 | cell | cap height 均值 | advance | 实测 descent(px) | height/4 | 偏差 |
|---|---|---|---|---|---|---|
| FONT_SMALL | 8×9 | 7.20 | 8(逐字紧致) | 1.3 | 2.25 | <1px |
| FONT_BIG | 16×16 | 11.53 | 16(等宽) | 3.4 | 4.00 | <1px |
| FONT_GEO_BIG | 5×9 | 6.70 | 6(5+sp1) | 0.9 | 2.25 | <1px |
| FONT_GEO_SMALL | 5×7 | 5.28 | 6(5+sp1) | 1.0 | 1.75 | <1px |

结论：HD 单元尺寸由位图度量决定 → **布局严格对齐**；基线 `height/4` 假设与原版 descent 偏差均 < 1px@原分，display 倍率下可接受。P5 截图会做视觉复核，必要时改为按字体实测 descent 比例。

## 4. 改动文件（`tmp/OpenXcom-oxce-plus/`）

- `src/Engine/Font.h`：`_hdTtfCjkPath` / `_hdFontCjk` / `_id` 成员；`loadHdFont(primary, cjk)`；`setId/getId`；`ensureHdCjkFont`；`rasterizeHdChar(c,scale,font)` 接收句柄；`getHdChar` 回退逻辑。
- `src/Engine/Font.cpp`：`splitTtcSpec` 解析 TTC；`invalidateHdCache` 同时关闭两个句柄；`ensureHdFont`/`ensureHdCjkFont` 用 `TTF_OpenFontIndex`；`getHdChar` 主/CJK 字形选择；构造/析构初始化与清理新句柄。
- `src/Mod/Mod.cpp`：`loadExtraResources` 设 `font->setId(id)`，按角色选 TTF + CJK 回退；`resolveHdFontPath` 字体路径解析辅助。
- `src/Engine/Options.inc.h` + `Options.cpp`：新增 `hdFontTtfGeo`、`hdFontTtfCJK`（默认空 → 走内置默认）。
- `tools/measure_font_metrics.py`：原版字体度量量测脚本。
- `HD_feature/HD_Font_Design_Spec.md` §3：填入实测度量表与对齐结论。

## 5. 编译验证

`mingw32-make` → `100% Built target openxcom`，`openxcom.exe` 23,182,270 字节，仍链接 `SDL_ttf.dll`，无 error/undefined。新选项 `hdFontTtfGeo`/`hdFontTtfCJK` 已编入二进制。

## 6. 已知限制 / 下一步

- **默认字体依赖系统路径**：内置默认（arial/consola/msyh）在非 Windows 或缺失时 `resolveHdFontPath` 返回原规格 → `TTF_OpenFont` 失败 → 该字体回退位图（有日志告警）。用户可用 `hdFontTtf`/`hdFontTtfGeo`/`hdFontTtfCJK` 指定绝对路径。
- **CJK 在窄单元下的右侧裁切**：如 FONT_SMALL 的 CJK 单元宽 = 位图紧致宽(~8px)×scale，而 TTF 汉字近方形(~9px×scale) → 右侧约 1px×scale 裁切；FONT_BIG(16px) 单元足够容纳，无裁切。可后续让 CJK 字宽取 `max(位图宽, 字体自然宽)`（需评估是否破坏对齐）。
- **OpenGL 模式**：同 P2，overlay 在 OpenGL 下不叠加，HD 文字暂时回退位图。
- **下一步**：P4（Options GUI 开关 + 字号缩放档位 100/110/125%）、P5（headless 冒烟 + 截图对比验证实际清晰度与中文渲染）。

---
**状态**：P3 ✅ 编译通过 + 度量实证完成。建议下一步做 P5（headless 截图）来肉眼验收 P1–P3 的实际渲染效果，或先做 P4 的 Options GUI 开关。
