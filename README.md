# Quasi-regular Patterns with Wang Tiles

一个面向论文复现与生成艺术创作的交互式图形项目。项目以 Wang Tiles、
domain-invariant Coons warp 和环面连续生成器为核心，在可验证无缝的前提下生成
非周期、可调且适合高分辨率输出的图案。

## 当前状态

CPU 数学核心、CPU 基准渲染、GPU 实时渲染与第一版参数编辑器已经完成。当前仓库包含：

- 原始论文：[Quasi_regular_Patterns_with_Wang_Tiles_.pdf](./Quasi_regular_Patterns_with_Wang_Tiles_.pdf)
- [实现计划](./docs/implementation-plan.md)
- [数学与坐标规范](./docs/math-spec.md)
- [验证方案](./docs/validation.md)
- [CPU 基准与复现记录](./docs/cpu-baselines.md)
- [GPU 基准与一致性记录](./docs/gpu-baselines.md)
- [交互编辑器与提交语义](./docs/interactive-editor.md)
- 已覆盖 625 种默认边颜色组合的 CPU 数学测试
- 周期 Fourier/QRP、周期梯度噪声、连续调色板和 CPU 双精度参考渲染器
- 单瓦片、2 x 2 与三套 10 x 10 固定种子基准，以及独立接缝误差热图
- OpenGL 4.3 实时主视图、相机平移缩放和 Jacobian/Newton/瓦片边界调试视图
- 可编辑 Wang 网格、边函数、生成器和色带的 Dear ImGui 面板
- 经验证后才替换权威场景的 draft/committed 参数事务与 revision

代码实现以 `docs/math-spec.md` 为权威定义。论文是研究来源；论文中尚不充分或不满足
周期条件的论证，不直接作为代码契约。

## 目标形态

- C++20、CMake、OpenGL/GLSL 桌面应用。
- CPU 双精度参考实现与 GPU 实时渲染实现相互校验。
- 可交互编辑 Wang 边函数、铺砌种子、生成器和调色板。
- 提供 Jacobian、Newton 残差和接缝误差调试视图。
- 支持高分辨率无缝图案导出与可复现参数。

## 计划中的首个里程碑

实时显示 10 x 10 Wang 铺砌，支持周期噪声和 torus-safe QRP 两类生成器，包含
接缝误差视图以及至少三套经过视觉打磨的预设。

## 构建与测试

Windows + Visual Studio 2022：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

生成可复现的 CPU 基准图（PPM 文件和误差热图写入被 Git 忽略的输出目录）：

```powershell
.\build\Release\qrp_cpu_reference.exe output\cpu
```

当前构建产生 CPU 参考工具、测试程序和带参数面板的 GPU 实时桌面应用。

运行实时程序：

```powershell
.\build\Release\qrp_realtime.exe
```

- 鼠标左键拖动：平移。
- 滚轮：以光标为中心缩放。
- `1`–`5`：切换固定预设。
- `D`：切换图案、Jacobian、Newton 残差和瓦片边界视图。
- `R`：重置相机；`Esc`：退出。
- 面板预设只载入草稿；`Apply validated draft` 校验并提交，`Discard` 放弃草稿。

运行隐藏窗口的 GPU/CPU 分阶段一致性检查：

```powershell
cmake --build build --config Release --target qrp_gpu_validate
```

实时目标通过 CMake 固定并获取 GLFW 3.4 与 Dear ImGui 1.92.9；仓库内包含由 glad
2.0.8 生成的纯 OpenGL 4.3 core loader，因此构建不依赖额外 Python 包。若只需要 CPU
目标，可在配置时传入 `-DQRP_BUILD_REALTIME=OFF`。
