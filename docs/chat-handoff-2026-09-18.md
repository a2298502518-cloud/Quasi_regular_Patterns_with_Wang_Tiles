# 项目交接记录（2026-09-18）

> 历史阶段记录：旧代码与命令已归档至 `b58337a`，见 [恢复说明](legacy-experiments.md)。当前入口见 [README](../README.md)。

**09-23 工程整理：当前状态和运行入口以 [README](../README.md) 为准。**
默认程序现为配方驱动的相位兼容瓦片研究，旧全局场和来源混合入口移至可选的
`qrp_legacy_experiment`；复现命令迁移见 [历史实验](legacy-experiments.md)。
本次只调整职责、依赖和输出元数据，没有修改生成公式，也没有删除既有纹样证据。
以下按时间保留的记录不是多份并行生效的“当前方案”。

本记录保留 09-18 固定实验的历史基线。09-21 的 QRP 方向扩展与本地试调台现状见
[方向性探索记录](qrp-direction-study.md)，当前运行入口见仓库 README；
不要把下文的“尚无 UI / 仅改变相位”等历史状态当作当前限制。

后续用户指出局部造型杂乱，已继续完成 [局部秩序研究](qrp-order-study.md)。
用户已选择 A，早期诊断见 [A 路线造型诊断](qrp-contour-study.md)。不推进规则载波；
用户明确调试台不是现阶段重点。
试调台代码虽已接入 A 候选，但本轮交互复核未完成，不把它列为已完成的核心成果。
后续用户再次强调：每轮展示纹样结果，且应做方法论修改，不继续主要靠小参数调整修补。
最新澄清更重要：用户认可现有环瓣类效果，不能再把混合形状自动当作缺陷。
此前 12 组 [造型控制研究](qrp-control-study.md) 复用方向权重、旋转和共同相位，
并给出当前配置下贯通波带的充分条件。此前“单元越一致越好”的判断不再作为任务目标。

**09-22 最新优先级：风格多样性是主线，控制研究只是基础，不继续围绕单一风格打转。**
已完成 [风格跨度研究](qrp-diversity-study.md)：13 组 640px/1600px 对照；
保留环瓣与波带，增加 QRP 通道乘积、联合能量、父子门控三种组合关系。
视觉观察支持保留交错格纹、曲线网格、层级环簇和分区条纹；联合能量未达到预期细胞风格。
数学扩展见 [模型 §5.4](content-constrained-wang-model.md#54-面向风格跨度的-qrp-通道组合)，
仍由 QRP 造型、Wang 内部铺砌，不做界面。六组 CTest 通过；不把候选图当作商业价值已验证。
后续先依据用户对新结构的判断选择继续扩展的方向，不默认回到一致性修复或防御测试。

**09-22 后续关键修正：用户要求 Wang 真正承担可复用内容铺砌。**
原全局场接边证明正确，但内容依赖全局位置；不得再把前述风格图当作独立内容瓦片的证据。
已完成 [独立瓦片首轮研究](qrp-reusable-tile-study.md)：16 种端点编码类型，九块 QRP 来源
凸组合，固定/匹配来源 × 环瓣/方向波带/乘积共六例。预烘焙后纯 ID 复制，两个合法布局的
768 个实例全部与源图片像素一致；七组 CTest 通过，实际共享边值/完整梯度抽样误差为 0。
但造型不能视为完成：固定来源显方格，匹配来源出现粘连和波带断连；64% 瓦片面积处于
过渡混合区。下一步应先推导造型与边界的联合兼容约束，而非继续微调匹配权重。
结果板位于 `output/qrp-tile-study/`；保留历史好效果和本轮负面结果，不接入 UI、不提交推送。
后续以用户对实际图样的判断为准，不把规则程度本身当作美感判据。

**09-22 再后续：已做逐模态相位兼容生成候选。**
见 [相位兼容研究](qrp-phase-compatible-tile-study.md)。不再混合标量来源，直接按边界约束
生成相位；七组测试通过，L=4 / L=8 六例及同库周期对照共 120 张原图，
480 个 Wang 实例和 240 个周期对照实例像素原样复用。
波带和格纹更连贯，有贯通核心带的充分条件；环形仍显规则组织，增大瓦片没有消除它。
已确认的取舍：整数相位闭合改变原 QRP 的全局频谱，含周期骨架，不能声称精确原始 QRP。
**用户已明确接受扩展，重点验证风格与铺砌价值，不再等待这一理论身份的确认。**
新增同库类型 0 合法重复与 Wang A/B 的对照，体现局部变化及重铺价值，但不宣称商业价值已证明。
下一阶段可验证保持兼容性的层级/门控关系，扩大风格跨度；不做 UI，不靠持续微调单风格。
输出 `output/qrp-phase-tile-study/`，仍保留旧结果；本轮未提交或推送。

**09-22 最新进展：相位兼容瓦片的父子组织与低频谱分辨率。**
已完成 [父子组织研究](qrp-organization-tile-study.md)，复用现有 Nested 公式，加入分簇
细环形、分簇条纹和带内横向短条。首轮 L=4 的低频等权父场出现纵列偏置，推导发现整数
圈闭合将五个平均波矢合并成轴向方向，不能用“振幅保持”推断方向结构也保持。
增加内部规则：在 L=4/8/16/32 中选全部研究通道相对闭合误差≤0.25 的首个跨度；当前 L=16，
误差从约 0.810 降至 0.158。源视野、QRP 参数、总像素不变，网格从 8×8 改为 2×2，
并非同布局的单因素消融，也不是完整局部梯度误差保证。当前 25% 方向分辨率依据针对 q=5。
第二轮二维父场组织更清楚；周期骨架与门控截短仍在，不以小碎片存在自动判定效果失败。
两轮 320 张原图、1,088 个 Wang 实例和 544 个合法周期实例均原样复用；七组 CTest 通过，
旧两研究 231 张 PNG 回归字节一致。没有新增模型公式副本、测试目标或 UI。
先看 `output/qrp-organization-tile-study/resolved-scale/board_style_span.png`，来源、重铺及
普通重复对照同目录；首轮负面结果留在根目录。每个库 16 张，但 2×2 布局未覆盖全部类型。
下一步继续评价组织风格与更大视野下的可见重组，不重问是否接受 QRP 扩展、不默认开展 UI。
仍未提交或推送。

**09-22 最新一轮：大范围重铺与内部相对相位状态。**
已完成 [大范围重铺研究](qrp-retiling-study.md)。先固定旧 L=16/384px 库扩到 8×8、3072px，
三类共 48 张源图与上一轮字节一致，两布局各包含全部 16 类型。大范围仍有强公共骨架，
回查发现原状态只沿一条源平移路径变化，低频父场尤其变化有限。
第二轮在 `PhaseCompatibleQrpTiles` 增加默认零的 `vertexPhaseOffsets`，仅给父场两个
内部状态赋 QRP 相对相位 (−1,−0.5)/(1,0.5)，子场、布局、标签与采样不变。
仍先烘焙 16 张再纯 ID 复制；新的相位常量保持既有 C¹ 接边，证书自动使用总状态相位差。
簇群形状和短条列起伏变化更明显，但前两类失去部分完整大环。保留两种风格分支，
不能自动将“更不规则”称作更好；内部相位不是新的 Wang 用户控件。
根目录 `output/qrp-retiling-study/` 是旧库大范围结果，新库在 `relative-phase/`；先看
`board_phase_states.png` 和 `board_phase_states_detail.png`。114 张原图、768 个 Wang 实例
及 384 个合法周期实例全部像素复用；七组 CTest 通过，旧两研究 231 张 PNG 回归一致。
下一阶段按实际风格用途组织 QRP 配方，不继续以消除规则性或最大化像素差异为目标。
没有改 UI，没有新增测试目标；未提交或推送。

以下各节保留 09-18 的历史快照；其中禁止图片字典、模型数、测试数、入口和“下一步”
不覆盖上述最新状态。当前允许预生成由 QRP 定义的固定内容库，不使用外部图案模板。

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
