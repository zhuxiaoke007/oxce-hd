# X-COM UI 高清化改造 — 项目计划

> 创建日期：2026-08-20 | 工作目录：`E:\Code\XCOM\HD_feature`
> 目标引擎：**C++ OpenXcom/OXCE**（本地源码 `tmp/OpenXcom-oxce-plus`）
> 实现方案：**A — 显示分辨率文字覆盖层（Display-Resolution Text Overlay）**
> 首期范围：**仅字体高清**（UI 面板/精灵保持像素风）

## 1. 背景与目标
当前 OpenXcom/OXCE 把全部内容（含字体）渲染进一个低分辨率内部缓冲（默认 **320×200**），再整体放大到窗口显示。字体被迫以低分辨率点阵存在、被一同放大，导致**显示模糊、可读性差**。
目标：在不破坏原版像素美术与玩法的前提下，让 UI 文字在高分辨率显示器上清晰可读。

## 2. 技术评估（基于 `tmp/OpenXcom-oxce-plus/src` 源码）
- 渲染管线：`buffer(320×200)` → `Zoom::flipWithZoom`（`Screen::flip`）放大 → 屏幕。
- 字体 = 位图精灵图集（`FontImage` = `Surface` 图集 + YAML 字符表），`Font::getChar` 裁切单字后 blit 进低分辨率 buffer。
- 着色：`Text::draw` 用 `ShaderDraw<PaletteShift>` 把字形（8-bit，墨迹=索引1、透明=0）按 `_color` 重新映射颜色。
- 既有“高清”能力（`useHQXFilter`/`useXBRZFilter` 32-bit scaler、OpenGL 平滑）改善的是像素放大质感，但**字体若仍以位图存在于低分辨率 buffer，放大后依旧糊**。
- 关键约束：把 TTF 栅格化进 320×200 buffer，放大后依旧糊。**要真高清，文字必须在显示分辨率层渲染。**
- 源码确认：OpenXcom 字体系统为纯位图，无真实 FreeType 文本路径；`Text` 着色完全依赖 8-bit 字形的 `PaletteShift` 约定（墨迹=索引1、透明=0），替换字形必须保持该约定。

## 3. 推荐方案（方案 A）：显示分辨率文字覆盖层
- 在 `Screen` 新增一块与**显示分辨率同尺寸、32-bit** 的 “text overlay” 表面。
- `Font` 增加可选 HD 后端：启用且配置 TTF 时，`getChar`/文字绘制用 **SDL_ttf（FreeType）在显示分辨率下栅格化字形**；字形输出为 8-bit 表面（墨迹=索引1、透明=0），**复用现有 `PaletteShift` 着色**（`Text` 逻辑不变）。
- 文字定位沿用现有缩放变换（`_scaleX/_scaleY` + `DX/DY`），把 `Text` 绘制重定向到 overlay 而非低分辨率 buffer。
- `flip()` 中：先放大像素 buffer → 再阿尔法叠加 text overlay。
- 优点：像素美术（面板/精灵）保持复古放大、文字单独高清；**不动整体 UI 布局，风险可控**；着色机制 100% 复用；与现有 hqx/xbrz/OpenGL 平滑共存。

## 4. 备选方案对比（已否决，仅供参考）
| 方案 | 说明 | 取舍 |
|---|---|---|
| A（采用）文字覆盖层 | TTF 在设备分辨率渲染后叠加 | 风险低、针对性强、保留像素美术 |
| B 全高 DPI 基准分辨率 | 提高 base 分辨率并整体重排 UI | 全局真高清，但需重排全部 320×200 UI 坐标/资源，工作量巨大、易布局 bug |
| C 高清位图字体图集 | 仅替换更高分辨率位图字体图集 | 最简单，但仍在低分辨率 buffer 内被放大模糊；只有配合 B 才彻底 |

## 5. 实施阶段
- **P0 构建环境**：用本地 `tmp/OpenXcom-oxce-plus` 建立可编译/运行的 OXCE 工程（CMake），确认能产出与现 `out/oxce` 等价的可运行体；集成 SDL_ttf/FreeType。
- **P1 字体子系统扩展**：`Font` 增加 HD/TTF 后端 + 8-bit 调色板兼容输出；`Text` 绘制路由到 overlay。
- **P2 Screen 文字覆盖层**：新增 32-bit overlay 表面、缩放定位、`flip()` 合成；`PaletteShift` 着色贯通。
- **P3 字体资源**：选取/制作 TTF（英文 + 中文 CJK，复用既有 zh-CN 需求），度量（高度/字距）尽量对齐原版 `FontSmall/Big/Geo*`，减少布局位移。
- **P4 选项与开关**：`Options` 增加 “HD Fonts” 开关、字号缩放、回退原版位图字体（默认关闭保兼容）。
- **P5 验证**：截图对比（主菜单/基地/地球层/战斗底栏/百科）字体清晰度、着色正确、布局无溢出；回归原版位图路径（开关关闭行为不变）。

## 6. 风险与缓解
- 文字度量差异致布局位移 → 选 TTF 尺寸使 cap height/advance 与原版位图尽量一致；提供字号缩放；先英文后中文。
- 着色兼容 → 字形强制 8-bit、墨迹=索引1、透明=0，复用现有着色（与 `zhcn_font` 逻辑一致）。
- 性能 → overlay 仅文字区、按需重绘、可缓存字形位图。
- 许可/构建 → GPL-3.0 个人项目；基于本地 OXCE 源码分支。

## 7. 交付物
改造后的 OXCE 引擎（可运行 exe）+ “HD Font” 选项/配置 + 字体资源（TTF 及度量说明）+ 验证截图/对比报告 + 本计划与实现文档（Markdown，存于 `HD_feature/`）。

## 8. 决策记录
- 2026-08-20：用户确认目标引擎=C++ OpenXcom/OXCE、方案=A 显示分辨率文字覆盖层、范围=仅字体高清。计划落盘至 `HD_feature/HD_UI_Plan.md`，进入 P0 构建环境评估。
