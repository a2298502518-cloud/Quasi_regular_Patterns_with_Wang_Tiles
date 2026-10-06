# 历史研究资料

本目录保留过去的推导、实验结果解释和决策背景，不代表当前论文或操作台入口。
2026-10-05 只做文件整理，记录正文没有重写；文档移动后相对链接按原目标修正。
当前主线见[文档索引](../README.md)和[论文核心](../qrp-bounded-connection-paper-core.md)。

2026-10-06 又按用户要求归档旧桌面、C++ 和旧拟合/图册代码，只维护当前直接公式路线。
本目录中的“当前操作台/当前基线”等字样描述记录当时的版本，不代表现有运行状态。
配方副本属于本地生成数据，已从 Git 变更统计中排除，但文件仍保留。

## 按路线阅读

| 阶段 | 主要记录 |
| --- | --- |
| 最早全局调制、造型与风格探索 | [交接](chat-handoff-2026-09-18.md)、[全局模型](content-constrained-wang-model.md)、[秩序](qrp-order-study.md)、[轮廓](qrp-contour-study.md)、[控制](qrp-control-study.md)、[方向](qrp-direction-study.md)、[风格](qrp-style-study.md)、[多样性](qrp-diversity-study.md) |
| 固定内容与相位兼容 | [可复用瓦片](qrp-reusable-tile-study.md)、[相位闭合](qrp-phase-compatible-tile-study.md)、[组织](qrp-organization-tile-study.md)、[重铺](qrp-retiling-study.md)、[联合场](qrp-joint-field-study.md)、[边界结构](qrp-boundary-structure-study.md) |
| 旧有限库与内容控制 | [覆盖](qrp-content-coverage-study.md)、[源接口](qrp-source-interface-study.md)、[目录](qrp-content-catalogue-study.md)、[局部上下文](qrp-local-context-study.md)、[三组风格](qrp-local-context-styles.md)、[几何控制](qrp-geometry-control-study.md)、[相位推进](qrp-phase-transport-study.md)、[阶段目标](qrp-stage-study.md) |
| 一般 Q、响应与旧桌面 | [一般 Q](qrp-general-q-study.md)、[形态](qrp-morphology-study.md)、[参数响应](qrp-parameter-response-study.md)、[组织保持](qrp-structure-retention-study.md)、[旧论文基线](qrp-method-and-paper-scope.md)、[参数管线](qrp-parametric-pipeline-study.md) |
| 直接公式与累积状态 | [共同坐标](qrp-common-coordinate-study.md)、[状态相容](qrp-wang-state-compatibility.md)、[内部相位](qrp-internal-phase-study.md)、[可继续性](qrp-continuation-study.md) |
| 旧验证与书目地图 | [历史验证](validation-history.md)、[2026-09-25 验证](validation-2026-09-25.md)、[历史文献地图](literature-map.md) |

## 报告与参数

- [原始 Word 实验报告](reports/基于QRP与Wangtile的准规则纹样生成实验报告.docx)：旧全局调制方法，原文件保留。
- [旧报告生成器恢复说明](report-generator/README.md)：生成器源码已在较早 Git 提交归档。
- [桌面缓存配方索引](workbench-recipes/index.json)：81 份原始 recipe.json 的独立副本，文件名为原缓存 key；校验值和原路径在索引中。保留参数不意味着旧图片仍在工作输出目录，导入旧版本时按操作台迁移提示处理。

## 旧代码与输出在哪里

旧路线独立脚本、旧输出和临时阅读截图移到根目录 `删除区_2026-10-05/待删除/`，内部保留原相对路径。
本目录中的旧命令和反引号输出路径是当时的实验记录，不是当前可直接运行入口。
指向已隔离输出/源码的 Markdown 链接不再作为活链接保留，必要时按原路径从删除区恢复。
删除区可以整目录删除；删除后未提交过的脚本和旧输出不可从该备份恢复，本文档、报告和配方仍保留。
更早 Git 源码另见[恢复说明](../legacy-experiments.md)。
