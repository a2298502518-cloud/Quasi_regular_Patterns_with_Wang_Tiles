# Quasi-regular Patterns with Wang Tiles

一个面向论文复现与生成艺术创作的交互式图形项目。项目以 Wang Tiles、
domain-invariant Coons warp 和环面连续生成器为核心，在可验证无缝的前提下生成
非周期、可调且适合高分辨率输出的图案。

## 当前状态

项目处于设计基线阶段，尚未开始算法与渲染代码实现。当前仓库包含：

- 原始论文：[Quasi_regular_Patterns_with_Wang_Tiles_.pdf](./Quasi_regular_Patterns_with_Wang_Tiles_.pdf)
- [实现计划](./docs/implementation-plan.md)
- [数学与坐标规范](./docs/math-spec.md)
- [验证方案](./docs/validation.md)

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

构建和运行说明将在首个可执行版本建立后补充。
