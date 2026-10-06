# QRP 与 Wang tiles 的准规则纹样生成

本项目现在只维护**保留局部 QRP 母题的有限 Wang 相容生成**，即已获老师认可方向的 relational 直接公式路线。
从参数生成相位场、有限角色与合法铺砌，不以图像拟合建库为前置步骤。

[论文核心](docs/qrp-bounded-connection-paper-core.md) · [推导与证据](docs/qrp-structured-atlas-study.md) ·
[文献研究](docs/qrp-ordered-content-research.md) · [原生迁移记录](docs/native-migration-2026-10-06.md)

## C++ / OpenGL 本地操作台

正式入口是 C++20 数学核心与 GLFW / ImGui / OpenGL 3.3 操作台，**运行不调用 Python**。
本机已有 Visual Studio C++ 工具链与 CMake，项目根目录运行：

```powershell
./tools/native.ps1 -Action run
```

编辑 Q、方向函数族、频率、源偏移和保护区后，点击“应用参数 / 生成”。草稿不改变已显示结果。
可切换同尺度纯 QRP 对照、显示单位 tile 网格、缩放/拖动画布、调整高度阈值及颜色。
Wang 构造参数不作为风格控制；布局种子和全同状态只放在单独的铺砌对照面板。
默认预览 24×24 单位、48px/单位，即 1152×1152；默认高清 192px/单位，即 4608×4608。
预览和高清均从同一 C++ 公式及采样配色求值，高清不是放大预览。
每次 GUI 导出新建 `output/native-workbench/<时间戳>/`，包含纹样、纯源和参数，不覆盖论文交付。

无需界面也可用 C++ 生成：

```powershell
./tools/native.ps1 -Action build
./build/Release/qrp_generate.exe --q 5.5 --extent 24 --pixels 64 --output output/my-native-qrp
# Q1 等源可能不支持自动峰探针，可显式给出合法源区域
./build/Release/qrp_generate.exe --q 1 --core-radius 6 --output output/q1-native
```

新机器需要 CMake≥3.25 和支持 C++20 的 Visual Studio C++ 工具链。首次构建通过 FetchContent 下载锁定的第三方源码；
本机可使用 `.codex-temp/native-deps/` 缓存，它只含第三方依赖，不依赖删除区或旧算法。
目前场求值在 C++ CPU 中执行，OpenGL 负责纹理显示及 UI；没有冒称已迁入 GPU Shader。

## 在 VSCode 使用

通过“终端 → 运行任务”选择 **QRP: run native workbench**。
F5 选择 **QRP C++ / OpenGL 操作台**；需要 C/C++ 扩展，启动前自动构建 Debug。
Ctrl+Shift+B 的默认任务为原生 Debug 构建。Python 配置已明确标为数值参考，不是操作台后台。

## 当前构造的范围

同一算法覆盖所声明线性相位源的任意有限 Q≥1，包括非整数；目前维护 basic 与 cubic-directions。
有限角点状态、共同整数提升和自动连接间距共同定义内容，指定核心原样保留，合法边标签保证标量场 C² 接边。
完整单位目录为 16WH 个类型，每组参数下有限且不随画幅扩张；W、H 可随参数变化。

保护局部母题不等于保持整幅 QRP 的原始频谱、全局对称或全部轮廓拓扑。
合法布局中包括周期对照，不称为强非周期 tile set；连续性和变化量也不能代替美感评价。
本轮迁移保持当前路线公式不变，没有恢复旧算法或重新挑选样图。
数学参数域不等于无限工程资源：当前原生数值矩阵预算为 2048 项、图像单边预算为 16384px，并限制尺寸枚举预算。
这些是明确报错的资源约束，不是逐 Q 配方，也不保证全部参数可以实时调节。

## 已有结果

| 本地目录 | 内容 |
| --- | --- |
| [高清交付](output/QRP_Wang_HD_2026-09-30/00_使用说明.txt) | 10 个 Q，30 张独立 4608×4608 PNG，参数与校验值保留 |
| [跨 Q 图库](output/qrp-motif-connection-q-gallery/README.md) | Q5、5.5、6、8、9、10、11、13 的同尺度对照 |
| `output/qrp-motif-connection-relational/` | Q4.8 三次方向、Q6、Q7；复用、一般 Q 与相对支消融证据 |
| `output/qrp-motif-connection-new-parameters/` | Q8、Q12 参数结果 |
| 其他保留的 atlas / motif 输出 | 必要历史机制对照，只留证据，不继续维护其旧生成代码 |

这些本地交付与生成物不进入公开 Git 仓库。

## 必要检查和历史资料

```powershell
./tools/native.ps1 -Action test
```

检查当前完整目录边迹、保护核心、梯度、解析界及扩幅；详见[验证记录](docs/validation.md)。
`tools/` 中保留的 Python 数值参考仅用于迁移对照及论文辅助，不是长期并行维护的另一条产品算法。
其运行方式和对照命令见[工具说明](tools/README.md)。
旧 C++、旧桌面、相位拟合、图册实验和旧测试仍在 `删除区_2026-10-06/`，不再是运行依赖。
历史研究文档、Word 报告和本地配方备份仍在[历史资料](docs/history/README.md)。
未永久删除旧代码；清空删除区后，其中未提交过的材料不能从该备份恢复。

[文档索引](docs/README.md) · [恢复说明](docs/legacy-experiments.md)
