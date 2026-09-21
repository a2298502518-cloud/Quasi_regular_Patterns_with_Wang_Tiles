# 实验报告生成工具

此目录保存当前《基于 QRP 与 Wangtile 的准规则纹样生成实验报告》的可复现源文件。

## 前置条件

- Windows 和 Microsoft Office 16；脚本使用
  `C:\Program Files\Microsoft Office\root\Office16\MML2OMML.XSL`。
- 系统字体 `Microsoft YaHei`、`Microsoft YaHei Bold` 和 `Cambria Math`。
- Node.js、pnpm，以及 `requirements.txt` 中的 Python 依赖。
- 已生成 `output/wang-qrp-experiment/A_coarse_parameter_family.png`。可先从项目根目录运行：

  ```powershell
  cmake --build build --config Release --target qrp_wang_qrp_experiment
  .\build\Release\qrp_wang_qrp_experiment.exe output\wang-qrp-experiment
  ```

## 生成步骤

在本目录运行：

```powershell
python -m pip install -r requirements.txt
pnpm install --frozen-lockfile
node render_equations.js
python build_report.py
```

`render_equations.js` 根据内置 LaTeX 公式更新 `equations/*.mml`，并生成用于核对的
`equations/manifest.json`。`build_report.py` 会生成临时流程图 `method-flow.png`，最终报告写入
`reports/基于QRP与Wangtile的准规则纹样生成实验报告.docx`。

仓库中保留 23 个 MML 文件作为当前报告的精确公式基线；manifest、流程图、依赖目录和缓存均为可再生成文件。
