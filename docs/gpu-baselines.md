# GPU 基准与一致性记录

## 1. 实时路径

实时应用使用 OpenGL 4.3 core 和全屏三角形。片元 Shader 按像素中心计算世界坐标，
查找 Wang 单元，执行固定上限的逆 Coons Newton 求解，再调用与 CPU 相同定义的周期
Fourier、周期梯度噪声、瓦片内部变化和 Oklab 调色板。

CPU 仍是数学参考。GPU 只保存渲染资源和上传后的不可变场景快照；切换预设时更新
SSBO、噪声梯度纹理和 uniform，普通重绘、平移与缩放不会重建 Wang 网格。

## 2. 依赖

- GLFW 固定为 3.4，由 CMake `FetchContent` 获取。
- OpenGL loader 由 glad 2.0.8 针对 `gl:core=4.3`、零扩展生成并随仓库保存。
- Shader 保存在 `shaders/`，运行时编译；编译或链接失败会直接终止并报告日志。

纯 CPU 构建可使用：

```powershell
cmake -S . -B build-cpu -DQRP_BUILD_REALTIME=OFF
```

## 3. 运行与验证

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
.\build\Release\qrp_realtime.exe
cmake --build build --config Release --target qrp_gpu_validate
```

`qrp_gpu_validate` 创建隐藏窗口，渲染固定 10 x 10 QRP 预设并读取三组缓冲：最终
8-bit 图像、`uv/scalar/detJ`、`linear RGB/Newton residual`。CPU 对同一批像素独立
求值，避免只比较最终颜色而掩盖中间阶段偏差。

## 4. 阶段 C 结果

2026-09-07 在 Intel RaptorLake-S Mobile Graphics Controller、OpenGL 4.3、MSVC 19.44
Release 构建上记录：

| 基线 | 最大 uv 差 | 最大标量差 | 最大线性 RGB 差 | 最大 detJ 差 | 最大 GPU 残差 | 最大 8-bit 差 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `single_qrp` | `1.557e-6` | `1.047e-5` | `9.856e-5` | `1.088e-5` | `4.996e-7` | 1 |
| `neighbors_2x2` | `1.733e-6` | `3.318e-5` | `2.060e-4` | `1.233e-5` | `5.000e-7` | 1 |
| `tiling_qrp_10x10` | `1.993e-6` | `7.518e-5` | `7.084e-4` | `1.075e-5` | `5.000e-7` | 1 |
| `tiling_mineral_10x10` | `1.972e-6` | `9.341e-5` | `5.452e-4` | `9.281e-6` | `4.998e-7` | 1 |
| `tiling_aurora_10x10` | `1.963e-6` | `8.467e-5` | `6.117e-4` | `1.551e-5` | `5.000e-7` | 1 |

所有基线的 CPU/GPU 逆映射失败数均为 0，最终图像没有超过 1 LSB 的像素。同步执行
`glFinish` 的 720 x 720、120 帧本机测量平均为 4.137 ms/帧（约 241.7 FPS）；该值只
描述本次设备和驱动，不作为跨设备承诺。

视觉检查覆盖三套 10 x 10 图案、宽屏、竖屏、裁切后的相机位置，以及 Jacobian、
Newton 残差和真实瓦片边界覆盖层。没有发现失败像素、坐标翻转或接缝。

## 5. 阶段 D 视觉管线结果

阶段 D 将色标插值升级为 CPU/GLSL 一致的 Oklab，并增加 `fwidth` 抗锯齿轮廓和由标量
屏幕导数生成的轻量浮雕。导数效果乘以参数域边界窗口，在每条 Wang 边附近平滑归零；
基础线性颜色仍写入独立验证附件。GPU/CPU 校验绘制显式关闭显示材质，因此材质不能掩盖
逆映射、生成器或颜色插值误差。

平衡后的三套 10 x 10 预设把主要去重复变化移到共享世界坐标，降低 tile-local 权重。
2026-09-07 同一设备的结果为：

| 基线 | 最大标量差 | 最大线性 RGB 差 | 最大 GPU 残差 | 最大 8-bit 差 |
| --- | ---: | ---: | ---: | ---: |
| `tiling_qrp_10x10` | `9.961e-5` | `8.734e-4` | `4.999e-7` | 1 |
| `tiling_mineral_10x10` | `1.212e-4` | `7.679e-4` | `4.998e-7` | 1 |
| `tiling_aurora_10x10` | `1.030e-4` | `7.723e-4` | `4.999e-7` | 1 |

720 x 720、240 帧同步测量为 2.854 ms/帧（约 350.4 FPS）。视觉检查覆盖每瓦片 24、72
与 256 像素：没有发现明显接缝、方格边框或低缩放摩尔纹。性能数字只描述本次设备与
驱动。
