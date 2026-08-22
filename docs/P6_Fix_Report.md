# P6 — HD 字体缺陷修复报告（2026-08-23）

构建指纹（SHA-256 前 8 位）：`3aee641b`（`tmp/build/bin/openxcom.exe`，须软件渲染 + `hdFonts: true`）

## 结论

P0–P5 的技术路线（显示分辨率 TTF 覆盖层 + SDL_ttf/FreeType）**方向正确，予以保留**。
P5 报告"已达成目标"不成立，实测存在两个真实缺陷（见下），本次已修复并游戏内验证。

## 实测确认的缺陷（修复前）

1. **选项界面子选项文字不可见**。Advanced Options 的列表（TextList）与右侧描述，
   HD 开启时整体消失或错位；取值列完全缺失。legacy 对照同区域文字齐全。
   （截图 `p6_options_legacy.png` vs 修复前抓帧）
2. **各界面字体未真正高清化**。绝大多数静态文字只在其 `_redraw` 触发的帧
   （通常仅状态首帧）进入 overlay，而 `Screen::clear()` 每帧清空 overlay，
   随后永远消失或回退位图缓存，观感仍是像素字。

## 根因（代码级）

- **R1 缓存式绘制 vs 每帧清空的 overlay 架构性错配**：Surface 是"绘制一次、
  缓存缓冲区"的模型；overlay 每帧被清，但 Text 只在 `_redraw` 时重绘。
- **R2 内嵌 Text 坐标错误**：TextList 单元格 / TextButton 标签以相对坐标绘入
  父表面，HD 分支却把 `getX()+x` 当作屏幕绝对基坐标直绘 overlay（B5 曾用
  `setHdEnabled(false)` 回避 TextButton，TextList 未处理）。
- **R3 默认字体选型错误**：P3 后期把默认 TTF 换成了 ark-pixel-16px（像素风格
  矢量字），即使管线正常，渲染结果也仍是"像素风"，违背项目目的。

## 修复内容（源码 `tmp/OpenXcom-oxce-plus`）

| 文件 | 修改 |
|---|---|
| `Engine/Surface.h` | 新增 `setRedraw(bool)` |
| `Engine/State.cpp` | `State::blit`：HD 模式下对 Text/TextButton/TextEdit/TextList/ComboBox 每帧强制重绘（字形已有按 (字符,scale) 缓存，开销为普通 blit 级）→ 解决 R1 |
| `Interface/Text.h/.cpp` | 新增 `setHdOrigin(x,y)`；HD 分支改用 `origin + getX() + x` 映射 → 解决 R2（坐标） |
| `Interface/TextList.cpp` | `draw()`：为每个单元格设置列表绝对原点并强制重绘 |
| `Interface/TextButton.cpp` | 标签恢复 HD 路径：设置按钮原点 + 每帧重绘（替代旧的禁用方案） |
| `Mod/Mod.cpp` | 默认字体改为平滑矢量字：UI=arial / Geo=consola / CJK=msyh(#0)/simhei，
  逐候选解析存在性，bundled ark-pixel 与 wqy 仅作最后兜底 → 解决 R3 |
| `HdValidationHook.h` | 验证钩子扩展：存档名以 `opt` 开头时自动推送 OptionsAdvancedState |

## 游戏内验证（1600×1000 窗口，`-hdshot:opt1.sav` 自动读档进选项界面）

- Advanced Options：标题、左列 20 行选项名、取值列、右侧描述全部可见
  （`p6_options_hd_fixed*.png`；legacy 对照 `p6_options_legacy.png`）。
- 运行日志 overlay 墨迹每帧 ~18k 且持续波动（每帧重绘正常工作）。
- 地理球界面 HD 文字正常（`p6_geoscape_hd.png`）。
- 验证方式：窗口截屏 + 逐区域像素/ASCII 分析对比 HD 与 legacy 两轮运行。

## 遗留事项（不阻塞目标）

- TextEdit（输入框）仍走位图路径；OpenGL 模式下 HD 整体回退位图（原有设计）。
- 长字符串在列表单元格内可能溢出单元格宽度（HD 前进宽度 ×1.2，单元格为位图宽度），
  与修复前行为一致，未观察到实际溢出问题。
- `hdshot` 调试钩子保留（仅 `-hdshot:` 参数激活），非发行功能。
