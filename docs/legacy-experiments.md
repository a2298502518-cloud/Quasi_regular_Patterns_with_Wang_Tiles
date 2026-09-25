# 历史实验复现

这些路径保留研究证据，不属于默认实验入口。旧报告中的原程序名统一改用
`qrp_legacy_experiment`，图像参数和文件名不变。

```powershell
cmake -S . -B build-legacy -DQRP_BUILD_LEGACY_EXPERIMENTS=ON -DQRP_BUILD_TESTS=OFF
cmake --build build-legacy --config Release --target qrp_legacy_experiment
```

调试台仍使用旧的全局造型模型，没有接入独立相位瓦片。只修正其默认可执行文件路径，
不继续 UI 开发。历史报告也没有同步为当前方法报告。

## 运行实验

```powershell
cmake --build build-legacy --config Release --target qrp_legacy_experiment
.\build-legacy\Release\qrp_legacy_experiment.exe output\wang-qrp-experiment
```

实验固定使用 `20 × 20` 网格、每瓦片 `80 × 80` 像素、五种边标签和固定随机种子。
输出目录包含七张图：

- `A_coarse_parameter_family.png`
- `B_layered_parameter_family.png`
- `C_uniform_vs_wang.png`
- `P0_zero_phase.png`
- `P1_axis_phase.png`
- `P2_oblique_phase.png`
- `P3_opposed_phase.png`

历史七图实验报告为
`reports/基于QRP与Wangtile的准规则纹样生成实验报告.docx`，生成说明见
`tools/report/README.md`。该报告尚未同步独立瓦片研究及方法定位修正。

## QRP 参数探索

同一个实验程序提供 `--style-study` 模式；默认的七图实验保持不变。

```powershell
.\build-legacy\Release\qrp_legacy_experiment.exe --style-study output\qrp-style-study
python tools\build_style_board.py output\qrp-style-study
```

排版脚本需要 Pillow（`python -m pip install Pillow`），默认使用 Windows 微软雅黑；
其他平台通过 `--font <支持中文的字体路径>` 指定字体。

输出 16 组 `640 × 640` 小样，每组包含粗场灰度、分层灰度和固定色带结果；
`manifest.json` 记录参数与场值统计，排版脚本生成四张对照板。
所有 Wang 配置固定，只改变 QRP 相位、方向数、粗场频率或层级强度。
观察结论、限制和下一步建议见 [当轮探索记录](qrp-style-study.md)。

方向扩展使用同一入口与排版脚本：

```powershell
.\build-legacy\Release\qrp_legacy_experiment.exe --direction-study
python tools\build_style_board.py output\qrp-direction-study
```

## 本地试调台

仅作辅助观察。代码已切换至 A 路线候选，当轮交互复核尚未完成；按用户要求暂停界面工作，
不将下面的功能清单作为当前研究成果的评价标准。

构建完成后运行（Python 3，无额外依赖）：

```powershell
python tools\workbench\server.py
```

浏览器访问 `http://127.0.0.1:8766`，结束时在启动终端按 `Ctrl+C`。
端口被占用时使用 `--port 8767`；非默认构建位置可通过 `--executable <程序路径>` 指定。

- 四个候选为几何花簇、疏朗单元、细线花簇、斜向线描，尚未确认为成熟风格。
- 切换纹样、QRP 场或形状蒙版；固定当前图后可与新结果对比。
- 保存的是当前已应用图片的参数 JSON；导入后先进入草稿，应用后才改变图片。
- 预览为 640px，“生成 1600px”按同一参数重新采样，再下载当前视图的 PNG。
- 全部生成物集中在 `output/qrp-workbench/`，缓存键包含所有参数、分辨率与可执行文件哈希。
- 服务只监听本机，Wang 参数不出现在 UI 或用户参数文件中；当前配色也固定。

当前模型标识为 `direct-qrp-design-v1`；旧方向模型配方不自动转换。
历史方向控制及旧 `--render` 参数语义见 [方向性探索记录](qrp-direction-study.md)。

## 局部秩序对照

```powershell
.\build-legacy\Release\qrp_legacy_experiment.exe --order-study
python tools\build_style_board.py output\qrp-order-study
```

输出 21 组研究小样及参数记录，排版包含逐层简化、保守路线和规则骨架对照。
高分辨率复现与公式边界见 [局部秩序研究](qrp-order-study.md)；旧七图保持不变。

## A 路线造型研究

```powershell
.\build-legacy\Release\qrp_legacy_experiment.exe --design-study
python tools\build_style_board.py output\qrp-design-study
```

输出 24 组对照；优先看同场不同区域的 `board_regions.png`。
方法、观察、未解决问题与验证边界见 [A 路线研究](qrp-contour-study.md)。

相位诊断使用 `--phase-study`，输出到 `output/qrp-phase-study`，再用相同排版脚本生成
`board_phase.png`。它保留同族变体的比较，不单凭此图断定混合造型需要修复。

## 造型控制研究

```powershell
.\build-legacy\Release\qrp_legacy_experiment.exe --control-study
python tools\build_style_board.py output\qrp-control-study
```

输出 12 组原图，`board_control.png` 比较方向强度与相位变体，`board_axes.png` 比较旋转和混合。
复用现有 QRP 模型，不新增用户参数或界面；控制语义、证据与限制见
[造型控制研究](qrp-control-study.md)。

## 风格跨度研究（全局内容基准）

```powershell
.\build-legacy\Release\qrp_legacy_experiment.exe --diversity-study output\qrp-diversity-study 32
python tools\build_style_board.py output\qrp-diversity-study
# 相同配置的 1600px 输出
.\build-legacy\Release\qrp_legacy_experiment.exe --diversity-study output\qrp-diversity-study\high-resolution 80
```

输出 13 组，每组包含成图、组合场灰度和蒙版。`board_selected.png` 比较四类代表候选；
`board_diversity.png` 保留第一轮结果，`board_nesting.png` 展示父子来源，
`board_grouping.png` 展示第二轮层级组织。固定双色，不以换色或相位变体充当新增风格。
联合能量候选未呈现预期的细胞结构；有效结果与限制见 [实验记录](qrp-diversity-study.md)。
当轮不接入试调台，也不新增用户可操作的 Wang 参数。

## 来源混合瓦片（历史候选）

```powershell
.\build-legacy\Release\qrp_legacy_experiment.exe --tile-study output\qrp-tile-study 96
python tools\build_tile_board.py output\qrp-tile-study
```

三类 QRP 配置各比较固定来源和边界匹配来源；每例预生成 16 张瓦片，复用到两种合法布局。
共 111 张原图，排版脚本另生成 `board_comparison.png` 和 `board_reuse.png`，并核验
全部 768 个实例与源瓦片的 RGB 完全一致。输出目录按既有规则忽略，不提交生成物。

当轮评价：独立复用与 C¹ 接边成立，但固定来源显出方格组织，匹配来源出现部分粘连和波带
断连。过渡区占瓦片面积 64%，不能称作小范围无损接边；下一步需研究造型与边界的联合约束。
推导、负面结果、限制及复现方法见 [独立瓦片研究](qrp-reusable-tile-study.md)。
