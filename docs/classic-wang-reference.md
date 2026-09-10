# 经典图像 Wang Tile 对照基线

本基线用于把经典的内容型 Wang tile 机制与主论文方法分开验证。它不是新风格预设，
也不替代《Quasi-regular Patterns with Wang Tiles》的主算法。

## 1. 当前实现的算法边界

数据流为：

```text
有限的 8 张图像 tile
  -> 四边签名 (South, North, West, East)
  -> 扫描线约束选片
  -> 直接栅格拼接
  -> PNG 与接缝指标
```

边标签数为 2。tile 集由下式独立枚举，而不是从参考工程复制常量：

\[
\mathcal S_8 = \{(s,n,w,e)\in\{0,1\}^4
\mid s\oplus n\oplus w\oplus e=0\}.
\]

因此总共有 8 个不同签名。对于扫描时已经确定的南边和西边标签，每种组合恰好有
2 个合法候选。铺砌器逐行、逐列枚举候选并用显式 seed 选择，不使用无上限拒绝采样，
也不附加“不能与左邻相同”等非 Wang 约束。当前画布使用开放外边界，不强制首尾环绕
匹配。atlas 在进入铺砌器前按边签名稳定排序，因此固定 seed 不依赖输入文件的枚举顺序。

本项目的逻辑坐标令 `y=0` 为底行，扫描从南向北进行；输出图像第 0 行仍是顶部。
渲染时显式翻转 tile 行，tile 图像中的 North 对应第 0 个像素行，South 对应最后一行。
代码提供按论文常见 `[N,E,S,W]` 顺序命名的构造入口，再显式映射到项目内部的
`South/North/West/East` 字段，避免后续接入 atlas builder 时依赖含混的数组下标。

## 2. 与主论文管线的关系

两条路径必须保持并列，而不能混合解释：

```text
经典对照：tile set -> 合法选片 -> image atlas lookup -> 拼接
主论文：Wang 边标签 -> 边函数 -> Coons 逆映射 -> 环面母纹 -> 着色
```

经典对照的可见内容预先属于有限 tile 集；主论文当前实现则根据边标签在线生成坐标
变形并采样公共母纹。建立该对照的目的，是明确判断视觉变化究竟来自有限内容 tile 的
组合，还是来自主论文的连续形变机制。

## 3. 当前图像内容

当前 8 张 tile 是项目内生成的诊断图：南北标签和东西标签分别使用两组确定颜色，角点
统一，内部仅作确定性插值。它们故意不追求纹样风格，只用来让四边签名、候选选择和
拼接关系可以直接观察。运行时没有 Coons warp、QRP 生成器、噪声、旋转、镜像或
per-tile 随机内容。

参考工程 `sashaouellet/WangTile` 没有明确许可证，因此本实现没有复制其源码、目录
结构、图像、手写 tile 表或经验常数。当前阶段吸收的是公开论文和参考工程共同展示的
算法思想：有限内容 tile 集、显式边码、受约束的扫描线选片以及 atlas 与铺砌器分离。

参考工程还包含“源图 patch -> overlap seam -> 旋转裁剪 -> 8 张纹理 tile”的 atlas
构造实验。该部分尚未进入当前基线；如果继续实现，应以 Cohen et al. 2003 与
Efros--Freeman 2001 为依据进行 clean-room 重写，并使用自有或明确授权的输入素材。

## 4. 运行与输出

```powershell
cmake --build build --config Release --target qrp_classic_wang_reference
build/Release/qrp_classic_wang_reference.exe output/classic-wang-reference
```

输出包括：

- `A_minimal_8_tile_set.png`：8 张诊断 tile；
- `B_classic_wang_tiling_10x10.png`：固定 seed 的 10 x 10 合法铺砌；
- `metrics.json`：tile 数量、候选数、签名覆盖和逐像素接缝指标。

## 5. 验证要求

- 8 个签名唯一且全部满足偶校验规则；
- 每个受约束边对恰有 2 个候选；
- 同一 seed 产生相同铺砌，不同 seed 仍产生合法铺砌；
- 1 x 1、1 x N、N x 1 和普通矩形网格均可生成；
- 缺失签名查询必须报错，禁止静默 fallback；
- 所有匹配边像素逐通道完全相同；
- 这组 tile 可产生随机的非周期外观，但不能称为数学意义上的 aperiodic tile set。
