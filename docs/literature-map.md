# 文献地图与实现核查清单

本文档把外部文献、项目数学定义和当前实现放在同一张核查表中。它不是一般性的相关
工作综述，而是后续算法修改和论文写作的证据入口。

本地主论文《Quasi-regular Patterns with Wang Tiles》是项目的实现目标，也是后续边做
实验边修订的主体。下列外部论文只承担概念澄清、理论借鉴和相关工作比较的作用，不把
任何一篇外部论文的完整复现当作本项目目标。

## 1. 当前需要分开的三个问题

1. **Wang 铺砌**：边标签如何约束 tile 的选择，随机铺砌与严格非周期铺砌有何区别。
2. **边界兼容的内容构造**：相同边标签如何保证纹理、几何、点分布或程序结构跨边连续。
3. **QRP 与不变映射**：QRP 的数学模型是什么，如何通过边界满足对称约束的映射填入
   基本区域，以及这些映射与本项目的 Coons warp 是否等价。

仅满足第 1 项并不能说明可见图案由 Wang Tile 产生；仅在连续世界坐标上生成无缝纹理，
也不能替代第 2 项的因果证明。

## 2. 核心文献及其用途

| 文献 | 解决的问题 | 对当前项目的直接作用 | 阅读状态 |
| --- | --- | --- | --- |
| Wang 1961 | Wang tile 与 domino problem 的数学起点 | 规范“tile set、合法铺砌、周期/非周期/aperiodic”的术语 | 保留书目信息，按需查阅 |
| Cohen et al. 2003 | 用少量 Wang tile 构造非周期纹理、对象分布和几何 | 判断 tile 内容是否真正服从边标签；核对扫描线随机铺砌 | 第一轮精读完成 |
| Wei 2004 | GPU 上的 tile atlas、寻址与过滤 | 核对实时渲染、缩放和过滤是否只在像素边界连续，还是在滤波足迹内也稳定 | PDF 已获取 |
| Fu & Leung 2005 | 在任意拓扑表面和四边形图上铺设 Wang tile | 为任意四边形网格阶段提供表面参数化和 tile orientation 参考 | PDF 已获取 |
| Lagae & Dutré 2006 | edge-colored Wang tile 的角点问题与 corner tile | 检查跨两个边的结构、胞元和离散对象在 tile 角点是否出现无约束组合 | 第一轮精读完成 |
| Yin et al. 2024 | QRP、k-uniform tiling 和不变映射的结合 | 为主论文中的 QRP 生成器提供背景与模型对照；不是复现目标 | 第一轮精读完成 |
| Wang et al. 2025 | 任意四边形基本区域上的 Coons 不变映射与图案着色 | 借鉴 Coons 边界构造并评估未来四边形扩展；不能替代主论文定义 | 第一轮精读完成 |
| Zaslavsky et al. 1992 | 弱混沌、Hamiltonian 系统和 quasi-regular patterns | 确认 QRP 的物理与数学背景，避免只凭名称解释 Fourier 叠加 | 通过图书馆或正版电子书查阅 |
| Lagae et al. 2008 | tile-based graphics 的系统课程资料 | 作为 Wang/corner tile、采样、NPR 和表面应用的总览与术语索引 | 在线按章节查阅 |

## 3. 与当前代码的数据流对应

| 文献概念 | 项目位置 | 当前判断 | 后续证据 |
| --- | --- | --- | --- |
| 边标签匹配 | `src/model/WangGrid.*` | 已实现随机合法网格 | 固定种子、邻边相等与选择分布测试 |
| 边标签对应边函数 | `src/model/EdgePalette.*`、`src/math/EdgeFunction.*` | 属于本地论文扩展，不是经典 Wang Tile 的通用定义 | 每种颜色的函数图、端点和单调性表 |
| 四边 Coons warp | `src/math/CoonsWarp.*` | 已实现，但全局可逆性不能只由单边单调推出 | 组合 Jacobian 下界、稠密采样和逆映射残差 |
| 逆映射后采样生成器 | `src/render/CpuReferenceRenderer.cpp`、`shaders/wang_pattern.frag` | 是论文核心管线 | identity/warped 对照图与 CPU/GPU 分阶段误差 |
| 周期生成器 | `src/generators/TorusFourier.*`、`PeriodicGradientNoise.*` | 主论文采用单位环面安全的整数频率版本 | 四边及角点周期测试；论文需给出自身定义与周期证明 |
| tile 内部随机变化 | `HybridTorusGenerator.*` 的 boundary window | 与 boundary-vanishing perturbation 思路一致，但窗口公式是项目强化版 | 边界值及一、二阶邻域检查 |

## 4. 必须独立核查的论断

### 4.1 Wang Tile 术语

- `non-periodic tiling` 不等于 `aperiodic tile set`。Cohen 等人的随机铺砌使用可产生
  周期铺砌的 tile set，但随机选择得到的具体铺砌通常不周期。
- Wang 边“颜色”是兼容性标签，不是最终图像颜色。论文中必须避免二者混用。
- 共享边像素相等只是最低条件；跨边对象、滤波足迹和角点邻域还需要更强的内容构造约束。

### 4.2 Coons 映射

- 单条边函数严格单调只保证边界上的一维可逆性，不能单独保证四边混合后的二维映射
  在全域内没有折叠。
- 当前项目采用组合 Jacobian 安全条件和稠密验证，这属于对本地草稿论证的加强，论文中
  应明确标为工程上的充分条件，而不是照抄原文结论。
- 任意非凸四边形上的可逆性与覆盖范围不能从单位方格结论直接外推。

### 4.3 QRP 与周期性

- QRP 的弱混沌/Hamiltonian 来源与“若干余弦波叠加”不是同一个层次的定义，需要从
  Zaslavsky 专著和 Yin et al. 2024 的公式向前追溯。
- 任意实方向的平面 Fourier 叠加通常不是单位环面周期函数。当前实现使用整数波矢以满足
  `G(u,0)=G(u,1)` 和 `G(0,v)=G(1,v)`；这是项目的周期安全改写。
- QRP、周期 tiling 和 Wang tiling 是三种不同结构来源，不能因为最终都能无缝显示就混称
  为 Wang Tile 结构。

## 5. 第一轮阅读顺序

1. Cohen et al. 2003：先建立经典 Wang Tile 的正确工程语义。
2. Lagae & Dutré 2006：补齐角点和跨边结构约束。
3. Yin et al. 2024：还原 QRP 与不变映射的真实定义。
4. Wang et al. 2025：核对 Coons 四边形映射，并与本地草稿逐式比较。
5. Wei 2004：补充 GPU 过滤与缩放问题。
6. Fu & Leung 2005：只在进入任意四边形/表面阶段前深入阅读。

完成前四项后，再决定胞元风格是改写为由边标签约束的 tile 内容，还是保留为连续世界场
对照组。此决定不应只根据画面是否美观作出。

第一轮精读与实现审计已经完成，详细结论见
[主论文与当前实现审计](./paper-implementation-audit.md)。当前决策是移除此前新增的预设
6–8 及其专用路径，保留预设 1–5 与主论文核心管线。下一轮新风格将从
“边标签—Coons 逆映射—周期生成器”内部重新设计，并以 Wang 因果消融、接缝验证和
CPU/GPU 一致性作为进入正式预设的条件。
