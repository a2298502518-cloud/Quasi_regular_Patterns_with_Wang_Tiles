# 原生入口与 Python 数值参考

这里只有一条数学路线：relational 有限相位构造。旧算法和实验入口已归档。
正式核心已迁入 `src/qrp/Field.*`，统一采样、配色、PNG 导出在 `src/render/Raster.*`。
`apps/desktop/main.cpp` 是原生操作台，`apps/export_main.cpp` 是 C++ 无界面出图入口。
Python 文件保留为冻结数值参考、研究复核与已有交付的复现工具，正式程序不调用它们。
后续产品功能只在 C++ 中发展，必要时才更新参考对照，不长期同步两套产品实现。

## 原生入口

```powershell
./tools/native.ps1 -Action run
./tools/native.ps1 -Action build
./tools/native.ps1 -Action test
./build/Release/qrp_generate.exe --q 7 --extent 24 --pixels 64 --output output/my-native
```

运行和构建说明见[根 README](../README.md)。

## Python 参考模块职责

| 文件 | 职责 |
| --- | --- |
| `qrp_source.py` | 基础/三次方向源、值与梯度、圆分关系和整数相位格 |
| `qrp_motif.py` | 统一母题启发式探针与显式保护区域 |
| `wang_qrp.py` | 有界连接间距、有限角点相位、单位角色与布局 |
| `wang_tiles.py` | S/N/W/E 标签匹配和纯像素组装 |
| `qrp_render.py` | 固定配色、线性光超采样、源图、铺砌与展示图板 |
| `qrp_checks.py` | 当前路线的接边、核心、梯度、复用与相对整数支消融 |
| `motif_connection_study.py` | 唯一命令行编排入口，不再拥有公式或另一套显示路径 |
| `qrp_wang_defaults.json` | 3 个冻结代表输入与当前默认配色，不含旧模态库 |
| `run.ps1` | 选择本地 Python 环境并传递生成参数 |

## 运行 Python 参考（需单独的 NumPy / Pillow 环境）

```powershell
# 3 个冻结代表样例，输出到独立的 qrp-current-run 目录
./tools/run.ps1
# 一般合法 Q 共用构造；请给新的输出目录，不覆盖现有交付
./tools/run.ps1 --q 5 5.5 8 12 --extent 24 --pixels 64 --output output/my-qrp-wang
# 显式圆形源区域与三次方向族
./tools/run.ps1 --q 7 --model cubic-directions --frequency 12 --core-radius 6 --output output/my-explicit-region
# 对当前既有结果做完整复核，会写 verification.json 和相对支对照
./tools/run.ps1 --verify --output output/qrp-motif-connection-relational
```

也可直接执行：

```powershell
./.codex-temp/joint-field-venv/Scripts/python.exe -X utf8 tools/motif_connection_study.py --q 7 --extent 24 --pixels 64 --output output/my-qrp-wang
```

合法源域为有限 Q≥1、正频率，基础/三次方向源共用构造。
`--core-radius` 以源坐标计；未指定时使用统一探针，不承诺自动识别全部美观母题。
`--extent` 是单位 tile 的世界画幅，`--pixels` 是每单位像素数。
输出含纯源对照、两种布局、局部替换、扩幅、周期对照及参数/指标 JSON。
移除了 tensor、radial、旧 bounded 等分支；必要的相对支消融仍保留在当前复核入口中。

## Python 数据和依赖

Python 参考只需要 `requirements.txt` 中的 NumPy 与 Pillow；这不是 C++ 原生入口的运行依赖。
历史 source-export、Hermite/响应空间求解器、旧目录、图册/坐标试验、桌面后台与文献阅读探针不再是运行依赖。
源码阅读文献并未删除，只停止维护与当前方法无关的实验实现。

当前高清导出和图库拼图脚本已改用 `wang_qrp`、`qrp_render`，30 张交付 PNG 本身不改。
历史命令对应旧版本，不能混作当前入口；备份位置见[恢复说明](../docs/legacy-experiments.md)。

迁移对照（先构建 Release；不自动运行全量高清）：

```powershell
./.codex-temp/joint-field-venv/Scripts/python.exe -X utf8 tests/compare_native.py
# 额外只对照一组已有 Q7 的 4608×4608 交付图
./.codex-temp/joint-field-venv/Scripts/python.exe -X utf8 tests/compare_native.py --hd
```
