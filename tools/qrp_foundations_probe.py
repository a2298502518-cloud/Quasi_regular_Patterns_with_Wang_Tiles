"""QRP 阅读报告的独立公式探针；不是项目生成器，也不接入构建或测试套件。"""

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
PAPER = (247, 242, 231)
INK = (30, 62, 74)
FONT = "C:/Windows/Fonts/msyh.ttc"


def field(q, x, y, phases=None):
    """Yin 公开稿 Eq. (1)：i 从 1 开始；实数 q 的项数为 floor(q)。"""
    count = int(np.floor(q))
    result = np.zeros(np.broadcast_shapes(np.shape(x), np.shape(y)))
    for i in range(1, count + 1):
        angle = 2 * np.pi * i / q
        phase = 0.0 if phases is None else phases[i - 1]
        result += np.cos(x * np.cos(angle) + y * np.sin(angle) + phase)
    return result


def formula_probes():
    # 固定二维网格只核对推导，不将有限采样当作周期性或美观性的证明。
    axis = np.linspace(-8 * np.pi, 8 * np.pi, 121)
    x, y = np.meshgrid(axis, axis)
    q5 = field(5, x, y) / 5
    angle = np.pi / 5
    theta = 2 * np.pi * np.arange(1, 6) / 5
    shift = (0.71, -1.13)
    phases = shift[0] * np.cos(theta) + shift[1] * np.sin(theta)
    basis = np.column_stack((np.cos(theta), np.sin(theta), np.ones(5),
                             np.cos(2 * theta), np.sin(2 * theta)))
    result = {
        "grid": {"size": 121, "extent": "[-8*pi, 8*pi]^2", "dtype": "float64"},
        "q5_q10_normalized_max_error": float(np.max(np.abs(q5 - field(10, x, y) / 10))),
        "q3_q6_normalized_max_error": float(np.max(np.abs(field(3, x, y) / 3 - field(6, x, y) / 6))),
        "q5_rotate_36_degrees_max_error": float(np.max(np.abs(q5 - field(
            5, x * np.cos(angle) - y * np.sin(angle),
            x * np.sin(angle) + y * np.cos(angle)) / 5))),
        "q4_closed_form_max_error": float(np.max(np.abs(
            field(4, x, y) - 2 * np.cos(x) - 2 * np.cos(y)))),
        "translation_phase_identity_max_error": float(np.max(np.abs(
            field(5, x + shift[0], y + shift[1]) - field(5, x, y, phases)))),
        "q5_phase_basis_rank": int(np.linalg.matrix_rank(basis)),
        "real_q_origin_left_limit_probe": {
            "q": 4.999999, "H_left": float(field(4.999999, 0.0, 0.0)),
            "H_at_5": float(field(5, 0.0, 0.0)),
        },
        "yin_preprint_eq15_sigma_at_boundary": float(1 / (1 + np.exp(5))),
        # 项目闭合公式的单模态反例：空间连续不等于参数连续。
        "closure_rounding_midpoint_values": [
            float(np.cos(np.pi * np.round(wave / (2 * np.pi))))
            for wave in (np.pi - 1e-6, np.pi + 1e-6)
        ],
    }
    return result


def ink_image(mask, size):
    rgb = np.where(mask[..., None], np.array(INK), np.array(PAPER)).astype(np.uint8)
    return Image.fromarray(rgb).resize((size, size), Image.Resampling.LANCZOS)


def board(title, subtitle, panels, columns, footer, destination):
    size, margin, gap, top, cell_height = 500, 28, 24, 126, 588
    rows = (len(panels) + columns - 1) // columns
    width = margin * 2 + columns * size + (columns - 1) * gap
    canvas = Image.new("RGB", (width, top + rows * cell_height + 56), "#ffffff")
    draw = ImageDraw.Draw(canvas)
    title_font = ImageFont.truetype(FONT, 34)
    label_font = ImageFont.truetype(FONT, 23)
    small_font = ImageFont.truetype(FONT, 19)
    draw.text((margin, 20), title, font=title_font, fill=INK)
    draw.text((margin, 75), subtitle, font=small_font, fill="#52646c")
    for index, (label, note, mask) in enumerate(panels):
        left = margin + (index % columns) * (size + gap)
        upper = top + (index // columns) * cell_height
        draw.text((left, upper), label, font=label_font, fill=INK)
        canvas.paste(ink_image(mask, size), (left, upper + 42))
        draw.text((left, upper + 550), note, font=small_font, fill="#52646c")
    draw.text((margin, top + rows * cell_height + 8), footer, font=small_font, fill="#52646c")
    canvas.save(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "output/qrp-foundations")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    # 两倍采样后缩小，只缓解显示混叠；不声称是精确面积覆盖率。
    axis = (np.arange(1000) + 0.5) / 1000 * (12 * np.pi) - 6 * np.pi
    x, y = np.meshgrid(axis, -axis)
    results = formula_probes()
    panels = []
    for q, note in [(3, "三角周期结构"), (4, "方格周期结构"), (5, "十重旋转对称的基本场"),
                    (6, "归一化后与 q = 3 相同"), (8, "八重旋转对称的基本场"),
                    (10, "归一化后与 q = 5 相同")]:
        panels.append((f"q = {q}", note, field(q, x, y) / q > 0))
    board("基本 QRP：更多求和项，不一定带来不同纹样",
          "统一条件：Q = H / q，零相位、等权重；[-6π, 6π]²；Q > 0 着墨；无 Wang、无空间变形。",
          panels, 3, "这些图用于核对模型性质，不是新的风格方案。数学等价性见报告推导与 JSON 数值核对。",
          args.output / "basic-fields.png")
    q5 = field(5, x, y) / 5
    levels = np.array([-0.4, -0.2, 0.0, 0.2, 0.4, 0.6, 0.8])
    band_index = np.searchsorted(levels, q5)
    thin = np.zeros_like(q5, dtype=bool)
    for level in levels:
        thin |= np.abs(q5 - level) < 0.015
    panels = [
        ("A / 单阈值分区", "Q > 0 着墨", q5 > 0),
        ("B / 多高度带交替着墨", "阈值：-0.4 到 0.8，间隔 0.2", band_index % 2 == 1),
        ("C / 多等值线附近的窄高度带", "相同阈值；|Q - 阈值| < 0.015", thin),
    ]
    board("同一个 QRP 场：分层方式参与决定可见纹样",
          "三图的 q、方向、相位、坐标窗口与两种颜色完全相同；只改变标量值到颜色的映射。",
          panels, 3, "C 是高度带，不是等几何线宽；此图不证明哪一种更美，也不证明用户可控性或商业价值。",
          args.output / "same-field-mappings.png")
    (args.output / "probes.json").write_text(json.dumps(results, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(results, indent=2, ensure_ascii=False))
    print(f"Figures: {args.output.resolve()}")


if __name__ == "__main__":
    main()
