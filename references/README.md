# 本地参考文献库

本目录用于保存项目的文献元数据和本地阅读材料。`papers/` 中的第三方 PDF 只用于个人
研究，已通过 `.gitignore` 排除，不随公开 Git 仓库分发。可以提交的内容包括：

- `library.bib`：论文写作和 Overleaf 可直接使用的 BibTeX 数据；
- `../docs/literature-map.md`：文献与数学定义、实现模块和待核查问题的对应关系；
- 本文件中的公开来源、许可证或访问说明。

## 已获取的本地资料

| 本地文件 | 公开来源 | 获取与分发说明 |
| --- | --- | --- |
| `papers/cohen_2003_wang_tiles_image_texture.pdf` | [University of Konstanz 作者页面](https://graphics.uni-konstanz.de/publikationen/Cohen2003WangTilesImage/index.html) | 论文注明可供个人或课堂使用；不在公开仓库再分发 |
| `papers/lagae_dutre_2006_colored_edges_corners.pdf` | [KU Leuven 作者预印本页面](https://graphics.cs.kuleuven.be/publications/LD06AWTCECC/) | 作者公开预印本；本项目仍按本地资料处理 |
| `papers/fu_leung_2005_wang_tiles_surfaces.pdf` | [Eurographics Digital Library](https://diglib.eg.org/items/5328973e-c6a4-46b7-8b78-8f3f807f4721) | 机构数字图书馆版本 |
| `papers/wei_2004_tile_based_texture_mapping.pdf` | [作者项目页面](https://graphics.stanford.edu/papers/tile_mapping_gh2004/) | 作者公开版本；本项目仍按本地资料处理 |
| `papers/yin_et_al_2024_qrp_periodic_tiling.pdf` | [论文 DOI](https://doi.org/10.1007/s41095-023-0359-z) | 本地公开稿题名使用 `Tiling`，期刊版题名使用 `Tilting`；用于 QRP、周期铺砌和不变映射核查 |
| `papers/wang_et_al_2025_arbitrary_quadrilateral_tilings.pdf` | [期刊页面](https://doi.org/10.3390/sym17081315) | CC BY 4.0；为统一管理仍不提交 PDF |

## 暂不下载全文的资料

- Hao Wang 1961 年原始论文：保留 DOI 和书目信息；当前任务主要使用后续图形学论文的
  定义，原文不是工程实现的阻塞项。
- *Weak Chaos and Quasi-Regular Patterns*：版权专著。保留 Cambridge 出版信息，需要时
  通过学校图书馆或合法电子书渠道阅读，不使用来源不明的扫描件。
- SIGGRAPH 2008 *Tile-Based Methods for Interactive Applications*：内容全面但文件较大，
  第一轮先使用其公开网页和分章节资料；遇到具体问题时再补对应章节。

## 使用规则

1. 引用任何公式前，在 `docs/literature-map.md` 中记录输入域、符号、约束和适用条件。
2. “视觉上相似”不能作为算法等价证据；必须追踪到坐标和边界条件。
3. 本地 PDF 与 `library.bib` 的题名、作者、年份和 DOI 应保持一致。
4. 新增 PDF 时先确认来源与许可，再决定仅本地保存还是允许公开分发。
