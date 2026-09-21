# Quasi-regular Patterns with Wang Tiles

本项目研究一种确定性的程序化纹样模型：使用 Wang 瓦片边标签构造全局
`C¹` 连续参数场，再用该参数场调制 quasi-regular pattern（QRP）的方向相位。
当前仓库只保留一条权威实现路径，即 `20 × 20` 的 CPU Wang-QRP 实验。

## 当前方法

每个瓦片使用同一套解析公式。瓦片之间的差异仅来自合法 Wang 网格上的四条边标签，
不使用预生成图片瓦片、逐瓦片随机相位、逐瓦片随机噪声、随机旋转或逐图归一化。

当前数据流为：

```text
WangGrid
  -> WangContentWeight
  -> ParametricWangQrpField
  -> HierarchicalQrpComposition
  -> GradientPalette
  -> PNG
```

- `WangGrid` 生成满足相邻边标签一致的有限网格。
- `WangContentWeight` 将四条边标签延拓为全局 `C¹` 权重场 `W(x)`。
- `ParametricWangQrpField` 用二阶角谐波相位参数生成粗尺度 QRP 场。
- `HierarchicalQrpComposition` 以统一门控公式加入固定的中、细尺度残差。
- 实验入口将场值映射到固定色带并输出可复现实验图。

当前结论只适用于有限的 `20 × 20` 实验窗口。项目没有证明无限铺砌严格非周期，
也没有声称已完成 GPU、实时 UI 或交互编辑器集成。

## 代码结构

| 路径 | 职责 |
| --- | --- |
| `src/model/WangGrid.*` | 合法 Wang 边标签网格 |
| `src/generators/CanonicalQrpField.*` | Canonical QRP 参考定义 |
| `src/model/WangContentWeight.*` | 标签到全局 `C¹` 权重场 |
| `src/model/ParametricWangQrpField.*` | 二阶角谐波参数化 QRP |
| `src/model/HierarchicalQrpComposition.*` | 多尺度层级合成 |
| `src/color/GradientPalette.*` | 固定连续色带 |
| `src/render/Image.*` | CPU RGB 图像缓冲 |
| `src/export/PngWriter.*` | PNG 编码与写出 |
| `apps/wang_qrp_experiment_main.cpp` | 唯一实验入口 |
| `tests/` | 五组数学与模型单元测试 |
| `tools/report/` | 当前 Word 实验报告的可复现生成源 |

CMake 的运行时依赖只包含 `qrp_wang_qrp_core`、`qrp_raster`、`qrp_png` 和
`qrp_wang_qrp_experiment`。`qrp_qrp_reference` 仅在启用测试时构建，作为
canonical QRP oracle，不进入实验程序的依赖图。

数学定义以
[当前模型说明](docs/content-constrained-wang-model.md)为准；验证边界见
[验证方案](docs/validation.md)，文献用途见
[文献地图](docs/literature-map.md)，固定实验状态见
[项目交接记录](docs/chat-handoff-2026-09-18.md)。

## 构建与测试

Windows、Visual Studio 2022：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

当前测试集合由五个 CTest 目标组成：

- `qrp_wang_grid_tests`
- `qrp_canonical_qrp_tests`
- `qrp_wang_content_weight_tests`
- `qrp_hierarchical_qrp_composition_tests`
- `qrp_parametric_wang_qrp_field_tests`

## 运行实验

```powershell
cmake --build build --config Release --target qrp_wang_qrp_experiment
.\build\Release\qrp_wang_qrp_experiment.exe output\wang-qrp-experiment
```

实验固定使用 `20 × 20` 网格、每瓦片 `80 × 80` 像素、五种边标签和固定随机种子。
输出目录包含七张图：

- `A_coarse_parameter_family.png`
- `B_layered_parameter_family.png`
- `C_uniform_vs_wang.png`
- `P0_zero_phase.png`
- `P1_axis_phase.png`
- `P2_oblique_phase.png`
- `P3_opposed_phase.png`

当前实验报告为
`reports/基于QRP与Wangtile的准规则纹样生成实验报告.docx`，生成说明见
`tools/report/README.md`。

## 当前边界

- 共享边的场值和完整梯度由解析构造与单元测试共同验证。
- 参数组之间的差异使用固定网格、固定层级和固定色带进行比较。
- `uniform--Wang` 对照只切换权重场，避免混入其他变量。
- 当前输出是研究实验结果，不是已集成的实时产品路径。
- 严格非周期性、GPU 等价实现、交互编辑和通用导出格式均不在当前完成范围内。
