# OpenXcom Extended — HD 高清字体与 UI 增强（oxce-hd）

基于 **OpenXcom Extended (OXCE) 8.6.2** 的修改版：在不改动原版 320×200 像素
美术资源的前提下，为全部 UI 文字叠加一层**显示分辨率的矢量字体（TTF/FreeType）
渲染层**，解决中文等字体在整屏放大后"笔画不清、仅轮廓可辨"的问题。

- 修改版可执行文件：`bin/openxcom.exe`（Windows x64，MinGW-w64 构建）
- 完整修改源码：`src/`（与上游 OXCE 8.6.2 的差异即本项目的全部改动）
- 各阶段设计/实施/修复报告：`docs/`

## 1. 使用方法

### 1.1 准备游戏数据（版权资源，需自备）

本仓库只包含引擎、开源数据与 HD 字体资源，**不含**《UFO: Enemy Unknown》/
《Terror From the Deep》原版游戏文件。请将原版安装目录中的 `UFO/` 与 `TFTD/`
两个文件夹放入 `runtime/`（与 `common/`、`standard/` 同级），或用 `-data` 参数
指向已有数据目录（见 1.3）。

`runtime/` 内已包含可直接分发的部分：

| 目录 | 内容 | 来源 |
|---|---|---|
| `runtime/common/` | 语言文件、字体位图（含中文大字库）、HD 兜底字体声明 | OXCE 开源数据 + 本项目中文扩展 |
| `runtime/standard/` | xcom1/xcom2 规则集与标准 mod | OXCE 开源数据 |
| `fonts/` | HD 兜底 TTF（ark-pixel-16px、文泉驿 wqy-16px） | SIL OFL / GPL+embedding exception |

### 1.2 启动

双击 `bin/oxce-hd.bat`（推荐，脚本会自动挂载 runtime 并创建用户目录），
或手动：

```bat
cd bin
openxcom.exe -data ..\runtime -user ..\userdata
```

首次运行会在 `userdata/` 生成 `options.cfg`。

### 1.3 HD 字体选项（options.cfg 或游戏内 高级选项）

| 选项 | 默认 | 说明 |
|---|---|---|
| `hdFonts` | `true` | HD 矢量字体总开关；`false` 时完全回退原版位图字体 |
| `hdFontScale` | `100` | 字号百分比（100=与位图版面完全一致；放大易造成文字超框，见 §4） |
| `hdFontTtf` | 空 | 指定主 TTF 路径；空=自动选择（优先系统 Arial，缺失回退捆绑 ark-pixel） |
| `hdFontTtfCJK` | 空 | CJK 回退字体；空=自动（微软雅黑 msyh.ttc → 黑体 → 捆绑 wqy） |
| `hdFontTtfGeo` | 空 | 地球仪数字专用（等宽优先 Consola）；空=同主字体逻辑 |

**必须软件渲染**：`useOpenGL: false`。OpenGL 模式下 HD 文字自动回退位图
（overlay 不参与 GL 合成，见 §3 架构说明）。

### 1.4 命令行调试钩子（非发行功能）

`-hdshot:<存档名>`：启动后自动读取 `userdata/xcom1/<存档>` 进入对应场景，
存档名前缀决定附加验证场景：

| 前缀 | 行为 |
|---|---|
| `opt` | 进入 高级选项 界面 |
| `optv` | 进入 视频选项 界面（ComboBox 验证） |
| `ufop` | 压入 UFO 百科分类+条目两层（串层回归验证） |
| `selb` / `newb` | 压入 基地建设+初始设施选择（修基地流程验证） |
| `actm` | 延迟 25 秒后弹出武器射击菜单（战斗菜单验证） |

源码位置 `src/src/HdValidationHook.h`，仅 `__HDFONTS__` 构建生效，随 `-hdshot`
参数激活，正常游玩不受影响。

## 2. 技术栈

| 层 | 技术 |
|---|---|
| 引擎 | OpenXcom Extended 8.6.2（C++17） |
| 窗口/渲染 | SDL 1.2.16（软件渲染 + SDL_gfx 缩放） |
| HD 字体 | SDL_ttf 2.0.11（FreeType），UI 中文默认微软雅黑，西文数字与中文统一走 CJK 字面以保证混排大小一致 |
| 构建 | CMake ≥3.15 + MinGW-w64 GCC（MSYS2，`D:\Tools\msys64` 环境） |
| 运行平台 | Windows x64（独立 bin 目录，含全部 MinGW/SDL 运行库，无需安装依赖） |

### 2.1 从源码构建

```bat
set PATH=D:\Tools\msys64\mingw64\bin;D:\Tools\cmake\bin;%PATH%
cmake -S src -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cd build && mingw32-make -j8
:: 产物 build\bin\openxcom.exe
```

`src/src/CMakeLists.txt` 会通过 pkg-config 探测 SDL_ttf；找到则定义
`__HDFONTS__` 启用 HD 功能，找不到则构建出无 HD 的原版行为。

## 3. HD 渲染架构（速览）

1. `Screen` 持有一块与显示分辨率相同的 8-bit overlay 表面（colorkey=0）。
2. `Font` 增加可选 TTF 后端：按（字符 × 缩放）缓存栅格化字形；CJK 表意字符
   统一方形 advance（=字体行高），保留 TTF 自然字面位置，保证字距均匀、
   中西混排大小一致。
3. `Text::draw` 的 HD 分支把字形以调色板索引写入 overlay；布局/换行度量与
   位图路径共用同一 `getCharSize`。
4. 渲染循环每帧：清空 overlay → 仅**最顶层界面**的文字重绘入 overlay（下层
   界面回退位图路径，由 blit 顺序保证遮挡正确）→ `flip` 先放大像素缓冲，再把
   overlay 以索引拷贝合成到最顶层。
5. 嵌入式文字控件（TextList 单元格、TextButton 标签、ActionMenuItem、
   TextEdit、ComboBox、Slider、BattlescapeMessage、WarningMessage、BaseView）
   通过 `Text::setHdOrigin()` 获得宿主表面的绝对坐标并逐帧重绘。

详细设计与逐 bug 修复记录见 `docs/`（P1 后端 → P7 串层修复）。

## 4. 注意事项 / 已知限制

- **OpenGL 模式不支持 HD 文字**（自动回退位图）；`useOpenGLShader` 等选项无效。
- `hdFontScale > 100` 会导致中文文本超出按钮/列表框宽度，且控件存在"大字放
  不下自动降级为小字体"的引擎机制（`Text::setText`），**建议保持 100**。
- 战斗菜单等界面的中文行宽接近控件上限，未来若更换更长的翻译需同步检查
  `ActionMenuItem`（宽 272）等布局。
- `zhcn-font` 语言 mod 的词条曾整体多包一层引号，已清洗（`*.bak-quotes` 备份
  于原 mod 目录）；重新生成翻译时注意值不要再带装饰引号。
- 位图字库未收录的简体字在**位图模式**下仍显示为 `?`（引擎原行为）；HD 模式
  下由微软雅黑补全。
- 存档兼容性：未改动存档结构，与官方 OXCE 8.6.2 互通。
- 版权：引擎与 `runtime/common`、`runtime/standard`、`fonts/` 遵循各自原许可
  （GPL/OFL 等，见 `LICENSE.txt`）；`UFO/`、`TFTD/` 原版数据严禁入库。

## 5. 推送 GitHub

仓库已做本地 git 初始化（见根目录 `.git/`）。`.gitignore` 已排除：

- `userdata/`（玩家存档与配置）
- `runtime/UFO/`、`runtime/TFTD/`（版权资源）
- `docs/screenshots/` 之外的大体积验证截图按需保留

一键推送：

```bash
cd oxce-hd
git remote add origin git@github.com:<你的用户名>/oxce-hd.git
git push -u origin master
```

## 6. 目录结构

```
oxce-hd/
├── README.md            # 本文档
├── LICENSE*             # 沿用 OXCE 的 GPL 许可
├── bin/                 # 独立执行环境（openxcom.exe + 全部运行库 + 启动脚本）
├── fonts/               # HD 兜底 TTF（ark-pixel / 文泉驿）
├── runtime/             # 数据目录（common+standard 开源数据；UFO/TFTD 自备）
├── docs/                # HD 功能设计/实施/修复报告（P0–P7）
└── src/                 # 完整修改源码（OXCE 8.6.2 + HD 补丁）
```
