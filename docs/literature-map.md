# 当前文献地图

本文档说明外部文献在当前 `20 × 20` C¹ Wang-QRP 实验中的用途。外部论文用于术语、
背景和相关工作核查；当前权重场、相位耦合与层级合成公式是本项目模型，不能写成外部
论文的直接复现。

## 1. 必须分开的概念

1. **Wang 邻接**：边标签约束相邻瓦片是否合法。
2. **连续内容构造**：合法标签如何进入解析场，并使共享边上的值与导数一致。
3. **QRP 内容模型**：完整共振方向集合如何产生准规则干涉结构。
4. **有限实验与非周期性**：固定窗口中未发现短周期不等于证明无限铺砌严格非周期。

仅生成合法标签网格不能证明可见内容由 Wang 标签控制；仅得到无接缝图像也不能证明内容
构造遵守 Wang 语义。当前实验用 uniform–Wang 单变量对照隔离标签调制贡献。

## 2. 保留文献及其用途

| 文献 | 当前用途 | 不承担的结论 |
| --- | --- | --- |
| Wang 1961 | Wang tile、合法铺砌和判定问题的术语起点 | 不直接提供当前程序化纹样公式 |
| Cohen et al. 2003 | 图形学中边着色 Wang tile、有限 tile set 与受约束选片语义 | 不作为预生成图像瓦片的实现规范 |
| Lagae & Dutré 2006 | edge-colored 与 corner-colored tile 的区别，以及角点组合问题 | 不直接证明当前场的角点连续性 |
| Yin et al. 2024 | QRP、共振方向和周期倾斜背景 | 当前二阶相位 Wang 耦合不是其原样复现 |
| Zaslavsky et al. 1992 | quasi-regular pattern 与弱混沌的历史背景 | 不作为当前 C++ 公式的逐式来源 |

对应 BibTeX 条目见 `references/library.bib`。本地可阅读 PDF 与来源说明见
`references/README.md`。

## 3. 文献—定义—实现对应

| 概念 | 当前定义 | 代码位置 | 验证 |
| --- | --- | --- | --- |
| 合法 Wang 邻接 | 相邻瓦片共享边标签相等 | `src/model/WangGrid.*` | `qrp_wang_grid_tests` |
| Canonical QRP | 完整方向集合的余弦和；参数化场使用其归一化形式 | `src/generators/CanonicalQrpField.*` | `qrp_canonical_qrp_tests` |
| 标签边界数据 | 标签映射为边值与固定坐标方向导数 | `src/model/WangContentWeight.*` | `qrp_wang_content_weight_tests` |
| C¹ 内部延拓 | 两组三次 Hermite 延拓之和 | `src/model/WangContentWeight.*` | 全签名范围与兼容边一阶数据 |
| 二阶相位耦合 | `A=A0+lambda_A U`、`B=B0+lambda_B U` | `src/model/ParametricWangQrpField.*` | `qrp_parametric_wang_qrp_field_tests` |
| 多尺度门控 | `F=C+beta_m(1-C²)D_m+beta_f(1-C²)²D_f` | `src/model/HierarchicalQrpComposition.*` | `qrp_hierarchical_qrp_composition_tests` |

`WangContentWeight`、`ParametricWangQrpField` 和
`HierarchicalQrpComposition` 是当前项目的组合贡献。引用相关背景时，应明确区分
“受某文献启发”与“文献已经给出该公式”。

## 4. 写作与核查规则

### 4.1 Wang 术语

- “颜色”在 Wang tile 文献中通常表示离散兼容标签，不是最终图像 RGB 颜色。
- 当前 `WangGrid` 生成有限的随机合法布局；这不等于构造了数学意义上的 aperiodic
  tile set。
- 当前所有瓦片共享一个解析内容公式，不应描述成每种标签签名拥有独立纹理图片。

### 4.2 QRP 术语

- `q` 表示完整共振方向数，`kappa` 表示空间频率。
- 非晶体学方向组合通常不是单位环面周期函数。
- “准规则”描述干涉结构和参数背景，不能自动推出严格非周期或统计性质。

### 4.3 连续性

- 共享边 `C¹` 结论来自本项目明确的边参数方向、固定坐标导数约定和 Hermite 延拓。
- 视觉上无缝不是数学证明；测试必须比较共享边两侧的场值与完整梯度。
- 当前结论仅覆盖轴对齐、单位大小、不旋转且不镜像的正方形瓦片。

### 4.4 实验结论

- RMSD 与符号分歧率证明固定窗口中的数值差异，不证明拓扑变化。
- uniform–Wang 对照证明 Wang 权重对当前结果有可见贡献，不证明无限铺砌非周期。
- 中、细尺度场在所有参数组中固定，因此不能把它们写成结构差异来源。

## 5. 当前阅读入口

建议按以下顺序核查：

1. Cohen et al. 2003：确认 Wang 边标签、有限集合和合法邻接的图形学语义。
2. Lagae & Dutré 2006：确认边标签模型在角点处需要额外约束意识。
3. Yin et al. 2024：核对 QRP 共振方向、符号和相关工作表述。
4. Wang 1961 与 Zaslavsky et al. 1992：仅在需要历史或术语背景时查阅。
5. 回到 [QRP–Wang 数学模型](content-constrained-wang-model.md)，逐项区分外部背景与
   本项目定义。
