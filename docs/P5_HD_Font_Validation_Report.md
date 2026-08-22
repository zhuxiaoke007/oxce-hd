# P5 验证报告：HD 字体实际渲染

**日期**：2026-08-22  
**验证目标**：确认 P1–P3 实现的 HD/TTF 字体后端能够真正输出清晰、可读、不缺字的中文与西文，且相对旧版放大路径有肉眼可见的清晰度提升。

---

## 1. 验证方法

### 1.1 为什么不用 `tools/run_checks.py --screenshots`
`tools/run_checks.py` 是 **Godot 复刻** 的入口，与 OpenXcom/OXCE C++ 引擎的 HD 字体特性无关。因此 P5 需要针对 C++ 引擎单独设计验证。

### 1.2 采用的方案：独立离屏渲染验证器
在 Windows 沙箱中直接启动完整 GUI 游戏并自动触发 F12 截图（`Screen::screenshot()`）不可靠，原因：
- 需要真实显示或可用的虚拟帧缓冲；
- 需要让游戏自动进入有文字的界面并触发截图键；
- 没有现成的 headless/autotest 入口。

因此新建独立验证器：
- 路径：`tools/hd_font_preview/`
- 技术栈：SDL 1.2 + SDL_ttf 2.0.11（**与引擎完全一致**），`SDL_VIDEODRIVER=dummy` 离屏运行；
- 输出：PNG 预览图。

该验证器**逐行复用了引擎中的同一套算法**：
- `TTF_OpenFont` / `TTF_OpenFontIndex`（支持 `msyh.ttc#0` 这种 TTC 索引写法）；
- 字号一次线性修正 `corrected = target * target / height`；
- `TTF_SetFontHinting(..., TTF_HINTING_LIGHT)`；
- `TTF_RenderUTF8_Shaded` 生成 8-bit coverage 表面；
- 扫描 coverage ≥ 128 得到 ink bbox；
- 基线公式 `cellBaseline = cellH*scale - max(1, cellH/4)*scale`、`dstY = cellBaseline - ascent + minY`；
- CJK 回退：`TTF_GlyphIsProvided` 检测主字体缺字 → 切换到 `msyh.ttc#0`；
- overlay 合成：先在 320×200 低分辨率背景上绘制/缩放，再按 `baseToDisplay(x,y) = (x*scale, y*scale)` 把 HD 字形覆盖上去。

验证器不是链接引擎，而是**用同一逻辑独立证明**：TTF→8-bit ink=1 管线和 P2 的 overlay 合成数学都能正确工作。

### 1.3 验证内容
渲染一张模拟 UI 画面，包含：
- 标题 `X-COM: UFO DEFENSE`（FONT_BIG，Arial）
- 状态行 `MISSION CONTROL // GEOSCAPE STATUS`（FONT_SMALL，Arial）
- 混排 CJK：`Aliens near 东京 and 莫斯科 detected`（FONT_SMALL，Arial + 微软雅黑 CJK 回退）
- 地球层数字：`FUNDS $1,234,567  SCORE 089  TIME 23:59`（FONT_GEO_BIG，Consolas 等宽）
- 中文长句：`中文显示测试：外星人防卫计划 · 高清字体后端验证`（FONT_SMALL，CJK 回退）

每个场景生成 **2x (640×400)** 与 **3x (960×600)** 两个缩放级别，并同时生成 **legacy** 版本作为对照：把同一段文字以 scale=1 渲染进 320×200 再 nearest-neighbor 放大，模拟旧引擎直接放大低分辨率文字的效果。

---

## 2. 验证结果

### 2.1 HD 2x 预览
`HD_feature/screenshots/p5_hd_preview/hd_2x.png`

![hd_2x](p5_hd_preview/hd_2x.png)

- 标题、正文、数字全部清晰锐利；
- 中文/日文地名完整渲染，无方块、无 fallback 失败；
- Geo 数字使用 Consolas 等宽，列对齐。

### 2.2 Legacy 2x 对照
`HD_feature/screenshots/p5_hd_preview/legacy_2x.png`

![legacy_2x](p5_hd_preview/legacy_2x.png)

- 同一文字以 1x 渲染后再放大 → 明显模糊；
- 中文行几乎不可辨认，证明旧放大路径对 CJK 极不友好。

### 2.3 HD 3x 与 Legacy 3x
- `hd_3x.png`：更高缩放下仍然锐利，CJK 字形细节完整；
- `legacy_3x.png`：放大后模糊进一步加剧。

---

## 3. 结论

| 检查项 | 结果 |
|---|---|
| TTF 栅格化到 8-bit ink=1 | ✅ 通过（与引擎同一套 binarize 阈值） |
| 字号修正使行高匹配原版单元 | ✅ 通过 |
| TTC 索引加载（微软雅黑 `#0`） | ✅ 通过 |
| CJK 回退（Arial 缺字 → 雅黑） | ✅ 通过，混排无方块 |
| 基线/下伸对齐 | ✅ 通过，多行文字水平整齐 |
| Geo 等宽数字 | ✅ 通过 |
| 相对旧版清晰度提升 | ✅ 显著，2x/3x 均肉眼可见 |
| 引擎内 overlay 合成 | ✅ 代码审查 + 编译通过；合成数学由验证器复现 |

**P1–P3 功能管线验证通过**。虽然真实游戏内截图受环境限制无法自动获取，但独立验证器以完全相同的 SDL_ttf 调用和合成数学产出了清晰的视觉证据，且引擎侧代码已编译通过并正确链接 `SDL_ttf.dll`。

---

## 4. 如何自己复现

```bash
export PATH="/d/Tools/msys64/mingw64/bin:$PATH"
cd "E:/Code/XCOM/tools/hd_font_preview"
g++ main.cpp lodepng.cpp -o hd_preview $(pkg-config --cflags --libs SDL_ttf) -O2
SDL_VIDEODRIVER=dummy ./hd_preview 2   # 生成 hd_2x.png + legacy_2x.png
SDL_VIDEODRIVER=dummy ./hd_preview 3   # 生成 hd_3x.png + legacy_3x.png
```

---

## 6. 游戏内验证（2026-08-22 补充）

独立验证器之外，最终构建（指纹 `31A44081`）已在真实游戏内验证：

- 主菜单 6 按钮、选项页分类与按钮文字全部可见（TextButton 标签回位图路径，见 `BUILD_FINGERPRINT.md` §5.5）；
- 战斗界面：地图、底栏状态数字（TU 绿/能量黄/生命红/士气蓝）、弹药数字正常渲染，HD 与 legacy 模式位图像素一致（§5.6 修复了 HD 合成对调色板的污染）；
- 地理球：右侧菜单（数据/基地/工表/UFO百科/装备/状态）与时间控制（五秒/5分/一小时/一分/三十分/1天）中文完整可读、无乱码；
- 截图存档：`screenshots/p5_in_game/`。

自动验证入口：`openxcom.exe -hdshot:<存档名>`（读档后停留场景，外部按 F12 截图；legacy 对照用 `hdFonts:false` 再跑一遍）。

## 5. 已知限制与后续

- 验证器是**离屏预览**，不是从真实游戏捕获；它证明的是后端逻辑与合成数学的正确性。真实游戏内 overlay 仍需要在实际运行时通过 `hdFonts: true` + `hdFontTtf`/Geo/CJK 选项激活。
- 标题（FONT_BIG）使用 Arial 导致字间距较宽；实际游戏可选用更适合的粗体 UI 字体，并通过 `hdFontTtf` 选项指定。
- 后续 **P4（Options GUI 开关）** 将提供可视化开关与字号缩放档位，降低用户手动改 `options.cfg` 的门槛。
