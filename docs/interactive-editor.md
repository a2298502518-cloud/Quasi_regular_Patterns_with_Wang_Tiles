# 交互编辑器与提交语义

实时程序的参数面板不是直接修改 GPU 资源的控制台。它只编辑 `PatternProject` 中的
draft；按下 **Apply validated draft** 后，项目层才执行完整验证并生成候选场景。只有
全部步骤成功，candidate 才会替换 committed 场景，revision 加一，渲染器上传一次新
快照。

```text
ImGui controls
  -> draft PatternConfiguration
  -> Apply intent
  -> validate dimensions / shader capacities / finite values
  -> validate every edge-color combination
  -> build candidate palette, grid and generator
  -> atomic committed swap + revision increment
  -> one GPU scene upload
```

验证失败时，draft 会保留供继续调整，committed 参数、revision、Wang 网格和当前画面
均不改变。**Discard** 会把 draft 恢复为 committed。这个约束由不依赖 OpenGL 或 ImGui
的项目层测试覆盖。

## 可编辑内容

- Wang 网格宽高、每瓦片像素数和确定性种子；
- 五组默认边函数的 `epsilon` / `delta`；
- Fourier、周期噪声、瓦片内部变化和世界空间调制权重；
- 色带中心、对比度、带状参数、色标位置和 sRGB 颜色；
- 抗锯齿轮廓频率、强度、宽度和边界安全浮雕强度；
- Pattern、Jacobian、Newton residual 与 Tile edges 调试视图。

颜色选择器以 sRGB 显示，项目配置和 Shader 上传仍保存线性 RGB；色标之间转换到 Oklab
平滑插值，再回到线性 RGB 合成。导数驱动的轮廓和浮雕在 Wang 边界附近平滑归零，避免
把论文未保证连续的跨边法线当成可靠数据。调试视图和相机属于轻量查看状态，不触发
Wang 网格重建。

## 操作方式

- 面板中的预设按钮：载入 draft，等待 Apply；
- 数字键 `1`–`5`：直接载入并提交已知安全的基准预设；
- 鼠标左键拖动、滚轮：平移和以光标为中心缩放；
- `D`：循环调试视图；`R`：重置相机；`Esc`：退出。

界面捕获鼠标或键盘时，相机与快捷键回调不会同时响应。

## 自动化视觉检查

普通 GPU 截图与 CPU/GPU 校验保持无 UI 的权威路径。另有一个离屏编辑器截图入口，用于
检查面板布局而不要求自动化环境显示窗口：

```powershell
.\build\Release\qrp_realtime.exe `
  --preset 3 --size 1100 800 `
  --capture-ui output\gpu\editor.ppm
```

生成内容位于被 Git 忽略的 `output/`，不会污染源码提交。
