# P8 修复报告 — 非整数缩放分辨率下 HD 文字错位（2026-08-24）

## 现象

窗口分辨率不是 320×200 整数倍时（如 800×600 = 2.5×），所有 HD 文字相对
按钮/面板系统性漂移：文字下移约半行按钮高度、右移数十像素；4K/整数倍
分辨率（如 1280×800 = 4×）正常。

## 根因

像素画面与 HD 文字用两套不同的缩放系数：

- **帧缓冲放大**（`Zoom::flipWithZoom` → SDL_gfx 定点数最近邻）使用**真实
  分数缩放**：800×600 → 2.5×，上下黑边各 50px，按钮实际画在
  `y_display = 50 + y_base × 2.5`。
- **HD 文字层**（`Screen::getContentScale`）把真实缩放 `lround` 成**整数**：
  2.5 → 3。文字盖章位置 `baseToDisplay = 黑边 + 坐标 × 3`，字形也按 3×
  栅格化（比周围像素美术大 20%）。

坐标离原点越远偏差越大（每 base 像素偏 0.5 显示像素），第一行按钮
（base y≈96）下错约 48px——正好压到下一行按钮上，与用户截图一致。
整数倍分辨率下取整无损，因此 4K 等场景看不出问题。

## 修复（scale 全链路 int → double）

| 文件 | 改动 |
|------|------|
| `Engine/Screen.h/.cpp` | `getContentScale` 改为按轴输出真实分数缩放；`getHdScale` 返回 double；`baseToDisplay` 用真实 sx/sy 逐轴映射（lround 到最近显示像素，误差 ≤1px） |
| `Engine/Font.h/.cpp` | `_hdScale` 缓存键、`ensureHdFont/ensureHdCjkFont/rasterizeHdChar/getHdChar` 的 scale 参数全部改 double；TTF 目标像素高 = `lround(字高 × scale)`，字形格宽高同理；基线下移量按真实比例取整 |
| `Interface/Text.cpp` | `hdScale` 局部变量改 double |

布局推进（base 坐标系的 advance 累加）不变，仍是位图度量，保证与
原版版面一致；hdFontScale 缩放只作用于字形尺寸与推进，不影响落点。

## 验证（800×600，主菜单，新旧 exe A/B 截图像素剖面）

| 检验项 | 旧版 | 新版 |
|--------|------|------|
| [新游戏] 按钮矩形 y 275-324 内文字条带 | 无；文字在 344-360（下方） | **295-309，正中** |
| [模组] 按钮矩形 y 415-464 内文字条带 | 428-444（偏上出界） | 438-448 居中 |
| 文字水平中心 | — | 273 vs 按钮中心 275（±2px） |
| 字形尺寸 | 3×（偏大 20%） | 2.5×（与美术一致） |

理论值核对：文字 base y=96 → 修复前 `50+96×3=338`（实测 344-360 ✓），
修复后 `50+96×2.5=290`（实测 295-309，含二值化阈值裁边 ✓）。

## 同步

- 源码：`oxce-hd/src`（权威）→ 已同步 `tmp/OpenXcom-oxce-plus`（构建树）
- 构建：`tmp/build/bin/openxcom.exe`（run_hd.bat 启动路径）
- 发布：已拷贝至 `oxce-hd/bin/openxcom.exe`（SHA256 一致）
