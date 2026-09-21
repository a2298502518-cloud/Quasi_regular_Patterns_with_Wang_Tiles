# 项目交接记录（2026-09-18）

## 1. 当前项目定位

项目当前只保留一个研究问题：如何用 Wang 边标签连续地调制 QRP 的内容参数，使不同
参数组产生明显不同的粗尺度结构，同时保证合法共享边上的场值和完整一阶导数连续。

当前权威结果是 `20 × 20` CPU 独立实验。它不是实时产品路径，也没有 GPU 或 UI
集成。下一步工作必须以当前数学模型、五组单元测试和七张固定实验图为共同基线。

## 2. 方法边界

当前方法遵守以下约束：

1. 不预生成图片瓦片或图片字典；
2. 不使用逐瓦片随机噪声、随机相位、随机旋转或独立内容公式；
3. 所有瓦片使用同一解析模型，真实边标签进入连续权重场；
4. 参数组之间保持 Wang 网格、层级残差、色带和分辨率一致；
5. 不通过逐图归一化、模糊或后处理掩盖粗场问题；
6. 只声称有限窗口中的数值与视觉结果，不声称严格非周期。

## 3. 当前数学模型

### 3.1 全局 C¹ Wang 权重

`src/model/WangContentWeight.*` 将南、北、西、东四条边标签映射为边界值与固定全局
坐标方向的一阶数据，再用两组三次 Hermite 基延拓到瓦片内部。合法相邻瓦片共享同一
标签、边参数方向和导数约定，所以分片场 `W(x)` 在整个有限网格上为全局 `C¹`。

当前固定参数为

```text
K = 5
w* = 0.5
rho = 0.10
sigma = 0.40
R_W = 2|rho| + |sigma|/2 = 0.40
W in [0.10, 0.90]
U = (W - w*) / R_W in [-1, 1]
```

### 3.2 参数化 QRP 粗场

`src/model/ParametricWangQrpField.*` 使用完整共振方向集合。对方向
`theta_r=2*pi*r/q`，定义

\[
c_r=\cos(2\theta_r),\qquad s_r=\sin(2\theta_r),
\]

\[
A=A_0+\lambda_AU,\qquad B=B_0+\lambda_BU,
\]

\[
C(x)=\frac1q\sum_{r=0}^{q-1}
\cos\left(\kappa e_r^Tx+A c_r+B s_r\right).
\]

`(A0,B0)` 改变同一共振星内部的相对相位，
`(lambda_A,lambda_B)` 控制 Wang 标签的连续调制。

### 3.3 多尺度层级

`src/model/HierarchicalQrpComposition.*` 使用

\[
F=C+\beta_m(1-C^2)D_m+\beta_f(1-C^2)^2D_f.
\]

中、细尺度场在所有参数组中固定，只增加统一细节，不负责制造粗骨架差异。

`src/generators/CanonicalQrpField.*` 是参考定义和测试基线，不是第二条产品路径。

## 4. 固定实验配置

唯一实验入口：

```text
apps/wang_qrp_experiment_main.cpp
target: qrp_wang_qrp_experiment
```

固定配置：

| 项目 | 当前值 |
| --- | --- |
| 网格 | `20 × 20` |
| 每瓦片分辨率 | `80 × 80` |
| 单图分辨率 | `1600 × 1600` |
| Wang 标签数 | `5` |
| 网格种子 | `0x4d595df4d0f33173` |
| 粗尺度 | `q=7, kappa=3.15` |
| Wang 相位耦合 | `(lambda_A,lambda_B)=(1.15,-0.80)` |
| 中尺度 | `q=9, kappa=7.4, (A0,B0)=(0.55,-0.30)` |
| 细尺度 | `q=11, kappa=12.8, (A0,B0)=(-0.25,0.45)` |
| 层级强度 | `(beta_m,beta_f)=(0.19,0.055)` |
| 色带 | `MidnightGold` |

四组粗尺度全局相位：

| 组别 | `(A0,B0)` | 输出 |
| --- | --- | --- |
| P0 | `(0.00,0.00)` | `P0_zero_phase.png` |
| P1 | `(1.65,0.00)` | `P1_axis_phase.png` |
| P2 | `(3.00,0.85)` | `P2_oblique_phase.png` |
| P3 | `(2.10,-2.45)` | `P3_opposed_phase.png` |

P0 仍保留 Wang 相位耦合，不能称为 uniform 或 canonical 基准。

## 5. 输出与测量

当前输出目录为 `output/wang-qrp-experiment/`，共七张图：

- `A_coarse_parameter_family.png`：四组粗场；
- `B_layered_parameter_family.png`：四组统一分层结果；
- `C_uniform_vs_wang.png`：P2 的中性权重与真实 Wang 权重对照；
- 四张 P0–P3 独立分层结果。

粗场成对测量：

| 参数对 | RMSD | 符号分歧率 |
| --- | ---: | ---: |
| P0–P1 | 0.291436 | 36.69% |
| P0–P2 | 0.436893 | 59.82% |
| P0–P3 | 0.438151 | 60.49% |
| P1–P2 | 0.276631 | 34.20% |
| P1–P3 | 0.389104 | 51.56% |
| P2–P3 | 0.447371 | 61.16% |

这些数字只说明固定窗口中的粗场存在显著差异。

## 6. 当前实验报告

当前报告为：

```text
reports/基于QRP与Wangtile的准规则纹样生成实验报告.docx
```

报告共 9 页，包含当前模型公式、固定配置、粗场对照和测量结果。后续修改报告时必须继续
区分全局相位参数变化与 Wang 权重变化，并保持 P0 的含义准确。

报告的可复现生成源位于 `tools/report/`，其中保留 23 个 MML 公式基线。生成报告前必须
先生成 `output/wang-qrp-experiment/A_coarse_parameter_family.png`；具体依赖和命令见
`tools/report/README.md`。

## 7. 当前验证

当前测试集合收敛为五个 CTest 目标：

1. `qrp_wang_grid_tests`
2. `qrp_canonical_qrp_tests`
3. `qrp_wang_content_weight_tests`
4. `qrp_hierarchical_qrp_composition_tests`
5. `qrp_parametric_wang_qrp_field_tests`

它们分别覆盖合法 Wang 网格、canonical QRP、C¹ 权重场、层级公式和参数化 QRP。
实验程序目前不是独立 CTest smoke；运行实验和目视检查七张图仍是单独步骤。

## 8. 关键文件

| 文件 | 职责 |
| --- | --- |
| `README.md` | 当前入口、构建和范围说明 |
| `docs/content-constrained-wang-model.md` | 当前数学定义 |
| `docs/validation.md` | 当前验证矩阵 |
| `docs/literature-map.md` | 外部文献用途与声明边界 |
| `src/model/WangGrid.*` | 合法标签网格 |
| `src/generators/CanonicalQrpField.*` | canonical QRP 参考 |
| `src/model/WangContentWeight.*` | 全局 C¹ Wang 权重 |
| `src/model/ParametricWangQrpField.*` | 二阶相位参数化 QRP |
| `src/model/HierarchicalQrpComposition.*` | 多尺度层级 |
| `src/color/GradientPalette.*` | 固定色带 |
| `src/render/Image.*`、`src/export/PngWriter.*` | CPU 图像缓冲与 PNG 写出 |
| `apps/wang_qrp_experiment_main.cpp` | 唯一实验入口 |
| `tools/report/` | 当前 Word 报告生成源与公式基线 |
| `reports/基于QRP与Wangtile的准规则纹样生成实验报告.docx` | 当前实验报告 |

## 9. 复现命令

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake --build build --config Release --target qrp_wang_qrp_experiment
.\build\Release\qrp_wang_qrp_experiment.exe output\wang-qrp-experiment
```

运行后核对五项 CTest、七张图和控制台中的六组成对测量。

## 10. 后续协作约束

- 数学定义、实现、测试、实验参数和报告必须同步更新。
- 不再增加与当前公式竞争的实验入口。
- 新结论必须注明来自解析证明、单元测试、数值统计还是视觉观察。
- 未完成 GPU、UI、通用导出或严格非周期证明前，不在文档中写成已完成。
- 生成物、构建目录和临时审计文件不作为源码事实依据。
