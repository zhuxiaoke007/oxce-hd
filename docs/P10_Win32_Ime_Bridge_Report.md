# P10 实现报告 — 游戏内 TextEdit 中文输入法支持（Win32 IME 桥）(2026-08-25)

## 需求

游戏内需要输入文字的对话框（如「基地命名」）无法使用中文输入法：
SDL 1.2 不支持 IME——中文输入法合成的结果经 `WM_IME_CHAR` /
`WM_IME_COMPOSITION` 消息到达窗口，SDL 的窗口过程不消费它们，
字符丢失（英文/ASCII 不受影响）。

## 方案（Engine/Win32Ime 桥）

零引擎改动地打通 IME 结果到游戏文本输入：

1. **窗口子类钩子**：`Screen::resetDisplay` 在 `SDL_SetVideoMode` 成功后
   调用 `Win32Ime::attach()`——枚举本进程可见顶层窗口（SDL_app），用
   `SetWindowLongPtr(GWLP_WNDPROC)` 挂载自己的 `imeWndProc`，其余消息
   全部转交 SDL 原窗口过程；显示重置重建窗口时自动重挂（幂等）。
2. **拦截两个 IME 结果消息**：
   - `WM_IME_CHAR`：wParam 为 UTF-16 码元（含 surrogate 配对处理）；
   - `WM_IME_COMPOSITION(GCS_RESULTSTR)`：从 IME 上下文取整段结果串
     （覆盖现代输入法走 composition 结果的路径）。
3. **转码注入**：UTF-16 → UCode（支持代理对），以
   `SDL_PushEvent(SDL_KEYDOWN{ keysym.unicode = UCode })` 推入事件队列。
   `TextEdit` 本就读 `key.keysym.unicode` 并做 `isValidChar` 校验
   （TEC_NONE 允许 ≥160），因此**游戏侧零改动**；普通按键
   （回车/退格/Backspace/ASCII）不走此路径，行为不变。

## 验证（自动化，端到端）

新游戏 → 选基地地点 → 基地命名对话框 → 以 `WM_IME_CHAR` 注入字符
（Python ctypes 精确传参；PowerShell IntPtr 会错误截断值，不可用于测试）：

| 注入 | 日志（Win32Ime: injected） | 屏幕上看到的输入框内容 |
|------|----------------------------|------------------------|
| 0x41 (A) | U+65 | `A` |
| 0x57FA 基 | U+22522 | `A基地工` |
| 0x5730 地 / 0x5DE5 工 | U+22320 / U+24037 |（同上，全部显示） |

输入法结果字符完整到达 TextEdit 并显示，与真实 IME 的投递路径一致。

## 文件

| 文件 | 说明 |
|------|------|
| `Engine/Win32Ime.h/.cpp` | IME 桥（`#ifdef _WIN32`，imm32 链接） |
| `Engine/Screen.cpp` | `Win32Ime::attach()` 挂接点（窗口创建/重置后） |
| `src/CMakeLists.txt` | 新增 `Engine/Win32Ime.cpp`；`WIN32_LIBS` 追加 `imm32` |

## 限制说明

- 仅 Windows。IME 候选窗口由系统默认处理（消息转交 DefWindowProc 链 + IME）。
- 兼容 BMP 常规汉字/符号；非 BMP（生僻字，需代理对）已按 surrogate 处理。
- 默认合成日志：每个注入字符 `Win32Ime: injected U+…`（INFO，用于可追溯）。
