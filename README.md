<p align="center">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License: MIT">
  <img src="https://img.shields.io/badge/language-C++20-%23f34b7d.svg" alt="C++20">
  <img src="https://img.shields.io/badge/Godot-4.6-478cbf.svg" alt="Godot 4.6">
  <img src="https://img.shields.io/badge/platform-Windows-0078d7.svg" alt="Platform: Windows">
  <img src="https://img.shields.io/badge/physics-libphyz-brightgreen.svg" alt="libphyz physics">
</p>

# PhyzBox：余烬航线

一款由真实轨道力学驱动的单人任务游戏。你是太阳系边缘最后一名引力操作员，需要在有限推进剂下规划机动节点、预判黄色轨迹，并把飞船送过五个越来越危险的天体任务。

![余烬航线驾驶舱](docs/images/ember-route-cockpit.png)

游戏不是按预设路径播放动画：天体位置、引力弹弓、交会距离和任务结果全部由 C++20 `libphyz` 双精度 N-body 引擎实时计算。Godot 负责驾驶舱、剧情、镜头和反馈，旧版 Win32/OpenGL 程序作为科学实验室保留。

剧情简报背景由 Godot shader 实时绘制星空、行星边缘光与轨道线，不使用生成式贴图。专业遥测默认折叠；主画面只保留任务指标、轨迹与机动控制。飞船由推进舱、太阳翼、散热板、天线、RCS 和四组电推进器构成，不再用球体代替。

## 立即游玩

已经构建发布包时，直接运行：

```powershell
.\dist\PhyzBox.exe
```

首次从源码构建：

```powershell
.\scripts\bootstrap_godot.ps1
.\scripts\build_all.ps1 -GodotTarget template_release -Jobs 4
.\scripts\run_game.ps1
```

Windows 游戏生成在 `dist/PhyzBox.exe`。物理库使用者参阅 [libphyz/README.md](libphyz/README.md)，游戏开发说明参阅 [godot/README.md](godot/README.md)，完整实施与验收记录见 [IMPLEMENTATION_REPORT.md](IMPLEMENTATION_REPORT.md)，视觉设计决策见 [VISUAL_REDESIGN_REPORT.md](VISUAL_REDESIGN_REPORT.md)。

## 怎么玩

每一关都遵循同一个清晰的轨道工程闭环：

1. 阅读任务简报，认清青色的“你的飞船”、洋红色的“任务目标”和金色主天体。
2. 输入三轴 `Δv` 和机动发生时刻；不熟悉轨道力学时可点“导航建议”取得一组经过测试、但仍需亲自执行的参数。
3. 点“预测轨迹”，观察黄色未来轨迹是否接近目标。
4. 点“提交节点”，再点“运行”；求解器会精确推进至节点并施加脉冲。
5. 通过实时任务指标判断是否需要暂停、修正或重新开始。成功后会解锁下一关并保存最佳成绩。

常用操作：

- 鼠标拖动画面空白处：绕系统旋转镜头
- 鼠标滚轮：缩放
- `Space`：暂停 / 继续
- `F`：聚焦飞船
- `T`：聚焦任务目标
- `H`：查看整个系统
- 左侧按钮：预测、提交/取消节点、立即点火、单步推进、改变时间倍率、保存/载入

## 三章五关

![余烬航线章节简报](docs/images/ember-route-briefing.png)

| 章节 | 任务 | 核心玩法 |
|---|---|---|
| 第一章·火种离港 | 离港窗口、静默信标 | 转移轨道与精确交会 |
| 第二章·巨人的回声 | 借光而行、苍穹偏转 | 引力弹弓与小行星偏转 |
| 第三章·三阳尽头 | 最后的观测者 | 在混沌三体系统中生存 |

章节剧情、教程、失败复盘、解锁进度与最佳分数都会本地保存。五关均通过自动导航解实测，不存在必须碰运气的“假任务”。

## 项目架构

```text
libphyz（C++20 双精度权威状态）
├─ Godot GDExtension → 余烬航线：任务、剧情、驾驶舱与渲染
├─ Win32/OpenGL      → 科学实验室与自由沙盘
└─ tests/examples    → 算法回归、守恒量与确定性验证
```

> 🌌 从混沌三体实验室发展为可玩的轨道工程游戏，完整历史见 [CHANGELOG.md](CHANGELOG.md)。

## 科学实验室

> 🌌 *Three-body problem → Astrophysical sandbox — see [project history](CHANGELOG.md).*

---

一个纯 C++20 / Win32 / OpenGL 的 3D 天体物理沙盘模拟器。默认场景是非共面的混沌三体，三个天体的位置和轨迹全部由实时万有引力积分得到，不是预制动画。

物理计算使用天文单位制：距离为 AU，质量为太阳质量，时间为年，引力常数为 `G = 4π² AU³ / (M☉·yr²)`。它不需要联网下载依赖，当前工作空间里用 MinGW `g++` 就能直接构建。

## 构建

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

或者：

```bat
build.bat
```

手动编译命令：

```powershell
g++ -std=c++20 -O2 -Wall -Wextra -pedantic src\*.cpp -o bin\PhyzBox.exe -lopengl32 -lgdi32 -luser32 -static-libgcc -static-libstdc++
```

## 运行

```powershell
.\bin\PhyzBox.exe
```

无窗口物理自检：

```powershell
.\bin\PhyzBox.exe --self-test
.\bin\PhyzBox.exe --self-test-explorer
```

## 操作

- 鼠标左键拖动：旋转相机
- 鼠标滚轮：缩放
- `Space`：暂停 / 继续
- `R`：重置当前场景
- `0`：重新读取 `phyzbox.ini` 并切换到自定义初始条件
- `1`：混沌三体主场景
- `2`：经典 figure-eight 三体轨道
- `3`：倾斜平面的 3D 拉格朗日三角三体
- `4`：双星 + 第三天体的层级系统
- `5`：飞行器借巨行星引力弹弓加速的演示场景
- `6`：自洽行星系统，所有恒星、行星和飞船都由同一个 N-body 核心推进
- `+` / `-`：加快 / 减慢基准模拟速度
- 拖动画面左上角的 `Base speed` 滑条：连续调节基准模拟速度
- `A`：开启 / 关闭自动时间倍率
- `C`：显示 / 隐藏混沌敏感性影子系统
- `E`：编辑模式，暂停后可拖动天体；按住 `Shift` 拖动改变 z 高度
- `H`：显示 / 隐藏引力场箭头
- `M`：开启 / 关闭碰撞合并
- `P`：显示 / 隐藏无质量测试粒子
- `T`：显示 / 隐藏轨迹
- `X`：导出当前快照到 `exports/phyzbox_snapshot.csv`
- `G`：显示 / 隐藏参考轨道环
- `O`：开启 / 关闭自动环绕相机
- `F`：切换相机跟随目标
- `Esc`：退出

探索模式专用驾驶：

- `W` / `S`：前进 / 反推
- `A` / `D`：左 / 右平移
- `Q` / `E`：下 / 上平移
- `Shift`：加力推进
- `Ctrl`：逆速度方向的有限推力制动（不是空气阻尼）

## 实现概览

- 物理核心：`src/NBodySystem.*`
- 初始条件配置：`src/SimulationConfig.*`
- 向量与矩阵数学：`src/Math3D.hpp`
- OpenGL 渲染与 HUD：`src/Renderer.*`
- Win32 窗口与输入：`src/Application.*`
- 开发日志与疑难记录：`DEVELOPMENT_LOG.md`

积分器使用四阶 Yoshida 辛积分，并在近距离交会时自动细分步长；这比朴素欧拉或普通二阶 Verlet 更适合长时间天体轨道演示。引力计算带很小的软化项，避免点质量奇点导致数值爆炸。每一对有质量天体使用对称的成对力更新，因此牛顿引力和 Paczynski-Wiita 强场近似都保持线动量守恒；势能诊断与实际采用的力模型一致。混沌场景不会闭合成稳定轨道，微小初始差异会在数值积分中迅速放大，这就是三体问题最迷人的部分。

默认开启自动时间倍率：天体距离较远时稍微加快，近距离飞掠时自动慢下来，方便观察关键交会。`+` / `-` 调整的是基准倍率，HUD 会同时显示实际倍率和基准倍率。

HUD 会明确显示积分器、活动 N-body 数量、时间步、能量漂移、角动量漂移、线动量残差、最近距离、系统状态和事件日志。屏幕上的球体半径只是显示半径；碰撞、洛希极限、逃逸速度等计算使用独立的真实物理半径。

物理与可视化增强：

- 碰撞合并：距离小于真实碰撞半径时按动量守恒合并天体。
- 无质量测试粒子：显示空间中受引力牵引的流线，但不反向影响恒星。
- 混沌影子系统：以 `1e-6 AU` 的初始扰动同时积分，用紫色幽影显示敏感性。
- 引力场箭头：`H` 打开后在参考面上显示局部引力方向。
- 编辑模式：`E` 进入后可直接拖动天体重新设置初始条件。
- 天体自转：每个球体都有自转轴、周期和实时推进的自转角，表面经纬线会随时间转动。
- 飞行器/引力弹弓：`spacecraft` 类型会作为低质量真实受力体积分，靠运动行星的引力飞掠自然改变速度，HUD 会显示飞行器速度、速度增益和最近飞掠距离。
- 自洽行星系统：`6` 号场景用固定 seed 生成一颗主恒星、32 个不同质量/偏心率/倾角的天体和一艘低质量飞船。初始状态来自轨道力学，但之后不再回写解析轨道；全部天体参加同一套成对引力计算，扰动、进动、散射和碰撞都由状态自然演化。飞船操控只增加有限外力，没有速度上限、真空阻尼、视觉球体反弹或位置吸附。

飞船靠近行星时，状态和事件使用物理接触距离及 Hill 球，而不是为了渲染而放大的球体尺寸。当前模型仍未包含大气、地形、刚体姿态、燃料和完整广义相对论；这些不会被伪装成预设好的游戏规则。

### 引擎设计参考

- [REBOUND](https://rebound.hanno-rein.de/)：N-body 状态由积分器推进，并针对长周期系统、近距离交会选择不同积分策略。
- [REBOUNDx](https://reboundx.readthedocs.io/en/latest/)：把广义相对论、潮汐等额外物理作为可组合效应加入，而不是写成场景动画。
- [Orekit numerical propagation](https://www.orekit.org/static/apidocs/org/orekit/propagation/numerical/NumericalPropagator.html)：区分状态、数值积分和可组合力模型。
- [NASA GMAT propagator](https://documentation.help/GMAT/Propagator.html)：由积分器与 ForceModel 共同传播轨道状态。

## 自定义初始条件

复制 `phyzbox.ini.example` 为 `phyzbox.ini`，即可设置初始天体数量和位置。没有 `phyzbox.ini` 时会使用默认三体场景；配置中没写的字段会自动补默认值。

常用字段：

```ini
body_count = 4

[body0]
position = -1.30, -0.20, 0.35
velocity = 0.60, 2.40, -0.70
mass = 1.4

[body1]
position = 1.15, -0.75, -0.25
```

支持字段包括 `name`、`type`、`mass`、`radius`、`physical_radius`、`density`、`temperature`、`luminosity`、`spin_axis`、`rotation_period`、`rotation_angle`、`color`、`position`、`velocity`。单位仍然是 AU、太阳质量、年；速度单位是 AU/year。`radius` 是屏幕可视半径，`physical_radius` 是碰撞合并和洛希极限判定用的真实半径；`rotation_period` 单位是年，`rotation_angle` 单位是角度。

`presets/` 里有几个示例场景，可以复制为 `phyzbox.ini` 后按 `0` 重新加载：

- `presets/close-encounter.ini`
- `presets/four-body-chaos.ini`
- `presets/black-hole-tde.ini`
- `presets/compact-remnants.ini`
- `presets/gravity-assist.ini`
- `presets/merger-lab.ini`

## 天体类型和强场效果

配置中的 `type` 支持：

- `star`
- `planet`
- `black_hole`
- `neutron_star`
- `white_dwarf`
- `minor_body`
- `spacecraft`

不同类型会自动估算真实半径、密度、温度、亮度和默认颜色。黑洞会使用史瓦西半径 `Rs = 2GM/c²`、事件视界吞噬、`ISCO = 3Rs` 参考内缘和吸积盘视觉；近黑洞引力使用 Paczynski-Wiita 伪牛顿势，能表现强场轨道进动和捕获趋势。它不是完整数值相对论求解器；真正完整 GR 需要求解动态时空度规和爱因斯坦场方程。这里实现的是适合实时交互的强场近似层。

洛希极限会根据天体密度/质量比估算。行星、小天体或恒星进入强潮汐区时会触发 `tidal disruption` 事件，并被撕成测试粒子流；如果主天体是黑洞，碎片会增强吸积盘亮度。

---

## Contributing

Contributions are welcome! See [CONTRIBUTING.md](CONTRIBUTING.md) for details.

- 🐛 **Report bugs** via [GitHub Issues](../../issues)
- 💡 **Suggest features** via [Feature Requests](../../issues/new?template=feature_request.md)
- 📖 **Improve docs** — typos, English translations, new preset scenarios
- 🛠 **Submit PRs** — see the [PR checklist](CONTRIBUTING.md#pull-request-checklist)

## License

[MIT](LICENSE) © 2026 hydarealman

---

*PhyzBox — Where gravity is the only dependency.*
