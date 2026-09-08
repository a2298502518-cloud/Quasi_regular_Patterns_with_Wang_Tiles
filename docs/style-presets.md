# 预设风格矩阵

## 1. 为什么扩展结构维度

最初三套 10 x 10 展示预设分别使用金色、矿物色和极光色，但共享相近的局部变化、
连续世界扰动和等高线浮雕，因此属于同一个“有机矿物/地形”风格家族。它们可以证明
配色与混合权重的可调性，却不足以单独证明算法具有宽广的结构表达范围。

新增预设保留 Wang 邻接、逆 Coons 采样和同一渲染/导出路径，只扩展生成器的连续标量
组织方式。`ScalarProfile` 选择标量组织方式，`worldDetailAmplitude` 控制跨多个 tile 的
多方向世界场，`worldGrainAmplitude` 提供连续的全局细节。胞元风格同样直接从世界坐标
确定，因此不会在 tile 边界重新播种。它们都复用同一套几何、预览与导出路径。

## 2. 六套展示预设

| 快捷键 | 预设 | 主要结构 | 材质语言 |
| ---: | --- | --- | --- |
| 3 | `tiling_qrp_10x10` | Fourier 主导的有机流线 | 金色等高线与浮雕 |
| 4 | `tiling_mineral_10x10` | 噪声主导的矿物纹理 | 暖灰矿物层次与浮雕 |
| 5 | `tiling_aurora_10x10` | Fourier/噪声混合流场 | 高饱和冷色等高线 |
| 6 | `tiling_graphic_screenprint_10x10` | ridged profile 与大尺度世界带 | 四级原色套印、软阈值抗锯齿、无浮雕 |
| 7 | `tiling_ink_wash_10x10` | 低权重局部噪声叠加跨 tile 流动颗粒 | 低对比灰阶、无轮廓、无浮雕 |
| 8 | `tiling_cellular_camo_10x10` | Worley 胞元距离与确定性区域身份 | 多区域土绿迷彩、深色边界、无浮雕 |

前两套 1 x 1 与 2 x 2 预设仍用于局部和邻接诊断，不计入六套正式风格展示。

## 3. 可验证性

- CPU 与 GLSL 使用相同的 profile、世界颗粒、胞元哈希和软色阶公式。
- profile 测试覆盖 natural、ridges、cells，并在非零世界细节下检查共享边连续性。
- 新预设分别通过 CPU/GPU 中间量及最终 8-bit 图像对照。
- PNG 与 JSON 继续记录 committed 参数；JSON 的生成器模型版本为 `hybrid_torus_v3`，
  并显式保存 scalar profile、世界细节、世界颗粒、胞元尺度与色阶参数。
