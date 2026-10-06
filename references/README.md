# 当前参考文献库

本目录保留 QRP–Wang 联合机制及历史对照需要的文献元数据和本地阅读材料。
`papers/` 中的第三方 PDF 仅用于个人研究，已被 Git 忽略，不随公开仓库分发。

可提交内容：

- `library.bib`：当前论文写作使用的 BibTeX；
- 本文件：本地 PDF 的来源与用途；
- [当前阅读与推导](../docs/qrp-ordered-content-research.md)：文献对现有方法的启发及适用边界；
- [历史文献地图](../docs/history/literature-map.md)：早期项目定义和实现之间的对应关系。

## 本地 PDF

| 本地文件 | 公开来源 | 当前用途 |
| --- | --- | --- |
| `papers/cohen_2003_wang_tiles_image_texture.pdf` | [作者页面](https://graphics.uni-konstanz.de/publikationen/Cohen2003WangTilesImage/index.html) | Wang 边标签、有限 tile set 与合法选片语义 |
| `papers/lagae_dutre_2006_colored_edges_corners.pdf` | [作者预印本页面](https://graphics.cs.kuleuven.be/publications/LD06AWTCECC/) | edge-colored/corner-colored 区别与角点问题 |
| `papers/yin_et_al_2024_qrp_periodic_tiling.pdf` | [公开稿](https://iccvm.org/2023/papers/s10-2-470-CVMJ.pdf)、[论文 DOI](https://doi.org/10.1007/s41095-023-0359-z) | QRP 函数族、周期多边形铺砌与局部对称的不变映射 |
| `papers/liu_2009_textile_qrp.pdf` | [出版社页面](https://www.mecs-press.org/ijieeb/ijieeb-v1-n1/v1n1-7.html) | QRP 函数扩展、参数与分层配色；未提供完整 110 函数工程 |
| `papers/lin_kaplan_2023_freeform_islamic_patterns.pdf` | [arXiv v1](https://arxiv.org/abs/2301.01471v1) | 规则母题、邻接关系和连接部分的联合构造；非 QRP/Wang，不当作无限非周期证明 |
| `papers/labbe_2021_toral_wang_markov.pdf` | [期刊全文](https://ahl.centre-mersenne.org/item/10.5802/ahl.73.pdf) | 环面状态与有限 Wang 编码的成立条件；严格区分正向包含、反向等价与固定内容 |
| `papers/derouet_jourdan_2017_procedural_wall.pdf` | [arXiv v1](https://arxiv.org/abs/1706.03950v1) | 内容结构约束、块补全及即时求值；2017 预印本，不代替 2019 刊本 |
| `papers/yeh_2013_tiled_patterns_published.pdf` | [作者托管刊本](https://graphics.stanford.edu/~yitingy/papers/tiles.pdf) | 边界合法与跨块结构的区别、硬/软因子及可满足性；13 页全文已读，不采用样例图片库路线 |
| `papers/lagae_2009_gabor_noise_corrected.pdf` | [作者页面与勘误](https://graphics.cs.kuleuven.be/publications/LLDD09PNSGC/) | 10 页纠正版全文已读；直接局部波场叠加的已有范式和限制，不把频谱控制等同母题秩序 |

Yin 等人的本地公开稿题名使用 `Tiling`，期刊版题名使用 `Tilting`；BibTeX 采用
期刊正式题名。

2026-09-29 定向重读及新增公开稿的实际阅读范围、公式条件、失效边界见
[有秩序内容研究](../docs/qrp-ordered-content-research.md)。该记录与论文写作基线分开；
新增参考不表示已采用其算法，也不以全文阅读代替独立复现。

2019 年 *Generating Stochastic Wall Patterns On-the-fly with Wang Tiles*
（[DOI](https://doi.org/10.1111/cgf.13635)）另完整阅读了
[作者上传的网页正文及附录](https://www.researchgate.net/publication/333663528_Generating_Stochastic_Wall_Patterns_On-the-fly_with_Wang_Tiles)，
未取得刊本 PDF。版本与阅读范围见上述研究记录第 9 节。

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
