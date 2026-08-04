# PhyzBox 专业天体动力学算法库方案（已批准并完成首版）

日期：2026-08-04
状态：**已批准；L0-L6 首版已实施并通过自动验收**
暂定库名：`libphyz`
目标：把现有天体物理算法整理成独立、可测试、可复用、可被原生程序和 Godot 同时调用的 C++20 库。

## 1. 定位

`libphyz` 不负责：

- 窗口和输入。
- OpenGL/Godot 渲染。
- 游戏任务和 UI。
- 资源路径和平台生命周期。

`libphyz` 只负责：

- 天体状态与单位。
- 力模型。
- 数值积分。
- 事件检测与物理响应。
- 轨道分析和守恒诊断。
- 轨迹预测。
- 模拟快照与可复现性。

推荐关系：

```text
                  ┌─────────────────┐
                  │     libphyz     │
                  │ pure C++20 core │
                  └────────┬────────┘
                           │
          ┌────────────────┼────────────────┐
          ▼                ▼                ▼
  Native laboratory   Godot GDExtension   CLI/tests
  Win32/OpenGL        game frontend       validation
```

## 2. 设计原则

1. **单一物理真相**：位置、速度、质量和时间只由核心库持有权威状态。
2. **组合优于场景分支**：场景只创建初始条件和选择模型，不编写专用运动规则。
3. **积分器与力模型解耦**：积分器只推进状态，力模型只提供导数/加速度。
4. **事件检测与响应解耦**：检测“发生了什么”与决定“如何处理”分开。
5. **单位明确**：公共 API 不允许没有说明的裸单位。
6. **可验证**：每个算法必须有解析解、收敛性或交叉软件基准。
7. **可复现**：固定配置、初始状态、随机种子和版本必须得到确定结果。
8. **前端无关**：核心代码不包含 Windows、OpenGL 或 Godot 头文件。
9. **性能透明**：公开力评估次数、接受/拒绝步数和事件次数。
10. **渐进迁移**：先封装现有正确算法，再增加新算法，不推倒重写。

## 3. 建议目录结构

```text
libphyz/
  CMakeLists.txt
  include/phyz/
    version.hpp
    math/
      vec3.hpp
    core/
      units.hpp
      time.hpp
      body.hpp
      state.hpp
      result.hpp
    dynamics/
      force_model.hpp
      force_pipeline.hpp
      newtonian_gravity.hpp
      paczynski_wiita.hpp
      thrust.hpp
    integration/
      integrator.hpp
      leapfrog2.hpp
      yoshida4.hpp
      dopri54.hpp                 后续
    events/
      event.hpp
      detector.hpp
      collision_detector.hpp
      roche_detector.hpp
      close_approach_detector.hpp
      event_resolver.hpp
    analysis/
      invariants.hpp
      orbital_elements.hpp
      reference_frame.hpp
      encounter.hpp
    simulation/
      simulation.hpp
      simulation_config.hpp
      step_report.hpp
      trajectory_predictor.hpp
    io/
      snapshot.hpp
      snapshot_format.hpp
      scenario_loader.hpp
    c_api/
      phyz_c.h                    后续稳定绑定层

  src/
    ...

  tests/
    unit/
    convergence/
    regression/
    reference/

  benchmarks/
    direct_nbody.cpp
    integrator_bench.cpp

  examples/
    two_body.cpp
    figure_eight.cpp
    gravity_assist.cpp
    custom_force.cpp
```

## 4. 核心数据模型

### `BodyId`

使用稳定 ID，不把 `std::vector` 下标暴露为长期身份。

原因：碰撞合并或删除天体后，下标会变化，UI、事件和存档不能依赖旧下标。

```cpp
struct BodyId {
    std::uint64_t value{};
};
```

### `BodyState`

只保存推进所需的动力学状态：

```cpp
struct BodyState {
    BodyId id;
    double gravitational_mass;
    double inertial_mass;
    double physical_radius;
    Vec3d position;
    Vec3d velocity;
};
```

说明：

- 将惯性质量和引力质量概念分开，默认相等。
- 无质量测试粒子可以设置 `gravitational_mass = 0`，但保留惯性响应。
- 名称、颜色和显示半径属于元数据，不应混入积分器热点结构。

### `BodyMetadata`

```cpp
struct BodyMetadata {
    BodyId id;
    std::string name;
    BodyKind kind;
    VisualProperties visual;
    PhysicalProperties physical;
};
```

### SoA 与 AoS

第一版可以保留易读的 AoS；公共 API 不承诺内部布局。性能阶段可在实现内部改为 Structure of Arrays，而不破坏调用方。

## 5. 单位系统

当前内部单位 `AU / solar mass / year` 对天体动力学很方便，应继续作为默认规范单位，但必须显式记录。

建议：

```cpp
struct UnitSystem {
    double length_in_meters;
    double mass_in_kilograms;
    double time_in_seconds;
    double gravitational_constant;

    static UnitSystem astronomical();
    static UnitSystem si();
};
```

规则：

- 每个 `Simulation` 创建时固定单位系统。
- 快照必须保存单位系统。
- 单次模拟中不允许偷偷改变单位。
- 公共文档对每个参数注明量纲。
- 第一版不引入复杂的模板量纲库，避免 API 过度膨胀。

## 6. 力模型接口

推荐最小接口：

```cpp
class ForceModel {
public:
    virtual ~ForceModel() = default;

    virtual std::string_view name() const noexcept = 0;
    virtual ForceTraits traits() const noexcept = 0;

    virtual void accumulate(
        const SystemView& state,
        double time,
        AccelerationBuffer& out) const = 0;

    virtual std::optional<double> potential_energy(
        const SystemView& state,
        double time) const = 0;
};
```

`ForceTraits` 应声明：

- 是否保守。
- 是否依赖速度。
- 是否依赖时间。
- 是否保持平移不变性。
- 是否支持反向积分。
- 是否提供势能。

这样诊断层可以知道：有推力时总机械能变化不是积分误差；速度依赖力不能随意套用只适合可分离 Hamiltonian 的辛积分器。

### 首批模型

1. `NewtonianGravity`
2. `PlummerSoftenedGravity`
3. `PaczynskiWiitaGravity`
4. `FiniteThrust`

后续模型：

- `PostNewtonian1PN`
- `J2Gravity`
- `RadiationPressure`
- `ConstantTimeLagTides`
- `MassLoss`

## 7. 积分器接口

```cpp
class Integrator {
public:
    virtual ~Integrator() = default;

    virtual std::string_view name() const noexcept = 0;
    virtual IntegratorTraits traits() const noexcept = 0;

    virtual StepResult step(
        MutableSystemView state,
        const ForcePipeline& forces,
        double& time,
        double requested_dt) = 0;
};
```

`StepResult` 返回：

- 实际推进时间。
- 力评估次数。
- 接受/拒绝步数。
- 最小内部步长。
- 错误估计。
- 警告和失败状态。

### 首批积分器

1. `Leapfrog2Fixed`
2. `Yoshida4Fixed`

### 第二批

3. `DormandPrince54Adaptive`

### 暂缓

- 完整 IAS15。
- WHFast/Wisdom-Holman 映射。

原因：先把接口、基准和误差控制建立好，再引入复杂算法。

## 8. 事件系统

事件分三层：

```text
Detector 发现过零/阈值
    ↓
Event 描述时间、参与体和数值
    ↓
Resolver 决定停止、记录、合并或改变状态
```

### `Event`

```cpp
struct Event {
    EventType type;
    double time;
    BodyId primary;
    BodyId secondary;
    double value;
    double threshold;
};
```

### 首批检测器

- 碰撞接触。
- 最近接近。
- 洛希极限穿越。
- 事件视界穿越。
- Hill 球进入/离开。
- 逃逸边界。

重要改进：真正专业的事件系统需要在一个时间步内部定位事件时刻，而不是只在步末检查。第一版可以使用二分或插值定位。

## 9. 分析算法模块

### 守恒量

- 总动能。
- 各力模型提供的势能。
- 总线动量。
- 总角动量矢量，而不仅是模长。
- 质心位置与速度。
- 外力冲量和外力做功。

### 轨道根数

- 笛卡尔状态到开普勒根数。
- 椭圆、抛物线和双曲线状态。
- 圆轨道/赤道轨道退化处理。
- 偏心率向量、节点向量、比角动量。
- 指定参考主天体或质心。

### 交会分析

- 相对位置/速度。
- 径向和切向速度。
- 线性预计最近接近。
- 数值轨迹上的最近接近。
- 双曲超额速度和转向角。

### 轨迹预测

- 从当前状态复制一个只读分支模拟。
- 应用计划中的脉冲/有限推力。
- 输出采样轨迹和预测事件。
- 预测过程不能修改主模拟。

## 10. 公共 API 示例

目标体验：

```cpp
#include <phyz/simulation/simulation.hpp>
#include <phyz/dynamics/newtonian_gravity.hpp>
#include <phyz/integration/yoshida4.hpp>

int main() {
    phyz::Simulation sim(phyz::UnitSystem::astronomical());

    sim.add_body({
        .name = "Sun",
        .mass = 1.0,
        .physical_radius = 0.00465047,
        .position = {0.0, 0.0, 0.0},
        .velocity = {0.0, 0.0, 0.0},
    });

    sim.add_body({
        .name = "Earth",
        .mass = 3.003e-6,
        .physical_radius = 4.2635e-5,
        .position = {1.0, 0.0, 0.0},
        .velocity = {0.0, 2.0 * phyz::pi, 0.0},
    });

    sim.forces().add<phyz::NewtonianGravity>();
    sim.set_integrator<phyz::Yoshida4Fixed>();
    sim.set_fixed_step(1.0e-4);

    const phyz::StepReport report = sim.advance_to(1.0);
    const phyz::InvariantReport invariants = sim.invariants();
}
```

原则：简单案例要简单，高级能力通过可选配置增加。

## 11. 错误处理

库边界应区分三类情况：

1. 编程错误：无效 ID、空视图等，可使用断言。
2. 用户/配置错误：负质量、无效单位、非法步长，返回结构化 `Result<T>`。
3. 数值失败：NaN、步长下溢、事件定位失败，返回 `StepStatus`，不能静默继续。

不建议让异常跨越 GDExtension/C API 边界。

第一版可实现轻量 `Result<T, Error>`，避免要求 C++23 `std::expected`。

## 12. 版本与兼容性

### 语义化版本

- `0.x`：API 仍可调整。
- `1.0`：公共 C++ API 稳定。

### 快照版本

快照头至少包含：

- magic number。
- 格式版本。
- libphyz 版本。
- 单位系统。
- 浮点格式和字节序。
- 启用的力模型及参数。
- 积分器及参数。
- 当前时间和随机种子。

### ABI

- 第一阶段只承诺源代码 API，不承诺跨编译器 C++ ABI。
- Godot 适配器与核心库使用同一工具链构建。
- 未来如需 Python/Rust/C#/插件生态，再提供稳定 C ABI。

## 13. 构建与发布

推荐使用 CMake 作为核心库的正式构建系统：

- 生成静态库和可选共享库。
- 支持 `install()` 和 `find_package(libphyz)`。
- 生成 `libphyzConfig.cmake`。
- 测试通过 CTest 运行。
- Godot 适配器可以用 SCons 调用已构建核心，或单独用 CMake 构建。

现有 `build.ps1` 在迁移期保留，作为兼容入口。

当前机器尚未安装 CMake/SCons，因此实施前需要单独批准工具链安装。

## 14. 验证矩阵

### 解析基准

| 测试 | 检查内容 |
|---|---|
| 单个自由粒子 | 匀速直线运动 |
| 两体圆轨道 | 周期、半径、能量 |
| 两体椭圆轨道 | 近远心点、周期、轨道根数 |
| 双曲散射 | 转向角和超额速度 |
| 质心运动 | 线动量守恒 |

### 已知数值解

- Figure-eight 三体周期轨道。
- 拉格朗日等边三体。
- 层级三体长期稳定性。

### 收敛性

- Leapfrog 误差随步长按二阶收敛。
- Yoshida 误差随步长按四阶收敛。
- 自适应积分器误差随容限收敛。

### 事件

- 一个步长内穿过碰撞面也能定位事件。
- 合并严格保持质量和线动量。
- 反向积分时事件方向正确。

### 交叉验证

- 选择若干纯牛顿场景，与 REBOUND 输出比较。
- 明确比较状态误差、能量误差和事件时间，而不是只比较画面。

## 15. 性能与线程安全

第一版目标：

- 核心模拟实例之间互不共享可变全局状态。
- 一个 `Simulation` 由一个线程写入。
- 多个预测分支可以并行运行。
- 只读快照可跨线程传递。
- 内部临时缓冲复用，避免每步分配。

性能指标：

- 每秒力对评估数。
- 每个积分步耗时。
- 每个天体内存占用。
- 轨迹预测耗时。
- 快照大小和恢复耗时。

优化顺序：

1. 消除热点分配。
2. 数据布局与缓存。
3. 多线程 direct N-body。
4. Barnes-Hut。
5. SIMD/GPU，只有基准证明需要时再做。

## 16. 渐进迁移步骤

### L0：冻结行为

- 保存当前所有无窗口测试输出。
- 增加两体和双曲散射基准。
- 记录现有 public 行为和误差阈值。

### L1：建立库骨架

- 创建 `libphyz` 目录和构建目标。
- 迁移 `Vec3`、Body ID、状态和单位。
- 原程序仍使用旧 `NBodySystem`。

### L2：迁移力计算

- 迁移牛顿/软化/伪牛顿力。
- 新旧实现对同一状态逐项比较加速度。

### L3：迁移积分器

- 迁移 Leapfrog 和 Yoshida。
- 运行阶数收敛和回归测试。

### L4：迁移诊断与事件

- 迁移守恒量。
- 引入稳定 `BodyId`。
- 迁移碰撞、洛希和近交会事件。

### L5：替换应用层依赖

- 原生 App 改为调用 `libphyz`。
- 删除 `NBodySystem` 中已经迁移的重复实现。
- 所有自检继续通过。

### L6：接入 Godot

- GDExtension 只绑定 `libphyz` 公共 API。
- 不把内部容器和实现细节暴露给 Godot。

## 17. 第一版 `libphyz 0.1` 建议范围

包含：

- 双精度 `Vec3d`。
- 稳定 `BodyId`。
- 天文单位与 SI 单位描述。
- Newtonian/Plummer/Paczynski-Wiita 力。
- Leapfrog2/Yoshida4 固定步长积分器。
- 能量、线动量、角动量和质心诊断。
- 碰撞、近交会和事件视界检测。
- 两体轨道根数。
- 版本化文本快照。
- CMake 安装目标。
- 示例和验证测试。

不包含：

- Godot 类型。
- Barnes-Hut/GPU。
- SPH。
- 完整 GR。
- 轨道确定和卡尔曼滤波。
- 稳定 C ABI。
- 网络与分布式计算。

## 18. 审批项

- [ ] 同意将 `libphyz` 作为 Godot 之前的第一优先级
- [ ] 同意纯 C++20、前端无关的核心定位
- [ ] 同意 CMake 作为正式构建系统，同时保留原脚本入口
- [ ] 同意先承诺源代码 API，不承诺 C++ 二进制 ABI
- [ ] 同意 `0.1` 范围
- [ ] 同意按 L0→L6 渐进迁移，不重写全部代码

批准后第一项实际工作应是 **L0：补齐基准测试和冻结现有行为**，而不是立即移动源文件。
