# P7 — HD 覆盖层串层（文字穿透弹窗）修复报告（2026-08-23）

构建指纹（SHA-256 前 8 位）：`c639291b`（`tmp/build/bin/openxcom.exe`，软件渲染 + `hdFonts: true`）

## 问题（用户报告，已全部复现并确认同根因）

1. UFO 百科点开分类子页（物品选择弹窗）时，底层界面的"选择物品 / UFO百科 /
   分类按钮"文字以浮层形式叠在弹窗上（用户截图 + 复现截图
   `p7_ufopaedia_bleed_before.png`，视觉模型确认渗入内容与用户描述一致）。
2. 选项界面点开子选项，同样出现其他界面文字重叠。
3. 修建初始基地时右侧渗出本不应显示的基地指令文字。

## 根因

P6 引入的"每帧强制重绘"与 overlay 的合成位置共同造成：

- OpenXcom 状态栈渲染时，从最上层 screen 状态到栈顶的**所有状态每帧都 blit**
  （弹窗式状态叠在底层状态之上）。
- P6 修复后，这些状态的文字**全部**每帧重绘进 HD overlay。
- overlay 在 `Screen::flip` 里合成在**最终画面最顶层**——于是被弹窗窗口遮住的
  底层状态文字穿透了弹窗的不透明背景，形成串层。

P5 之前无此问题只是因为底层状态文字根本不会每帧重绘（叠加态下 overlay
只有首帧墨迹，随即被清空）——即旧的"不可见"bug 掩盖了这一架构错配。

## 修复（`tmp/OpenXcom-oxce-plus`）

| 文件 | 修改 |
|---|---|
| `Engine/Screen.h/.cpp` | 新增 `_hdOverlayActive` 门控 + `setHdOverlayEnabled()`；`getTextOverlaySurface()` 仅在启用时返回 overlay（否则 Text 自动回退位图路径） |
| `Engine/Game.cpp` | 渲染循环中，blit 每个状态前设置门控：**只有 blit 序列中最顶部的状态可写 overlay**，下层状态回退位图（位图按 blit 顺序绘制，天然被上层窗口正确遮挡） |
| `Engine/State.cpp` | 每帧强制重绘仅对"overlay 可用"（即顶层）状态的文字表面生效；下层状态保持缓存，不再白白重绘 |
| `HdValidationHook.h` | 验证钩子新增 `ufop` 前缀场景：自动压入 UfopaediaStartState + UfopaediaSelectState 复现串层 |

设计取舍：弹窗打开期间，弹窗外围露出的底层界面文字临时以位图（低清）显示，
弹窗关闭后恢复 HD。这是避免串层的正确折衷——位图路径的遮挡语义由 blit 顺序
天然保证。

## 验证（视觉模型 agnes-2.5-flash 识图 + 像素分析，1600×1000 窗口）

- **UFO 百科场景**（`-hdshot:ufop1.sav` 自动复现）：
  修复前 `p7_ufopaedia_bleed_before.png` ——"选择物品/UFO百科/X-COM载具与军备/
  底部按钮"渗入弹窗；
  修复后 `p7_ufopaedia_fixed.png` —— 识图对比结论："渗入弹窗的串层文字已消失，
  弹窗自身文字完整可读，无其他渲染异常"。
- **选项界面回归**（`-hdshot:opt1.sav`）：`p7_options_after_fix.png` —— 分类按钮、
  20 行选项、右侧说明完整可读，左上角原先渗出的地球仪文字消失（识图发现的
  "遮挡"仅为截屏时桌面终端/输入法窗口，非游戏内串层）。
- 用户报告的三个场景（UFO 百科/选项子页/修建基地弹窗）同为"非 screen 弹窗
  状态叠加"路径，由同一处修复覆盖。

## 验证工具备注

本轮起接入 Agnes 视觉模型（agnes-2.5-flash，经本地 MCP 服务器
`C:\Users\MSI\.zcode\mcp\agnes_mcp_server.py` 调用）做游戏截图的语义级验证，
替代此前的 ASCII 像素推断。
