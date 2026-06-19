# PhyzBox

一个纯 C++ / Win32 / OpenGL 的 3D 三体天体运动演示项目。默认场景是非共面的混沌三体，三个天体的位置和轨迹全部由实时万有引力积分得到，不是预制动画。

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
- `6`：程序化宇宙探索模式，生成数百个星球并驾驶玩家飞船巡航
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
- `Ctrl`：阻尼刹车

## 实现概览

- 物理核心：`src/NBodySystem.*`
- 初始条件配置：`src/SimulationConfig.*`
- 向量与矩阵数学：`src/Math3D.hpp`
- OpenGL 渲染与 HUD：`src/Renderer.*`
- Win32 窗口与输入：`src/Application.*`
- 开发日志与疑难记录：`DEVELOPMENT_LOG.md`

积分器使用四阶 Yoshida 辛积分，并在近距离交会时自动细分步长；这比朴素欧拉或普通二阶 Verlet 更适合长时间天体轨道演示。引力计算带很小的有限恒星半径级软化项，避免点质量奇点导致数值爆炸。混沌场景不会闭合成稳定轨道，微小初始差异会在数值积分中迅速放大，这就是三体问题最迷人的部分。

默认开启自动时间倍率：天体距离较远时稍微加快，近距离飞掠时自动慢下来，方便观察关键交会。`+` / `-` 调整的是基准倍率，HUD 会同时显示实际倍率和基准倍率。

HUD 中的能量漂移、角动量漂移、最近距离、系统状态、混沌发散度、合并次数和事件日志都来自实时计算，可用于判断模拟是否仍然守恒得足够好。屏幕上的球体半径为了可视化被放大了，不代表真实恒星半径。

物理与可视化增强：

- 碰撞合并：距离小于真实碰撞半径时按动量守恒合并天体。
- 无质量测试粒子：显示空间中受引力牵引的流线，但不反向影响恒星。
- 混沌影子系统：以 `1e-6 AU` 的初始扰动同时积分，用紫色幽影显示敏感性。
- 引力场箭头：`H` 打开后在参考面上显示局部引力方向。
- 编辑模式：`E` 进入后可直接拖动天体重新设置初始条件。
- 天体自转：每个球体都有自转轴、周期和实时推进的自转角，表面经纬线会随时间转动。
- 飞行器/引力弹弓：`spacecraft` 类型会作为低质量真实受力体积分，靠运动行星的引力飞掠自然改变速度，HUD 会显示飞行器速度、速度增益和最近飞掠距离。
- 程序化宇宙探索：`6` 号场景会用固定 seed 生成一颗主恒星、约 220 个星球和玩家飞船。星球按解析轨道公转；飞船每帧的加速度由自主推进和主恒星/全部星球的引力矢量和共同决定，即 `a = a_engine + Σ GM_i r_i / |r_i|^3`。HUD 会显示最近星球、综合引力、逃逸速度和飞船速度。

探索模式目前是太空飞行垂直切片：可以飞向不同星球、进入它们的重力井、低空擦掠并感受不同质量/距离带来的加速度变化。真正的地表地形分块、降落后行走、资源和生态系统还没有接入。

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
