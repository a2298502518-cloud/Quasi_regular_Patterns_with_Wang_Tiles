# 当前参考文献库

本目录只保留当前 C¹ Wang-QRP 模型需要的文献元数据和本地阅读材料。
`papers/` 中的第三方 PDF 仅用于个人研究，已被 Git 忽略，不随公开仓库分发。

可提交内容：

- `library.bib`：当前论文写作使用的 BibTeX；
- 本文件：本地 PDF 的来源与用途；
- `../docs/literature-map.md`：文献、项目定义和实现之间的对应关系。

## 本地 PDF

| 本地文件 | 公开来源 | 当前用途 |
| --- | --- | --- |
| `papers/cohen_2003_wang_tiles_image_texture.pdf` | [作者页面](https://graphics.uni-konstanz.de/publikationen/Cohen2003WangTilesImage/index.html) | Wang 边标签、有限 tile set 与合法选片语义 |
| `papers/lagae_dutre_2006_colored_edges_corners.pdf` | [作者预印本页面](https://graphics.cs.kuleuven.be/publications/LD06AWTCECC/) | edge-colored/corner-colored 区别与角点问题 |
| `papers/yin_et_al_2024_qrp_periodic_tiling.pdf` | [论文 DOI](https://doi.org/10.1007/s41095-023-0359-z) | QRP、共振方向与周期倾斜背景 |

Yin 等人的本地公开稿题名使用 `Tiling`，期刊版题名使用 `Tilting`；BibTeX 采用
期刊正式题名。

## 仅保留书目信息

- Wang 1961：Wang tile 与判定问题的术语起点。
- Zaslavsky et al. 1992：quasi-regular pattern 与弱混沌的历史背景；需要时通过学校
  图书馆或合法电子书渠道查阅。

## 使用规则

1. 外部文献只承担背景、术语和相关工作作用。
2. 本项目的 C¹ Hermite 权重、二阶相位 Wang 耦合和层级公式不得写成外部论文的直接
   复现。
3. “视觉上相似”不能作为算法等价或严格非周期的证据。
4. 引用公式前核对输入域、符号、约束和适用条件。
5. 新增 PDF 前先确认公开来源与许可；默认只保存在本地。
