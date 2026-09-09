# PNG 与参数导出

## 1. 权威路径

导出不是第二套离线渲染器。`GpuPatternRenderer::renderImage` 只创建离屏颜色附件，随后
调用实时预览使用的 `draw`。因此 Wang 网格、逆 Coons 求解、生成器、Oklab 色带、轮廓
和浮雕全部来自同一 committed 场景和同一 GLSL 程序。

编辑器中的未提交草稿不会进入导出。如果面板显示 `DRAFT CHANGED`，应先点击
`Apply validated draft`；`PNG export` 区域也会明确提示当前导出仍使用已提交参数。

## 2. 编辑器导出

在 `PNG export` 区域设置：

- `Path`：必须以 `.png` 结尾；父目录会自动创建。
- `Resolution scale`：1 到 16；输出宽高分别是网格尺寸乘以基础每瓦片像素数和倍率。
- `Export PNG + JSON`：显式触发高成本离屏绘制、GPU 读回和编码。

默认 10 x 10、每瓦片 72 像素、4 倍率输出 2880 x 2880。成功后同时生成同名 `.json`。

## 3. 命令行导出

按整数倍率导出完整网格：

```powershell
.\build\Release\qrp_realtime.exe `
  --preset 3 `
  --export output\exports\pattern.png `
  --export-scale 4
```

指定任意画布尺寸并自动居中铺满；纵横比不同时会等量裁切两侧或上下，不产生黑边：

```powershell
.\build\Release\qrp_realtime.exe `
  --preset 3 `
  --export output\exports\wide.png `
  --size 1920 1080
```

高级复现可用 `--camera originX originY pixelsPerTile` 显式覆盖导出相机。`--size` 与显式
`--export-scale` 互斥，避免分辨率语义含糊。

## 4. 文件语义

PNG 是 8-bit RGB 无损图像，包含 `sRGB` chunk 和与其匹配的 `gAMA=45455`。Shader 的
最终输出已经采用 sRGB 编码，写入标记是为了让支持色彩管理的查看器按正确颜色空间解释。

JSON 的 `schemaVersion` 当前为 1，记录：

- 项目名与 committed revision；
- 输出尺寸、材质开关和相机；
- Wang 网格、基础每瓦片像素数与无精度损失的十六进制 64 位种子；
- 全部边函数、生成器语义版本、Fourier modes、周期噪声种子与混合参数；
- Oklab 插值语义、tone、线性 RGB 与 sRGB 色标；
- 轮廓和浮雕材质参数。

生成图像与 JSON 位于被 Git 忽略的 `output/` 目录；它们是可再生结果，不进入源码提交。
