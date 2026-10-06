# 当前验证范围

当前只维护 relational 直接公式路线，正式核心位于 `src/qrp/Field.*`，原生入口为 `qrp_workbench` / `qrp_generate`。
`tools/wang_qrp.py` 与其 Python CLI 保留为冻结数值参考，不是原生程序后台。
旧桌面/C++/拟合/图册测试已随其代码归档，不再用旧测试数量证明当前方法。

理论与数值边界见[论文核心](qrp-bounded-connection-paper-core.md)和[推导记录](qrp-structured-atlas-study.md)。
原始证据保持原路径：

- [代表样例](../output/qrp-motif-connection-relational/results.json)
- [复用、独立求值与一般 Q](../output/qrp-motif-connection-relational/verification.json)
- [新参数样例](../output/qrp-motif-connection-new-parameters/results.json)

C² 和有限尺寸存在性来自推导，不靠截图或采样证明；数值一致也不等于审美保证。

## 当前原生必要检查

```powershell
./tools/native.ps1 -Action test
./build/Release/qrp_workbench.exe --smoke --capture output/native-migration-check/原生操作台.png
```

一个 CTest 入口覆盖三组输入的完整目录边迹、保护核心、梯度、解析界和布局扩幅。
OpenGL smoke 在隐藏窗口中实际运行公式生成、纹理上传、ImGui 绘制、截图及 PNG 保存，不调用 Python。
窗口缩放、拖拽、草稿编辑、Apply 和高清按钮的完整鼠标操作仍需人工验收；不把 smoke 冒称完整交互测试。

## C++ 迁移对照

```powershell
./.codex-temp/joint-field-venv/Scripts/python.exe -X utf8 tests/compare_native.py
# 额外只核对一组已有高清图，而非重新输出全部交付
./.codex-temp/joint-field-venv/Scripts/python.exe -X utf8 tests/compare_native.py --hd
```

8 组输入：Q4.8 三次方向、Q5.5/6/7/8/12 基础方向，以及 Q7 非零偏移的显式/自动区域。
选区、认证关系、整数支、W/H、解析界和两种布局与参考一致；24 张 288×288 PNG 的 RGB 像素数组一致。
采样场值/梯度最大差异约 1.64e−13；这是有限参数对照，不是对所有参数的位级等价证明。
Q7 三张 4608×4608 原生图与原高清交付的 RGB 像素数组一致，固定 RGB8、sRGB 标记及 300 DPI。
PNG 压缩及元数据形式可不同，不宣称文件 SHA-256 与旧导出相同。

迁移程序运行不依赖 Python；上面的 Python 环境只用于开发阶段的独立参考比较。
Release / Debug 构建、原生 CTest 和实际 OpenGL smoke 已执行；详细限制见[迁移记录](native-migration-2026-10-06.md)。

## Python 参考检查

```powershell
./.codex-temp/joint-field-venv/Scripts/python.exe -X utf8 tests/current_pipeline_tests.py -v
```

3 项检查：非整数/三次方向源梯度，当前场的接边和保护区，独立/缓存像素一致与固定尺度扩幅。
CLI 生成还会核对合法布局、梯度/关系、复用、局部修改和扩幅。
不为已退出路线继续维护测试，也不增加大量防御性参数扫描。

## 本轮代码整理回归

整理前对全部 11 组现有源参数保存 12×12、16px/单位的纯源与布局 A/B 像素摘要。
整理后，包括移走旧源码和 C++ build 之后，这 33 张代表图片的像素摘要完全一致。
Q4.8 三次方向、Q5.5、Q7、Q8、Q12 的自动探针及完整数值报告一致；30 张高清 PNG 的 SHA-256 未变。
3 项当前测试与一次实际 CLI 出图通过，高清导出脚本能够加载新模块。
不重生成整套高清图，不重新挑选参数，也不把工程重构称为算法改进。

完整范围见[代码整理记录](code-cleanup-2026-10-06.md)。
[2026-10-05 文件整理](file-cleanup-2026-10-05.md)、[2026-09-25 旧验证](history/validation-2026-09-25.md)均保留为当时事实，不作为现在的构建命令。
