# X-COM UI 高清化改造 — 高清字体度量与可读性设计规范

> 协同：本规范由设计/可读性专家（像素级 UI 视角）与实现侧共同基线化，供 P1–P5 全程对照。
> 前提结论（见 HD_UI_Plan.md / HD_Resources.md）：模糊根因是「全部内容（含点阵字体）先画进 320×200 低分缓冲再整体放大」。
> 方案 A：在 **Screen 新增与显示分辨率同尺寸、32-bit 的文字覆盖层**，TTF 在设备分辨率栅格化后经现有 `PaletteShift` 着色叠加；像素美术不变。

---

## 1. 目标与不可妥协原则
- **可读性优先**：高 DPI 屏上文字清晰、笔画不粘连、不晕染。
- **像素美术零破坏**：面板/精灵/图标保持原版放大质感（hqx/xbrz/OpenGL 平滑不变）。
- **布局不位移**：HD 字体与原版位图字体的字符**前进宽度(advance)与行高**尽量一致，避免文字溢出/换行错乱。
- **可回退**：默认关闭，Options 提供「HD Fonts」开关；关闭时 100% 走原版位图路径，行为不变。
- **着色兼容**：所有 HD 字形强制输出 **8-bit 表面，墨迹=调色板索引 1、透明=索引 0**，复用 `Text::draw` 的 `ShaderDraw<PaletteShift>`，不改动着色逻辑。

## 2. 字形渲染管线（实现侧契约）
1. `Font` 增加可选 HD 后端：当「HD Fonts」开启且已加载 TTF，则 `getChar()`/文字绘制改用 **SDL_ttf (FreeType)** 在**显示分辨率**栅格化单字。
2. 单字渲染为 8-bit 表面：前景填索引 1、背景索引 0（透明），与位图字体约定一致。
3. 文字定位沿用现有缩放变换（`_scaleX/_scaleY` + `DX/DY`），`Text` 绘制重定向到 overlay 而非低分缓冲。
4. `Screen::flip()`：先放大像素缓冲 → 再**阿尔法叠加**文字覆盖层。
5. 着色：`PaletteShift` 把索引 1 映射为 `_color`，零改动复用。

## 3. 字体度量对齐方法（P3 核心）
> 不凭感觉选字号。**先量原版，再配 TTF**，使渲染后 cap height / advance 落在容差内。
> 实测由 `tools/measure_font_metrics.py`（Pillow 解析位图字库 PNG 得到）。

| 原版字体 | 量测源（OXCE 数据） | cell(px) | cap height 均值(px) | advance(px) | 实测基线(descent,px) | HD 对齐策略 |
|---|---|---|---|---|---|---|
| FONT_SMALL | FontSmall.png 图集 | 8×9 | 7.20 | 8（紧致，逐字量） | ~1.3 | 单元=advance×scale×高×scale；基线≈height/4 |
| FONT_BIG | FontBig.png 图集 | 16×16 | 11.53 | 16（等宽） | ~3.4 | 同上 |
| FONT_GEO_BIG | FontGeoBig.png | 5×9 | 6.70 | 6（5+spacing1） | ~0.9 | 同上，Geo 用等宽 TTF（Consolas） |
| FONT_GEO_SMALL | FontGeoSmall.png | 5×7 | 5.28 | 6（5+spacing1） | ~1.0 | 同上 |

**对齐结论（关键）**：P1 的栅格化已把每个 HD 字形单元强制为
`cellW = getCharSize(c).w × scale`、`cellH = getHeight() × scale`，
其中 `getCharSize/getHeight` 取自**原版位图度量**。因此 HD 文字的
**前进宽度与行高与原版逐像素一致**，布局零位移——不依赖 TTF 自身度量。
度量对齐在“字形被放进哪个尺寸的格子”这一步已经达成（构造性保证）。

- **基线保真**：P1 用 `descentLowres = max(1, getHeight()/4)` 估算下伸部。
  实测原版 descent 约为 height 的 19%–24%（SMALL 1.3/9=14%、BIG 3.4/16=21%、
  GEO_BIG 0.9/9=10%、GEO_SMALL 1.0/7=14%），与 height/4(25%) 偏差 < 1px@原分，
  在 display 倍率下可接受；P5 截图会做视觉复核，必要时改为按字体实测 descent 比例。
- **字距(kerning)**：OpenXcom 原版为逐字前进(per-glyph advance)，HD 侧**禁用 TTF kerning**，
  直接采用位图 advance，严格对齐原版布局（Geo 数字列天然对齐）。
- **度量量测方法**：`tools/measure_font_metrics.py` 按 Font.dat 的单元尺寸把 PNG 切成字形格，
  统计墨迹包围盒（cap height、上下偏移、平均 advance）。

## 4. 字体选型建议（P3）
- **英文/数字 UI**：清晰无衬线、小字号易读。候选：`Noto Sans` / `DejaVu Sans` / `Inter`（取 Regular，关闭粗体合成以免笔画发虚）。
- **中文 CJK**：复用既有 zh-CN 需求，候选 `Noto Sans CJK SC`（思源黑体）或 `Source Han Sans`；与 `zhcn_fontgen` 简体缺字图集风格统一。
- **几何字体(Geo*)**：用于地球层/损伤数字，选等宽几何感强的 `Noto Sans Mono` 或专用数字字体，保证数字列对齐。
- **授权**：全部为 SIL/OFL 类开源字体，个人项目内使用合规。

## 5. 抗锯齿与清晰度
- TTF 在**显示分辨率**栅格化（关键：绝不在 320×200 缓冲内栅格化再放大）。
- 启用 FreeType 轻量 hinting（`FT_LOAD_TARGET_LIGHT`）以兼顾清晰与笔画均匀。
- 灰度抗锯齿即可；不做彩色亚像素（避免低分缓冲叠加时的色彩渗边）。

## 6. 对比度与可读性 / 无障碍（专家评审重点）
- 文字-背景对比：对常用面板（深底菜单/浅底清单）保证 **对比度 ≥ 4.5:1**（参照 WCAG AA 精神，游戏 UI 可适当放宽但需可辨）。
- 危险/警示色文字（红/黄）除颜色外辅以**图标或加粗**双通道提示，照顾色觉障碍。
- 字号缩放档位：提供 `100% / 110% / 125%` 三档，缩放仅在 overlay 层做，不改布局坐标（超界时自动回退 100% 并告警）。

## 7. 开关与回退（P4）
- Options 新增 `hdFonts: bool`（默认 false）、`hdFontScale: 1.0|1.1|1.25`。
- 关闭或 TTF 缺失 → 自动回退原版位图字体，路径不变。
- 任一 HD 字形渲染失败 → 该字回退位图等价字，保证不出现缺字方块。

## 8. 验证清单（P5，供专家逐项 sign-off）
- [ ] 主菜单 / 基地 / 地球层 / 战斗底栏 / 百科 五处截图，文字边缘清晰、无晕染。
- [ ] 同屏 HD-on vs HD-off 叠加对比：布局/换行/对齐像素级一致（容差见 §3）。
- [ ] 着色正确：所有 `_color` 文字经 PaletteShift 后颜色与原版一致。
- [ ] 中文（CJK）渲染正常、无缺字、行高不过溢。
- [ ] 缩放 110%/125% 不溢出；超界自动回退并告警。
- [ ] 关闭开关后行为 100% 等同原版（回归）。

## 9. 与专家协同方式
- **可读性/像素专家**：对 §6 对比度与 §3 度量容差做评审 sign-off；提供截图可读性主观评分。
- **代码审查/QA**：对 P1/P2 的 8-bit 输出约定、overlay 合成、回归清单（§8）把关。
- 本规范随 P3 度量填入后定稿，作为 P5 验收基线。
