# Quasi-regular Patterns with Wang Tiles

目标是由 QRP 生成风格多样的纹样，由 Wang 瓦片承担真正的内容复用与合法铺砌。
Wang 是内部机制，不向用户开放参数；设计参数属于 QRP。调试台不是现阶段重点。

## 当前可运行基线与边界

1. `ParametricWangQrpField` 定义 QRP 模态、方向权重与相位。
2. `PhaseCompatibleQrpTiles` 在每个模态上满足边界相位兼容，生成位置无关的局部场。
3. `QrpChannelComposition` 组合兼容通道，得到环形、方向波带、格纹与父子组织。
4. `EndpointWangTiles` 独立定义 16 种端点编码类型；先烘焙内容库，再按合法布局纯 ID 复制。

相同边标签约束场值与完整一阶梯度，不依赖实例全局位置或事后接缝处理。
相位闭合改变原 QRP 的频谱并引入周期骨架，**不是 canonical QRP 的无损裁切**。
用户已接受方法可以扩展 QRP；这不表示当前闭合构造已经是最终论文方法，
仍须审视它是否支持形态控制和内容复用的共同目标。

当前保留源平移与父场相对相位两种组织候选。变化更多不等于更美观，
数值匹配不等于商业价值。尚未证明严格非周期性，也未完成 GPU、实时编辑器或通用导出。

理论、效果和负面结果见：
[相位兼容](docs/qrp-phase-compatible-tile-study.md)、
[父子组织](docs/qrp-organization-tile-study.md)、
[大范围重铺](docs/qrp-retiling-study.md)。
概念重审见 [QRP 理解](docs/qrp-foundations-review.md) 和 [Wang 理解](docs/wang-foundations-review.md)。
最新 [总场联合拟合探针](docs/qrp-joint-field-study.md) 对照共享边界拟合与逐模态闭合；
它是有明确局限的独立研究实验，不接入默认构建，也不代表已经解决形态保持问题。
下一步以实验结论判断是否需要改动内容、边界或类型关系，而不是继续扩建界面。

研究探针现默认使用三组新的设计样例：青金叠瓣（q=5）、靛蓝细描（q=8）、铜金镶边（q=12），
采用多高度分层或窄带线描。参数与源谱快照在 `tools/qrp_design_cases.json`，
仍共用原来的共享边界求解器；C++ 基线配方不因这次换样例改变。
运行 `python -X utf8 tools/joint_field_probe.py`，输出在 `output/qrp-design-tile-study/`，
依赖 NumPy、SciPy、Pillow。新样例、对照与验证范围见研究记录的第 10 节。

2026-09-25 的相位重审见 [第 11 节](docs/qrp-joint-field-study.md#11-相位控制重审与传递实验2026-09-25)：
旧二阶相位在偶数 q 上会退化为驻波幅度控制，q=8 存在全场抵消反例。
核心新增显式 `phaseHarmonicOrder`（默认仍为 2），本轮 q=8/12 的相位实验使用 3，方向权重与零相位图像不变。
12 组同条件实验支持相位变化能传递到可复用内容库，但不代表同一库的母题重复已解决；下一步研究库内局部组织覆盖。
结果在 `output/qrp-phase-response-study/`；默认设计样例和 C++ 历史配方未切换。

后续 [内容覆盖与类型分配研究](docs/qrp-content-coverage-study.md) 将同一 QRP 的 16 个不同区域组成目标库，
比较顺序分配与按同色边内容分配。后者在相同源集合上降低约 9%–21% 的拟合误差，库内重复减弱；
固定分配后再改变 QRP 相位也能保留响应。但边界附近仍有结构改动及少量场值越界，尚不替换默认路径。
输出在 `output/qrp-content-coverage-study/` 和 `output/qrp-content-coverage-phase-transfer/`，后者给出覆盖与相位控制的联合作用。

## 构建与运行

Windows / Visual Studio 2022：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure

# 基础相位兼容、父子组织、大范围重铺：同一入口，只选择配方
.\build\Release\qrp_wang_qrp_experiment.exe --study phase
.\build\Release\qrp_wang_qrp_experiment.exe --study organization
.\build\Release\qrp_wang_qrp_experiment.exe --study retiling
```

无参数只显示帮助，不再隐式运行历史七图实验。
三个旧别名 `--phase-tile-study`、`--organization-tile-study`、`--retiling-study`
继续可用，避免既有相位研究命令失效。语法为：

```text
--study phase|organization|retiling [输出目录] [每瓦片像素数]
```

默认输出分别为 `output/qrp-phase-tile-study`、`output/qrp-organization-tile-study`
和 `output/qrp-retiling-study`。前两项默认 96px/瓦片，重铺默认 384px/瓦片；
缩小验证分辨率会影响细节显示，不应据此评价纹样质量。

图板需要 Python 与 Pillow：

```powershell
python tools\build_tile_board.py output\qrp-retiling-study
```

图板读取 manifest 中的案例标题、分组和父子关系，不实现生成公式。
它核验铺砌实例与缓存瓦片像素一致；schema 1 的旧研究输出仍可读取。
每份清单读入后传给各图板，字体与配色由 `tools/board_common.py` 共用，
当前图板不再导入旧风格图板模块。
跨轮比较命令及其分辨率前提见各研究记录。

## 工程边界

| 部分 | 职责 |
| --- | --- |
| `qrp_wang_qrp_core` | QRP 谱定义、相位兼容局部场、通道组合 |
| `src/model/EndpointWangTiles.hpp` | 与内容算法无关的固定边标签目录 |
| `apps/TileStudyRecipes.*` | 案例配方、谱闭合跨度选择、运行窗口 |
| `apps/ReusableTileStudy.*` | 按配方构造兼容通道并运行 |
| `apps/TileStudyExport.*` | 统一拥有一轮输出的布局、目录和清单；共用烘焙与纯像素铺砌 |
| `qrp_raster` | 当前与历史路径共用的图像容器和双色覆盖混合 |
| `src/color/InkCoverage.*` | 共用双色覆盖混合；不合并不同几何渲染语义 |
| `apps/legacy/` | 旧全局场、规则载波、轮廓造型和来源混合的复现入口 |
| `qrp_research_baselines` | 历史模型，仅由对照测试或可选旧入口使用 |
| `tests/` | 保留七组必要数学与几何测试，不新增测试体系 |

`QRP_BUILD_LEGACY_EXPERIMENTS` 默认 OFF。旧模型仍有对照用途，没有删除，
但不进入当前实验程序的链接依赖；仅构建当前程序时无需它们。
`QRP_BUILD_TESTS=ON` 会编译测试需要的历史基线和 canonical oracle。
轮廓几何单独由 `qrp_contour_geometry` 供测试和历史入口使用；旧色带仅编入历史入口。
旧实验与冻结的试调台复现见 [历史实验](docs/legacy-experiments.md)。

每轮实验结束要明确：哪些候选进入当前配方，哪些仅保留证据，哪些退出默认路径。
不再通过不断添加永久 CLI 分支代表研究进展。

[验证说明](docs/validation.md) · [历史理论](docs/content-constrained-wang-model.md) ·
[文献地图](docs/literature-map.md) · [历史交接快照](docs/chat-handoff-2026-09-18.md)
