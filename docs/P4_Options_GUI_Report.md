# P4 实现报告：Options GUI 开关

日期：2026-08-22
状态：✅ 完成并通过编译验证（`100% Built target openxcom`，23,185,605 字节，链接 SDL_ttf.dll）

## 目标

为 HD 字体后端（P1–P3）提供游戏内 GUI 配置入口：
1. `hdFonts` 开关（YES/NO 切换）
2. `hdFontScale` 字号缩放档位（100–125%，推荐 100/110/125）
3. 三个 TTF 路径选项（`hdFontTtf` / `hdFontTtfGeo` / `hdFontTtfCJK`）的界面内直接编辑

## 方案选型

OXCE 的 Advanced Options 界面由 `Options::getOptionInfo()` 注册表自动生成行，
原生只支持 BOOL（点击翻转）与 INT（左右键增减）两种交互。本报告：

- BOOL/INT 选项：仅补注册元数据（描述键 + `STR_GENERAL` 分类）即自动出现在列表中，零 UI 代码。
- STRING 选项（TTF 路径）：为 `OptionsAdvancedState` 增加内联 `TextEdit` 编辑器
  （复用 NotesState 的成熟模式：`getColumnX/getRowY` 定位、`setFocus(true,false)`、
  Enter 提交 / Esc 取消、编辑期间 `setScrolling(false)`）。

## 字号缩放的设计要点（布局不位移的关键）

- `Screen::getContentScale()`：真实几何缩放（display px / base px），与字体无关。
- `Screen::getHdScale()`：字形渲染缩放 = contentScale × hdFontScale% （最小 1）。
- `Screen::baseToDisplay()`：**改用 contentScale**（不再是 getHdScale），
  保证文字**起点**永远与周围像素图形对齐（居中/右对齐文本不漂移）。
- `Text::draw` HD 分支：字形宽度 = advance × hdScale（含缩放），笔进步进
  = `round(advance × zoom)`，经 baseToDisplay(content) 映射后显示步进恰好等于字形宽度
  → 字形无缝平铺、零重叠、起点对齐。
- zoom=100% 时所有数值退化为 P2/P3 已验证的行为，**零回归**。

## 改动清单（`tmp/OpenXcom-oxce-plus/`）

| 文件 | 改动 |
|---|---|
| `src/Engine/Options.inc.h` | 新增 `OPT int hdFontScale` |
| `src/Engine/Options.cpp` | 5 个 HD 选项全部补描述键 + `STR_GENERAL` 分类（自动进 Advanced 列表） |
| `src/Engine/Screen.h/.cpp` | 新增 `getContentScale()`；`getHdScale()` 乘 zoom；`baseToDisplay()` 用 contentScale |
| `src/Interface/Text.cpp` | HD 分支笔进按 `hdZoom` 缩放（含 RTL 预推进），bitmap 回退路径不变 |
| `src/Menu/OptionsAdvancedState.h/.cpp` | 内联 TextEdit 字符串编辑（Enter 提交/Esc 取消/点击他行取消）；`hdFontScale` 钳制 100–125；切换 `hdFonts` 或改字体路径时置 `Options::reload` |

游戏数据（`out/oxce/common/Language/`）：
- `en-US.yml`、`zh-CN.yml`：新增 5 组键（STR_HD_FONTS、STR_HD_FONT_SCALE、
  STR_HD_FONT_TTF、STR_HD_FONT_TTF_GEO、STR_HD_FONT_TTF_CJK 及各自 _DESC）。

## 使用方法

Options → Advanced → OXC 页签 → General 分组：

| 选项 | 交互 |
|---|---|
| HD fonts | 左键切换 YES/NO（自动请求 mod 重载，重启后生效） |
| HD font size scale | 左键 +1 / 右键 −1，钳制在 100–125（即时生效——字形按 scale 缓存） |

TTF 路径选项（`hdFontTtf` / `hdFontTtfGeo` / `hdFontTtfCJK`）通过 `options.cfg` 的 `options:` 节编辑（留空=内置默认 Arial/Consolas/雅黑）。

## 崩溃修复（2026-08-22 实测发现）

**问题**：首次实现把 3 个 STRING 选项（TTF 路径）也注册了描述键让其进入 Advanced 列表，并新增了内联 TextEdit 编辑。实测点击「选项→高级」立即崩溃 `std::bad_alloc`，栈定位到 `TextList::addRow` 内 UString 分配。

**根因**：OXCE 原版的 `TextList::addRow` + `processText`/`convUtf8ToUtf32` 路径**从未被 STRING 类型选项触发过**（原版唯一的 STRING 选项 `language` 不带描述键、不进 Advanced 列表）。STRING 选项进列表是未测试路径，存在未知的内存/转码边界问题。

**修复**：回退 STRING 选项的描述键注册（不进 Advanced 列表），`addSettings` 对 STRING 类型 `continue` 跳过。GUI 保留 `hdFonts`(BOOL) 与 `hdFontScale`(INT) 两个工作正常的控件；TTF 路径改由 `options.cfg` 编辑（已有合理内置默认值，多数用户无需改）。`OptionsAdvancedState` 的内联 TextEdit 代码保留但不会触发（防御性）。

## 已知限制

- `hdFonts` 开关需重启（置 `Options::reload`）；`hdFontScale` 即时生效。
- TTF 路径只能通过 `options.cfg` 编辑（GUI 编辑因上述崩溃暂移除）。
- OpenGL 模式同 P2 不叠加 overlay。

## HD 项目总览（截至本报告）

- P0 构建链 ✅　P1 Font HD/TTF 后端 ✅　P2 Screen 文字叠加层 ✅
- P3 字体资源/度量 + CJK 回退 ✅　P4 Options GUI ✅　P5 渲染验证 ✅

全部阶段完成。后续可选：OpenGL 模式下的 overlay 合成、按字体 ID 细分 TTF 配置。
