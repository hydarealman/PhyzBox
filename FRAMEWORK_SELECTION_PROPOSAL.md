# PhyzBox 游戏化框架选型方案（已批准并实施）

日期：2026-08-04
状态：**已批准；G0-G4 已执行并通过自动验收**
目标：为“轨道工程解谜 + 真实多体物理 + 自由实验沙盘”选择游戏开发框架。

## 1. 结论

推荐方案：

> **Godot 4.6.x 作为游戏前端，现有 PhyzBox 物理核心保留为独立纯 C++20 库，通过 godot-cpp / GDExtension 接入。**

不建议：

- 不把 N-body 核心改写成 GDScript。
- 不让 Godot 内置 `RigidBody3D` 接管天体轨道。
- 不立即删除当前 Win32/OpenGL 程序。
- 不在第一阶段启用 Godot 全局双精度自定义构建。

Godot 当前稳定维护线为 4.6.x；项目实施时应锁定一个明确补丁版本，并使用匹配版本的 `godot-cpp`。

## 2. 候选方案比较

| 方案 | 游戏/UI 开发 | 保留现有 C++ | 跨平台 | 迁移成本 | 结论 |
|---|---:|---:|---:|---:|---|
| 继续 Win32 + 固定管线 OpenGL | 弱 | 最好 | 差 | 短期低、长期高 | 不推荐作为游戏主线 |
| SDL3 + Dear ImGui/ImPlot | 中 | 最好 | 好 | 中 | 更适合科学工具，不如 Godot 适合完整游戏 |
| Godot + GDExtension | 强 | 好 | 好 | 中 | **推荐** |
| 全部改写为 Godot/GDScript | 强 | 差 | 好 | 高 | 不推荐，损失核心和验证积累 |
| Unity/Unreal | 强 | 可行 | 好 | 高 | 对当前小型 C++ 项目过重 |

## 3. 为什么选择 Godot

Godot 可以直接补齐当前项目最薄弱的游戏层：

- 菜单、任务选择、暂停面板和结算 UI。
- 3D 场景、相机、粒子、后处理和字体。
- 输入映射、手柄支持。
- 场景资源、任务配置、存档。
- 动画、音频和多平台导出。
- 编辑器内快速制作关卡与 UI。

`godot-cpp` 是 Godot 官方维护的 C++ GDExtension 绑定，不要求重新编译 Godot 引擎，适合包装现有第三方/独立 C++ 库。[官方说明](https://docs.godotengine.org/en/4.5/tutorials/scripting/cpp/about_godot_cpp.html)

## 4. 推荐架构

```text
PhyzBox/
  physics_core/                 纯 C++20，无 Godot 依赖
    include/phyz/...
    src/...
    tests/...

  apps/
    native_lab/                 现有 Win32/OpenGL 验证程序，迁移期保留

  godot/
    project.godot
    scenes/
    scripts/                    GDScript：任务、UI、游戏流程
    resources/                  关卡、任务、素材
    addons/phyzbox_native/
      src/                      很薄的 GDExtension 适配层
      bin/

  third_party/
    godot-cpp/                  锁定到对应 Godot 版本
```

### 职责划分

#### C++ 物理核心负责

- 天体状态和单位。
- N-body 力计算。
- 积分器。
- 碰撞和物理事件。
- 轨道根数与诊断。
- 预测轨迹。
- 快照、恢复和确定性测试。

#### GDExtension 适配层负责

- 创建/销毁模拟实例。
- 把 Godot 输入转换成机动命令。
- 把 C++ 状态快照转换成 Godot 可读取的数据。
- 发出碰撞、任务事件等信号。
- 不保存游戏规则，不承担渲染。

#### Godot/GDScript 负责

- 任务目标和关卡流程。
- 菜单、HUD、轨道规划界面。
- 3D 节点和视觉表现。
- 音效、动画和提示。
- 存档入口与任务评分。

## 5. 精度方案

天体模拟不能直接依赖 Godot 默认 `Vector3` 作为权威状态。

Godot 默认三维向量通常是单精度；官方的大世界坐标方案需要双精度引擎构建。[官方大世界坐标文档](https://docs.godotengine.org/en/4.7/tutorials/physics/large_world_coordinates.html)

本项目建议：

1. C++ 核心始终使用 `double` 和 AU/太阳质量/年单位制。
2. Godot 只接收“相对当前观察中心”的局部渲染坐标。
3. 相机跟随目标变化时更新 floating origin，不修改物理状态。
4. 轨迹点在 C++ 侧减去渲染原点后再转成 Godot `Vector3`。
5. 第一阶段使用标准 Godot 构建，避免维护自定义双精度引擎。

这能同时保留轨道精度和常规 Godot 插件/导出兼容性。

## 6. 运行线程方案

推荐将渲染与模拟解耦：

```text
Godot 主线程
  输入 / UI / 渲染
        │ 命令队列
        ▼
C++ 模拟线程
  固定步长推进 / 预测 / 事件
        │ 只读状态快照
        ▼
Godot 主线程插值显示
```

约束：

- Godot 场景节点只在主线程修改。
- C++ 核心不保存 Godot 对象指针。
- 状态通过不可变快照或双缓冲传递。
- 暂停和单步由命令控制，不直接抢占模拟对象。
- 第一版也可以先单线程验证接口，性能测试后再启用模拟线程。

## 7. 为什么不直接选 SDL3

SDL3 已正式发布，提供跨平台窗口、输入、音频和现代 GPU API，技术上完全可行。[SDL3 官方说明](https://wiki.libsdl.org/SDL3/FrontPage)、[SDL GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU)

但 SDL3 仍是底层库：

- UI、布局、关卡编辑、动画和资源系统仍需自行组合。
- Dear ImGui 很适合调试面板和科学工具，但制作任务界面、教程和游戏菜单会增加大量自研工作。
- 项目目前最缺的是游戏产品层，而不是另一个窗口/渲染抽象层。

如果最终定位回到“专业模拟工具”，SDL3 + ImGui/ImPlot 会比 Godot 更合适；如果确定做游戏，Godot 更合适。

## 8. 迁移原则

采用“绞杀式迁移”，不推倒重写：

1. 先把物理核心从 Win32/Application/Renderer 依赖中完全分离。
2. 保持现有 `--self-test` 可运行。
3. 建立最小 Godot 技术样例：只显示两个由 C++ 核心推进的天体。
4. 对比同一初始条件在 native_lab 和 Godot 中的状态哈希。
5. 验证通过后，再迁移相机、轨迹和 HUD。
6. 最后才开发任务系统和机动节点。
7. 在 Godot 前端达到功能替代前，不删除原生前端。

## 9. 分阶段执行建议

### G0：工具链验证

- 安装/锁定 Godot 4.6.x。
- 准备匹配版本 `godot-cpp`。
- 准备 SCons 或 CMake 构建路径。
- 构建官方最小 GDExtension 样例。

验收：Godot 能加载一个本地 C++ 扩展并调用方法。

### G1：物理核心拆分

- 建立 `physics_core` 静态库。
- `--self-test` 链接新库并全部通过。
- 原生程序继续正常运行。

验收：物理核心头文件不包含 Windows/OpenGL/Godot 头文件。

### G2：Godot 最小垂直样例

- C++ 创建一个两体系统。
- Godot 每帧读取只读快照。
- 显示天体、相机和轨迹。
- 支持暂停、单步和时间倍率。

验收：与原生自检在相同时间点的状态误差低于设定阈值。

### G3：轨道游戏 MVP

- 任务目标。
- Δv 与燃料。
- 机动节点。
- 预测轨迹。
- 成功/失败和评分。
- 五个手工验证关卡。

验收：完整完成“规划—执行—时间推进—结算”循环。

### G4：替代与清理

- Godot 前端覆盖原生前端的必要功能。
- 决定原生前端保留为开发实验室，或进入维护模式。
- 整理构建、发布和许可证说明。

## 10. 当前环境情况

当前机器检测结果：

```text
godot   未安装
scons   未安装
cmake   未安装
ninja   未安装
g++     D:\mingw-w64\mingw64\bin\g++.exe
```

因此目前不能直接构建 GDExtension。安装 Godot 和构建工具属于下一步外部环境变更，应在批准本方案后进行。

## 11. 主要风险

1. GDExtension 与 `godot-cpp` 需要匹配目标 Godot 版本，应锁定版本，避免频繁跟随开发版。
2. Godot 物理、坐标和 C++ 核心物理不能出现双权威状态。
3. 不能每帧为大量轨迹点创建大量 Variant/Array；后续需批量传输或使用渲染缓冲。
4. GDScript 只做游戏逻辑，热点计算不能逐步迁入脚本。
5. Godot 导出包必须带对应平台的 GDExtension 动态库。
6. 迁移期维护两个前端会增加短期工作量，所以要保持适配层极薄。

## 12. 请求审批

建议审批内容：

- [ ] 采用 Godot 4.6.x + GDExtension
- [ ] 保留纯 C++ 双精度物理核心
- [ ] 采用局部渲染坐标，不立即自编译双精度 Godot
- [ ] 采用渐进迁移，暂不删除原生前端
- [ ] 先执行 G0 工具链验证，不开始游戏功能开发

只有以上方向确认后，才进入安装工具链与最小技术样例阶段。
