# 历史实验与恢复

2026-09-25 清理后，旧全局 Wang 调制、规则载波、轮廓造型、来源混合、试调台及旧报告生成器不再留在工作目录。
它们的完整源码、配方、当时文档已保存在提交 **b58337a**；删除前 Release 构建及七组 CTest 均通过。

当前仍保留 QRP 参考公式、相位兼容基线，以及正在研究的共享边界联合拟合。
历史研究文档、已生成图片和 Word 报告没有删除；它们记录阶段性结论，不代表当前方法。

如需复现旧实验，可另开工作目录，不把旧实现重新混回主线：

```powershell
git worktree add --detach ../qrp-legacy-b58337a b58337a
```

在该历史工作目录中，按其 README 和本文件的历史版本执行；旧入口需启用
`QRP_BUILD_LEGACY_EXPERIMENTS`。旧报告生成步骤在该提交的 `tools/report/README.md`。

历史记录索引：

- [全局场理论](content-constrained-wang-model.md)、[旧验证记录](validation-history.md)
- [方向控制](qrp-direction-study.md)、[局部秩序](qrp-order-study.md)
- [轮廓造型](qrp-contour-study.md)、[造型控制](qrp-control-study.md)、[风格跨度](qrp-diversity-study.md)
- [来源混合瓦片](qrp-reusable-tile-study.md)

上述记录中的旧命令对应历史版本。当前运行入口以根目录 README 为准。
