# 《余烬航线》Godot 游戏前端

这是 `libphyz` 的可玩 Godot 4.6.3 前端，通过 C++ GDExtension 调用权威物理状态。Godot 只负责显示、输入、剧情和任务反馈；N-body 状态、数值积分、机动节点与轨迹预测始终保留在双精度 C++ 层。

## 玩家闭环

选择任务 → 阅读简报 → 设置 `Δv` 与节点时刻 → 预览黄色轨迹 → 提交节点 → 推进时间 → 根据真实状态成功或失败。

五关覆盖霍曼式转移、交会对接、引力弹弓、小行星偏转和混沌系统生存。任务结果不依赖预制路径。导航建议提供经过自动化测试的起步解，目的是让第一次游玩的玩家迅速理解流程，而不是跳过操作。

界面包含：

- 三章五关的中文剧情、教程、结算与解锁进度
- 青色飞船、洋红目标、金色主天体的角色标签
- 黄色未来轨迹、目标连线和实时任务指标
- 轨道镜头、目标聚焦、暂停、倍率、单步与立即点火
- 版本化快照、战役进度和最佳分数本地存档

## 开发与运行

在仓库根目录执行：

```powershell
.\scripts\bootstrap_godot.ps1
.\scripts\build_all.ps1 -GodotTarget template_release -Jobs 4
.\scripts\run_game.ps1
```

发布版输出为 `dist/PhyzBox.exe`。

单独运行物理/任务集成测试：

```powershell
.\.tools\godot\Godot_v4.6.3-stable_win64_console.exe --headless --path .\godot --script res://scripts/test_runner.gd
```

运行真实界面的自动可玩性冒烟测试：

```powershell
.\.tools\godot\Godot_v4.6.3-stable_win64_console.exe --headless --path .\godot --script res://scripts/ui_smoke_runner.gd
```

测试会实际实例化驾驶舱、关闭剧情简报、使用导航建议、提交节点、推进第一关、检查成功结算和下一关解锁。生成的本机 DLL、Godot 导入缓存与工具链不会进入 Git。
