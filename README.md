# Quasi-regular Patterns with Wang Tiles

研究目标：把 QRP 的形态生成与 Wang 的有限内容复用结合，生成风格多样、可调的纹样。
QRP 参数属于设计层；Wang 是内部铺砌机制，不向用户暴露参数。现阶段不开发调试台。

## 当前研究与对照

- **QRP 源场**：`ParametricQrpField` 唯一拥有方向、幅度、相位与解析梯度；不依赖 Wang 网格。
- **相位兼容对照（C++）**：`PhaseCompatibleQrpTiles` 逐模态闭合，`QrpChannelComposition` 组合通道。
  可生成固定库并合法重铺，但改变原始频谱、引入周期骨架，不是无损 QRP 裁切。
- **联合拟合候选（Python）**：共享边界自由度的 Hermite H¹ 拟合；进一步比较同源局部形变、
  不同源区域覆盖及源块—类型分配。仍有边界结构改变和少量场值越界，尚不是最终论文方法。
- **Wang 目录与复用**：16 种二值端点编码类型，先生成内容库，再按合法布局直接复制瓦片像素。
  同边标签约束实际场值及一阶梯度；不使用实例全局位置重算内容，不事后补缝。

三组已认可测试样例保持不变：青金叠瓣（q=5）、靛蓝细描（q=8）、铜金镶边（q=12）。
相位研究中 q=5 用二阶相位基，q=8/12 用三阶；旧二阶仍保留明确的对照语义，不能混淆为同一种控制。
`tools/qrp_design_cases.json` 是实际源谱和显示配方的冻结快照，单独修改其中的 q 等元数据不会重建模态。

最新结论见 [内容覆盖与类型分配](docs/qrp-content-coverage-study.md)；
下一步仍是研究共享接口附近的 QRP 结构代价，不把低误差、低重复或 C¹ 连续直接当成美观或商业价值。
没有证明严格非周期性、任意参数下的拓扑保持或主观审美优越。

## 构建与运行

Windows / Visual Studio 2022：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

当前 C++ 对照入口只保留一种命令形式，无参数显示帮助：

```powershell
.\build\Release\qrp_wang_qrp_experiment.exe --study phase
.\build\Release\qrp_wang_qrp_experiment.exe --study organization
.\build\Release\qrp_wang_qrp_experiment.exe --study retiling
# 可追加 输出目录 和 每瓦片像素数
python tools/build_tile_board.py output/qrp-retiling-study
```

默认输出在 `output/qrp-<study>-tile-study`（phase / organization）
或 `output/qrp-retiling-study`；前两者默认 96px/瓦片，重铺默认 384px/瓦片。
低分辨率只用于行为回归，不用于评价细节质量。三个旧 CLI 别名已移除。

研究实验：

```powershell
python -m venv .venv
.\.venv\Scripts\python.exe -m pip install -r tools/requirements.txt
.\.venv\Scripts\python.exe -X utf8 tools/joint_field_probe.py
.\.venv\Scripts\python.exe -X utf8 tools/phase_response_study.py
.\.venv\Scripts\python.exe -X utf8 tools/content_coverage_study.py
.\.venv\Scripts\python.exe -X utf8 tools/content_coverage_study.py --phase-transfer
```

覆盖实验不再依赖旧图片或系数缓存；相位迁移需先完成零相位覆盖实验，以冻结其类型分配。
C++ 源谱导出工具 `qrp_export_sources` 随默认构建生成；两个研究脚本支持 `--exporter` 指定其他构建位置。
所有生成物写入忽略的 `output/`。具体输入、输出、命令与限制见 [研究工具](tools/README.md)。

## 代码边界

| 位置 | 唯一职责 |
| --- | --- |
| `src/model/ParametricQrpField.*` | QRP 谱与解析求值 |
| `src/model/EndpointWangTiles.hpp` | 类型 ID 与边标签 |
| `src/model/PhaseCompatibleQrpTiles.*` | 逐模态闭合对照 |
| `apps/TileStudyRecipes.*` | C++ 对照配方 |
| `apps/TileStudyExport.*` | 烘焙、布局、像素复用与清单 |
| `apps/QrpSourceExport.*` | C++ 与 Python 共用的源谱序列化 |
| `tools/joint_field.py` | 源谱解释、共享边界求解、采样与固定显示 |
| `tools/wang_tiles.py` | Python 端点目录、合法布局与纯像素拼接 |
| `tools/*_study.py`、`joint_field_probe.py` | 固定实验的编排与证据输出 |
| `tests/` | canonical 参考、QRP 核心、相位兼容瓦片三组必要测试 |

旧全局模型、轮廓造型、来源混合、试调台及报告生成器已移出工作目录，
完整保存在 Git 提交 `b58337a`，见 [历史恢复说明](docs/legacy-experiments.md)。
历史图片、研究记录和 Word 报告保留，不将它们当成当前成果。

[当前验证](docs/validation.md) · [QRP 理解](docs/qrp-foundations-review.md) ·
[Wang 理解](docs/wang-foundations-review.md) · [联合拟合](docs/qrp-joint-field-study.md) ·
[相位兼容对照](docs/qrp-phase-compatible-tile-study.md) · [文献地图](docs/literature-map.md)
