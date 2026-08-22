# X-COM UI 高清化改造 — 辅助资源配置档案

> 目标：对 OpenXcom/OXCE 的 UI 进行高清化改造（显示分辨率文字覆盖层方案，A 方案），
> 解决低分辨率位图字体放大导致的模糊与可读性问题。
> 本档案记录为该项目调研、安装、配置的技能 / MCP / 专家资源，及其协同方式。

---

## 1. 资源调研结论（2026-08-20）

| 资源类型 | 调研方式 | 结果 |
|---|---|---|
| 技能（BuiltinMarket） | `workbuddy_marketplace_skill` 检索 `C++` / `image screenshot` / `font typography` | **0 命中**，无匹配的游戏引擎/字体/图像处理技能 |
| MCP / Connectors | `search_plugins`(connector) | 绝大多数为企业/金融 SaaS；仅 GitHub(已连)、TAPD、awesun 与本项目相关 |
| 领域专家 | `search_plugins`(expert) | 命中 3 个高度相关：像素君 / 软件工坊 / 鹏城信息AI专家 |

**结论**：市场无现成技能可装；本地已有技能足以覆盖开发闭环。需新增的是「专家」与「协作型 MCP」，二者均需用户在面板启用（卡片通道本次被会话 pending 状态阻塞，见第 6 节）。

---

## 2. Skills 配置

### 2.1 市场技能
无需安装（无匹配项）。若后续出现 OpenXcom/SDL/C++ 相关技能，再行评估。

### 2.2 本地可用技能（已就绪，映射到阶段）
| 技能 | 路径 | 在本项目中的作用 | 对应阶段 |
|---|---|---|---|
| `spec-workflow` | 本地 | 需求分析/技术设计/任务规划（本计划已用） | 全周期 |
| `tdd` | 本地 | 红-绿-重构，编写覆盖层/着色的单元测试与回归 | P1 / P2 / P5 |
| `cli-anything-hub` | 本地 | 发现并调用构建类 CLI（需本地存在工具链） | P0 |
| `playwright-browser-automation` | 本地 | 截图采集与对比（回归验证，P5） | P5 |
| `skill-creator` | 本地 | 把本项目可复用流程沉淀为技能 | 收尾 |
| `prompt-engineering-expert` | 本地 | 优化专家/代理提示词 | 全周期 |
| `github` | 本地 + MCP | 仓库 / Issue / PR 协同（已连，已建仓） | 全周期 |
| `mcporter` | 本地 | 直接调用 MCP server/工具 | 全周期 |

---

## 3. MCP / Connectors 配置

### 3.1 已连接且已启用
- **GitHub**（账户 `zhuxiaoke007`）：承担代码版本控制与任务跟踪。
  - ⚠️ **GitHub MCP 集成（mcp__github）权限受限**：`create_repository` 返回 403（Resource not accessible by integration），仅 `get_me` 等只读调用可用。
  - ✅ **实际协同通道：本地 `gh` CLI**（已安装 v2.97.0，已以 `zhuxiaoke007` 认证，含 GITHUB_TOKEN）。
    - 已创建私有仓库 **`https://github.com/zhuxiaoke007/xcom-hd-ui`**
    - 已建 **P0–P5 共 6 个 Issue**：#1 构建环境 / #2 Font HD后端 / #3 Screen覆盖层 / #4 字体资源 / #5 选项开关 / #6 验证回归
  - 后续协同：本地 `gh` 做 repo/issue/PR；MCP 仅用于只读查询。

### 3.2 推荐但未连接（待用户在连接器面板启用）
| 连接器 | 用途 | 对本项目的价值 |
|---|---|---|
| **TAPD** | 需求/缺陷/任务/迭代管理 | 将 P0–P5 与回归问题纳入更完整的研发看板（GitHub Issue 已可替代，按需启用） |
| **向日葵远程控制 (awesun)** | 命令行管理远端 Windows 设备、远程截屏、文件传输 | **关键桥接**：本沙箱无 C++ 工具链，可借此连到装有 VS/CMake 的本地开发机进行编译与截图回归 |

### 3.3 已知环境约束（必须在计划中体现）
- 本执行沙箱 **无 `cl` / `cmake` / `g++` / `mingw`**，且 OXCE `deps/` 未含 `SDL_ttf`。
- 因此 **P0 构建环境无法在本沙箱完成**：需在用户本地 Windows 开发机（VS2022 + CMake + SDL/SDL_ttf 1.2）执行，或经 `awesun` 桥接远程执行。
- 截图回归（P5）同样依赖可运行体，需在上述构建机产出 exe 后采集。

---

## 4. 领域专家配置（推荐，待用户在专家面板启用）

> 同一会话仅可启用一位专家/专家团。以下按与本项目的相关度排序，供选择。

| 专家 | 名称 | 专长 | 对口目标 |
|---|---|---|---|
| **像素君** | `UiDesigner` | 设计系统、组件库、像素级完美、无障碍 UI | **最直接**：字体清晰度/可读性正是核心目标；像素美术保留 |
| **软件工坊** | `SoftwareWorkshop` | 6 角色：产品评审/代码审查/安全审计/QA测试/设计系统/调试运维 | 高清化改动后的**代码审查 + 回归验证**（质量保障） |
| **鹏城信息AI专家** | `GameDevelopmentStudio` | 策划/技术/美术/音频/质量/运营 全流程游戏开发团 | 多阶段（P0–P5）协同推进 |

**推荐启用顺序**：先用 `UiDesigner` 把关字体度量/可读性设计，再用 `SoftwareWorkshop` 做代码审查与回归。

---

## 5. 协同工作模型

```
计划层   spec-workflow  ──► 产出 HD_UI_Plan.md（已确认）
   │
开发层   本地技能(tdd/cli/skill-creator) + 专家(UiDesigner/SoftwareWorkshop)
   │         │ 编码 Font/Screen/Text 覆盖层
   ▼
构建层   本地开发机 (VS/CMake/SDL_ttf)  ◄── awesun 桥接（沙箱无工具链时）
   │         │ 产出 oxce-hd.exe
   ▼
验证层   playwright/PIL 截图对比 + tdd 单测 + SoftwareWorkshop 审查
   │
跟踪层   GitHub Issue(P0–P5) / TAPD 看板  ◄── 协同闭环
```

**质量门禁**：`tdd` 单测 + `SoftwareWorkshop` 代码审查 + P5 截图像素对比，三者齐备方可合入。

---

## 6. 待办 / 阻塞项
1. **推荐卡片通道阻塞**：本次 `suggest_plugin_install` 被会话 pending 状态拦截（后端 10s 超时 / "already pending"）。需在专家/连接器面板手动启用 `UiDesigner`(及备选)、`TAPD`/`awesun`，或清除 pending 后重试。
2. **P0 构建前置**：需确认本地开发机工具链（VS2022 + CMake + SDL/SDL_ttf 1.2），或启用 `awesun` 桥接。
3. **专家启用后**：将本项目约束（320×200 低分缓冲、PaletteShift 墨迹=索引1/透明=0、SDL1.2+SDL_ttf1.2）同步给专家，避免给出不适用建议。
