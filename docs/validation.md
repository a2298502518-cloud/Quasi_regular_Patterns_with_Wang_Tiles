# 当前验证方案

本文档只覆盖 `20 × 20` C¹ Wang-QRP 实验。验证分为解析性质、单元测试、固定实验对照
和视觉检查四层。编译成功或单张效果图不能单独证明模型成立。

## 1. 验证原则

1. 先验证局部公式，再验证共享边连续性，最后检查完整实验图。
2. 解析梯度必须与中心差分一致，不能只检查场值。
3. 所有范围结论必须来自公式证书或固定阈值，不使用逐图归一化修补。
4. 参数因果对照一次只改变一个因素。
5. 数值差异、视觉差异、拓扑差异和严格非周期性是不同强度的结论。

## 2. 五组单元测试

当前 CTest 只包含五个目标。

### 2.1 `qrp_wang_grid_tests`

验证内容：

- 网格尺寸、标签范围和参数校验；
- 相邻瓦片共享边标签完全相等；
- 相同尺寸、标签数和种子生成相同网格；
- 不同合法种子仍满足邻接约束。

该测试只证明有限网格合法且可复现，不证明无限 Wang tile set 严格非周期。

### 2.2 `qrp_canonical_qrp_tests`

验证内容：

- canonical QRP 参考和的原点值、模态数和振幅界；
- 四方向特殊情形的闭式值、闭式梯度和周期性；
- 解析梯度与中心差分一致；
- 完整共振方向集合的旋转对称性；
- 归一化采样坐标到模型坐标的映射；
- 项目实际使用的非晶体学方向组合不能误写为单位环面周期。

### 2.3 `qrp_wang_content_weight_tests`

验证内容：

- 合法与非法参数的统一处理；
- 局部解析梯度与中心差分一致；
- `K=5` 的代表性认证参数下，全部 `5^4=625` 个边标签签名保持有限并满足范围；
- 每一方向上的全部兼容标签对在共享边上具有相同场值与完整梯度；
- 瓦片角点统一满足 `W=w*`、`grad W=0`。

共享边测试必须同时覆盖切向和横向导数，只比较像素颜色或只比较场值均不充分。

### 2.4 `qrp_hierarchical_qrp_composition_tests`

验证内容：

- 解析梯度与中心差分一致；
- 中、细尺度强度为零时精确恢复粗场值和梯度；
- 默认参数的保守范围采样；
- 负强度和不安全的强度和被拒绝。

### 2.5 `qrp_parametric_wang_qrp_field_tests`

验证内容：

- 零全局相位、零 Wang 相位时退化为归一化 canonical QRP；
- 参数化 QRP 的解析梯度与中心差分一致；
- 改变方向数、空间频率、全局相位或 Wang 相位会改变场值；
- 余弦平均始终位于 `[-1,1]`；
- 非法方向数、频率、权重中心或认证半径被拒绝。

## 3. 构建与运行

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

预期 CTest 名称为：

```text
qrp_wang_grid_tests
qrp_canonical_qrp_tests
qrp_wang_content_weight_tests
qrp_hierarchical_qrp_composition_tests
qrp_parametric_wang_qrp_field_tests
```

测试结果应以当前构建目录中本次运行的 CTest 输出为准，不能使用旧构建目录中的历史日志
替代。

## 4. 固定实验验证

运行：

```powershell
cmake --build build --config Release --target qrp_wang_qrp_experiment
.\build\Release\qrp_wang_qrp_experiment.exe output\wang-qrp-experiment
```

实验必须生成以下七张图：

| 文件 | 作用 |
| --- | --- |
| `A_coarse_parameter_family.png` | 四组粗尺度结构的 2×2 对照 |
| `B_layered_parameter_family.png` | 使用相同中、细尺度残差的四组最终结果 |
| `C_uniform_vs_wang.png` | P2 中性权重与真实 Wang 权重的单变量对照 |
| `P0_zero_phase.png` | P0 分层结果 |
| `P1_axis_phase.png` | P1 分层结果 |
| `P2_oblique_phase.png` | P2 分层结果 |
| `P3_opposed_phase.png` | P3 分层结果 |

四组结果必须共享：

- `20 × 20` Wang 网格及固定种子；
- 每瓦片 `80 × 80` 像素；
- 粗尺度的 `q`、空间频率和 Wang 相位耦合；
- 中、细尺度参数及层级强度；
- 同一色带和同一数值到颜色映射。

只有粗尺度 `(A0,B0)` 改变。uniform–Wang 对照只将真实权重替换为
`W=w*=0.5`，其余条件不变。

## 5. 数值对照

实验程序打印四个粗场之间的 RMSD 与符号分歧率。固定配置的基准值为：

| 参数对 | RMSD | 符号分歧率 |
| --- | ---: | ---: |
| P0–P1 | 0.291436 | 36.69% |
| P0–P2 | 0.436893 | 59.82% |
| P0–P3 | 0.438151 | 60.49% |
| P1–P2 | 0.276631 | 34.20% |
| P1–P3 | 0.389104 | 51.56% |
| P2–P3 | 0.447371 | 61.16% |

重新生成时允许最后若干位受到标准库与平台浮点实现影响，但不应出现数量级变化。
这些统计量用于证明参数组在固定窗口中不同，不能单独证明轮廓拓扑发生变化。

## 6. 视觉检查

每次改变数学模型、采样坐标、色带或输出尺寸后，至少检查：

- 七张图均成功解码、尺寸正确且画面非空；
- `A` 图中的四个粗骨架具有清楚差异；
- `B` 图中的统一层级没有覆盖粗骨架差异；
- `C` 图可识别 Wang 调制贡献，同时没有明显瓦片方格和共享边裂缝；
- P0 仍含 Wang 调制，不能误标成 canonical 或 uniform 基准；
- 图像方向与全局坐标约定一致，没有上下翻转导致的误读。

## 7. 当前未覆盖

当前验证不包含：

- 独立的 `qrp_wang_qrp_experiment` CTest smoke；
- GPU 与 CPU 数值一致性；
- 实时渲染、UI 或交互编辑；
- 通用项目序列化或机器可读实验清单；
- 任意旋转、镜像、缩放或非正方形瓦片；
- 无限铺砌严格非周期证明；
- 粗场拓扑变化的严格认证。

报告或论文不得把上述未覆盖项目写成已完成事实。
