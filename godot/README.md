# PhyzBox Godot 前端：旅行者太阳系

Godot 4.6.3 主场景是仅保留紧凑时间条的旅行者 1 号历史航行体验。默认入口为 `scenes/voyager_main.tscn`，运行时加载官方星历与 Hipparcos 星表；旧 `main.tscn` 任务驾驶舱保留用于 `libphyz` 集成和回归测试，但不再作为游戏首页。

## 运行

在仓库根目录：

```powershell
.\scripts\run_game.ps1
```

或直接启动编辑器 / 项目：

```powershell
.\.tools\godot\Godot_v4.6.3-stable_win64_console.exe --path .\godot
```

## 输入

| 输入 | 作用 |
|---|---|
| `Space` | 播放 / 暂停 |
| 拖动底部时间条 | 选择并显示当前时间流速 |
| `+` / `-` | 逐档改变流速，并同步拖动条 |
| `A` | 自动接近减速 |
| `M` | 跟随视图 / 太阳系总览 |
| `C` | 任务可视化色彩 / 物理曝光 |
| `1`–`5` | 跳到主要历史节点 |
| `←` / `→` | 前后移动 1 天 |
| `Shift + ←/→` | 前后移动 30 天 |
| 鼠标拖动 / 滚轮 | 环绕 / 缩放 |
| `Home` | 回到 1977 起点 |
| `Esc` | 退出 |

窗口标题显示 UTC 日期、暂停 / 播放状态、当前倍率与色彩模式。画面底部只有一个半透明的时间控制条：可直接拖动九档流速，并持续显示当前倍率；播放按钮和 `Space`、拖动条和 `+ / -` 始终双向同步。默认“任务可视化”模式采用 Filmic 曝光、深海军蓝天空和可辨识的行星色域；按 `C` 切换到较低曝光的物理观察模式。两种模式只改变渲染，不改变星历或物理状态。

## 运行时结构

```text
voyager_main.tscn
└─ voyager_main.gd
   ├─ historical_ephemeris.gd       PHYZEPH2 双精度星历 / Hermite 插值
   ├─ Voyager 程序化部件模型
   ├─ 真实半径与距离驱动的跟随视图
   ├─ AU 尺度太阳系总览
   ├─ Hipparcos MultiMesh 星空
   └─ 程序化行星、大气、土星环、银河与色彩分级 shader
```

核心数据：

- `data/voyager_ephemeris.phyz`：JPL DE440s、Voyager 1/2 SPK 预处理结果
- `data/voyager_ephemeris.json`：来源、覆盖期和 SHA-256
- `data/hipparcos_bright.phyzstars`：V≤9 的 83,337 颗恒星
- `data/hipparcos_bright.json`：查询条件、ESA credit 与 SHA-256

跟随视图不会把 km 级坐标直接塞进 Godot 单精度世界。它先保持双精度相对状态，再按当前目标重建相机局部空间，同时保持天体的真实角直径。太阳系总览使用 AU 坐标，并单独设置可见图标尺寸。

## 测试

旅行者历史模式：

```powershell
.\.tools\godot\Godot_v4.6.3-stable_win64_console.exe --headless --path .\godot --script res://scripts/voyager_test_runner.gd
```

旧任务物理与可玩解：

```powershell
.\.tools\godot\Godot_v4.6.3-stable_win64_console.exe --headless --path .\godot --script res://scripts/test_runner.gd
.\.tools\godot\Godot_v4.6.3-stable_win64_console.exe --headless --path .\godot --script res://scripts/ui_smoke_runner.gd
```

全量构建脚本会依次运行 C++ 单元测试、原生实验室自检、GDExtension 构建、两套 Godot 回归和 Windows 导出验证。
