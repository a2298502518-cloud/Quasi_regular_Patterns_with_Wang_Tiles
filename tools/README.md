# 研究工具

Python 3.11+，依赖见 `requirements.txt`；建议使用独立 venv，避免与系统 NumPy / SciPy 混装。
图板默认用 Windows 微软雅黑；有图板的研究入口支持 `--font <字体路径>`。
QRP 基础阅读探针目前固定使用该字体。

## 入口与生命周期

| 入口 | 用途 | 默认输出 |
| --- | --- | --- |
| `joint_field_probe.py` | 已认可三组设计；总场拟合与逐模态闭合对照 | `output/qrp-design-tile-study` |
| `phase_response_study.py` | 三组设计 × 四组相位，包含旧相位抵消反例 | `output/qrp-phase-response-study` |
| `content_coverage_study.py` | 同区域基线、16 区域顺序分配、匹配分配 | `output/qrp-content-coverage-study` |
| 上一入口加 `--phase-transfer` | 固定零相位的分配，仅改变 QRP 相位 | `output/qrp-content-coverage-phase-transfer` |
| `build_tile_board.py <目录>` | C++ 基线 PNG 图板与像素复用核验 | 输入目录 |
| `qrp_foundations_probe.py` | 独立 QRP 文献公式核对 | `output/qrp-foundations` |
| `wang_foundations_probe.py` | Wang 组合核对与已有库重铺 | `output/wang-foundations` |

求解、显示和 Wang 拼接只在 `joint_field.py`、`wang_tiles.py` 中维护。
阅读探针不是共享算法模块；其他实验不再从阅读探针导入执行逻辑。
基础 QRP 阅读探针保留独立论文公式，不能用它替换 C++ 参数化源场。

## 输入与复现

`qrp_design_cases.json` 保存默认三组实际模态、固定显示、布局及 C++ oracle。
`joint_field_probe.py --source <C++ phase 输出目录>` 仍可复现旧零阈值诊断，
包括无约束拟合与乘积通道对照；旧输出不会被自动升级为新样例。

相位与覆盖实验通过构建出的 `qrp_export_sources` 获取实际源场，不依赖 `.codex-temp/`。
它导出 q=5/8/12、频率 2、跨度 16 的固定研究源：

```text
qrp_export_sources [phase_a phase_b [harmonic_order [offset_x offset_y]]]
```

没有 offset 时保持默认两顶点来源；指定 offset 后两个来源相同，得到真实 QRP 源块。
默认相位基阶数仍为 2；实验脚本显式为 q=8/12 选择 3。导出包含原始/闭合场值与梯度核对点。

覆盖实验中的源池只保存一次，类型只保存索引。每次运行重新求解，不拿不完整或旧算法缓存当新结果。
固定网格、尺度和类型数是研究配方，不是新增用户参数。

自定义输出时，零相位与相位迁移使用成对路径：

```powershell
python -X utf8 tools/content_coverage_study.py --output output/coverage
python -X utf8 tools/content_coverage_study.py --phase-transfer --zero-phase-output output/coverage --output output/coverage-phase
```

相位迁移输出另含 `phase_response.json`，比较同一分配的零相位与新相位。
结果是有限样本的场误差、边界与复用证据，不是风格数量、美观或严格非周期性的证明。
