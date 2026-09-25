"""将 C++ 实验生成的小样排为对照板；不在 Python 中重写 QRP 或 Wang 公式。"""

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw

from board_common import ACCENT, BACKGROUND, INK, MUTED, fonts

GROUPS = [
    ("01  相位组合", "同一共振方向集合内的形态变化"),
    ("02  共振方向数", "离散结构选项，不等同于连续复杂度"),
    ("03  粗场空间频率", "只改变粗场频率，中细场保持固定"),
    ("04  分层细节", "粗结构不变，只改变中细层强度"),
]
DIRECTION_GROUPS = [
    ("01  方向偏置", "从等权团簇到方向性条带，其他条件不变"),
    ("02  结构方向", "旋转载波方向；Wang 网格保持固定"),
    ("03  使用场景", "以 D03 为参考，依次改变频率、细节或相位"),
    ("04  交叉方向", "加入第二组方向权重，观察连接结构而非声称真实编织"),
]
VIEWS = {
    "coarse_gray": ("粗结构 / 固定灰度", "只看 QRP 粗场 C，不含分层细节与色带"),
    "final_gray": ("分层结果 / 固定灰度", "只看最终标量场 F，不含色带"),
    "color": ("最终着色 / 固定色带", "同一 MidnightGold 色带；颜色轮廓不等同于结构轮廓"),
}


def parameter_lines(case):
    phase_a, phase_b = case["phase"]
    medium, fine = case["detail"]
    lines = [
        f"q={case['q']}   κ={case['frequency']:g}   A={phase_a:g}, B={phase_b:g}",
        f"细节 βm={medium:g}, βf={fine:g}",
    ]
    if "directional_bias" in case:
        lines.append(f"η={case['directional_bias']:g}   旋转={case['orientation_degrees']:g}°   交叉={case.get('cross_mix', 0):g}")
    return lines


def make_board(directory: Path, manifest, view: str, face):
    panel, gap, margin = 320, 20, 28
    header, row_height, footer = 148, 458, 60
    rows = (len(manifest["cases"]) + 3) // 4
    directional = manifest.get("study") == "directional"
    width = 2 * margin + 4 * panel + 3 * gap
    board = Image.new("RGB", (width, header + rows * row_height + footer), BACKGROUND)
    draw = ImageDraw.Draw(board)
    title, subtitle = VIEWS[view]
    draw.text((margin, 20), f"QRP 参数探索  |  {title}", font=face[34], fill=INK)
    draw.text((margin, 72), subtitle, font=face[20], fill=MUTED)
    note = "方向实验：第一行递增偏置，其余以 D03 为参考" if directional else "S03 为共同参考"
    draw.text((margin, 104), f"Wang 铺砌及内部耦合全部固定 · 20×20 窗口 · {note}", font=face[17], fill=MUTED)
    for index, case in enumerate(manifest["cases"]):
        row, column = divmod(index, 4)
        x = margin + column * (panel + gap)
        row_top = header + row * row_height
        if column == 0:
            group_title, group_subtitle = (DIRECTION_GROUPS if directional else GROUPS)[row]
            draw.text((x, row_top), group_title, font=face[25], fill=INK)
            draw.text((x + 246, row_top + 6), group_subtitle, font=face[17], fill=MUTED)
        y = row_top + 41
        with Image.open(directory / f"{case['id']}_{view}.png") as source:
            thumbnail = source.convert("RGB").resize((panel, panel), Image.Resampling.LANCZOS)
        board.paste(thumbnail, (x, y))
        is_reference = case["id"] == manifest["reference"]
        draw.rectangle((x - 1, y - 1, x + panel, y + panel), outline=ACCENT if is_reference else "#bbc3c2", width=2 if is_reference else 1)
        label = case["id"].split("_")[0] + ("  参考" if is_reference else "")
        draw.text((x, y + panel + 5), label, font=face[17], fill=ACCENT if is_reference else INK)
        for line_index, line in enumerate(parameter_lines(case)):
            draw.text((x, y + panel + 29 + 21 * line_index), line, font=face[15], fill=MUTED)
    draw.text((margin, board.height - 42), "灰度统一：-1→黑，0→中灰，+1→白；无逐图归一化。此板用于观察，不代表风格分类或商业价值验证。", font=face[17], fill=MUTED)
    board.save(directory / f"board_{view}.png")


def make_reading_panel(directory: Path, manifest, face):
    """三个相同参数的视图并排，避免把色带产生的线条误认为粗场结构。"""
    indices = (0, 1, 2, 14) if manifest.get("study") == "directional" else (2, 4, 8, 14)
    selected = [manifest["cases"][i] for i in indices]
    panel, margin, gap, label_width = 320, 28, 20, 218
    width = 2 * margin + label_width + 3 * panel + 2 * gap
    board = Image.new("RGB", (width, 160 + len(selected) * 350 + 54), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "结构还是着色？同一小样的三层观察", font=face[34], fill=INK)
    draw.text((margin, 76), "仅改变 QRP 参数；Wang 配置固定。所有图均使用完整的 20×20 窗口。", font=face[20], fill=MUTED)
    for column, title in enumerate(("粗结构 C", "分层结果 F", "最终着色")):
        draw.text((margin + label_width + column * (panel + gap), 118), title, font=face[25], fill=INK)
    for row, case in enumerate(selected):
        y = 160 + row * 350
        draw.text((margin, y + 12), case["id"].split("_")[0], font=face[25], fill=INK)
        draw.text((margin, y + 53), f"q={case['q']}  κ={case['frequency']:g}", font=face[20], fill=MUTED)
        draw.text((margin, y + 85), f"A={case['phase'][0]:g}  B={case['phase'][1]:g}", font=face[17], fill=MUTED)
        draw.text((margin, y + 117), f"βm={case['detail'][0]:g}", font=face[17], fill=MUTED)
        draw.text((margin, y + 147), f"βf={case['detail'][1]:g}", font=face[17], fill=MUTED)
        if "directional_bias" in case:
            draw.text((margin, y + 179), f"偏置 η={case['directional_bias']:g}", font=face[17], fill=MUTED)
            draw.text((margin, y + 211), f"旋转={case['orientation_degrees']:g}°", font=face[17], fill=MUTED)
            draw.text((margin, y + 243), f"交叉={case.get('cross_mix', 0):g}", font=face[17], fill=MUTED)
        for column, view in enumerate(VIEWS):
            with Image.open(directory / f"{case['id']}_{view}.png") as source:
                thumbnail = source.convert("RGB").resize((panel, panel), Image.Resampling.LANCZOS)
            board.paste(thumbnail, (margin + label_width + column * (panel + gap), y))
    draw.text((margin, board.height - 42), "统一灰度映射保留亮度差异；缩略图会弱化细节，判断时请配合 640×640 原图。", font=face[17], fill=MUTED)
    board.save(directory / "board_reading.png")


def make_candidate_strip(directory: Path, manifest, face):
    panel, margin, gap = 320, 28, 20
    board = Image.new("RGB", (2 * margin + 4 * panel + 3 * gap, 542), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "四个可试调的 QRP 起点", font=face[34], fill=INK)
    draw.text((margin, 75), "同一模型、同一色带、固定 Wang 铺砌 · 只改变方向权重与结构旋转", font=face[20], fill=MUTED)
    for column, (index, name) in enumerate(zip((0, 1, 5, 15), ("团簇", "串珠", "流线", "交叉"))):
        case = manifest["cases"][index]
        x = margin + column * (panel + gap)
        with Image.open(directory / f"{case['id']}_color.png") as source:
            board.paste(source.resize((panel, panel), Image.Resampling.LANCZOS), (x, 120))
        draw.text((x, 450), name, font=face[25], fill=INK)
        draw.text((x, 489), f"η={case['directional_bias']:g} · 旋转={case['orientation_degrees']:g}° · 交叉={case.get('cross_mix', 0):g}", font=face[17], fill=MUTED)
    board.save(directory / "board_candidates.png")


def make_order_boards(directory: Path, manifest, face):
    """只排版 C++ 原图，组间的模型、色带和对照关系明确标注。"""
    panel, margin, gap = 320, 28, 20
    boards = {
        "board_decision": (
            "从杂乱到秩序：两种不同的取舍",
            "前两张粗场相同；第三张保留直接 QRP；第四张换成规则载波＋QRP 形变。",
            [[("A01_cluster_original", "原结果", "q=7，独立细节＋多重色带"),
              ("A04_cluster_ink", "只简化显示与细节", "轮廓的杂乱仍然存在"),
              ("A13_q5_zero_level", "A / 保留 QRP 造型", "q=5，弱耦合，清楚的形与间隔"),
              ("O05_elongated", "B / 固定单元骨架", "轮廓更稳定，排列也更规则")]]),
        "board_ablation": (
            "先诊断：杂乱来自哪一层？",
            "每行从左到右逐步简化；前两步不改变粗骨架。Wang 网格完全相同。",
            [
                [("A01_cluster_original", "原团簇", "独立细节＋周期色带"),
                 ("A02_cluster_no_detail", "去掉独立细节", "只保留粗 QRP"),
                 ("A03_cluster_no_bands", "再去掉周期色带", "同色组，单调映射"),
                 ("A04_cluster_ink", "改用简明双色", "形状仍来自同一粗 QRP")],
                [("A05_flow_original", "原流线", "独立细节＋周期色带"),
                 ("A06_flow_no_detail", "去掉独立细节", "只保留粗 QRP"),
                 ("A07_flow_no_bands", "再去掉周期色带", "同色组，单调映射"),
                 ("A08_flow_ink", "改用简明双色", "分叉、粗细变化仍存在")],
            ]),
        "board_direct": (
            "保守路线：仍由 QRP 直接决定轮廓",
            "全部无独立细节、同一双色；每一步只变一个因素。弱耦合是内部机制对照。",
            [[("A09_zero_phase", "全局相位归零", "q=7，保留原内部耦合"),
              ("A10_weak_coupling", "减弱内部耦合", "q=7，相位=0"),
              ("A11_q5", "改变方向数", "q=5，相位=0"),
              ("A12_q5_phase", "加入少量相位", "q=5，A=0.5，B=0.15")]]),
        "board_order": (
            "候选路线：先有清楚的单元，再加入变化",
            "同一双色、无独立细节。规则载波负责造型；Wang-QRP 只驱动有限形变。",
            [[("O01_periodic_spots", "规则单元基准", "变化量=0，是周期图案"),
              ("O02_varied_spots", "单元＋QRP 变化", "变化量=0.8，其他相同"),
              ("O03_periodic_ribbons", "规则条带基准", "变化量=0，是周期图案"),
              ("O04_varied_ribbons", "条带＋QRP 变化", "变化量=0.8，其他相同")]]),
        "board_profiles": (
            "同一 QRP 骨架：形与线的表达",
            "q=5，相位=0，弱内部耦合，无独立细节；四张图的原始标量场完全相同。",
            [[("A11_q5", "较大深色填充区域", "深色对应 C<0.2"),
              ("A13_q5_zero_level", "选择负值区域", "深色对应 C<0"),
              ("A14_q5_negative_level", "缩小深色填充区域", "深色对应 C<-0.15"),
              ("A15_q5_contour", "只描一层结构轮廓", "C=0 附近的等值带")]]),
        "board_candidates": (
            "局部秩序候选 · 先评价造型，不靠复杂色带",
            "这是有序载波＋QRP 形变的新构造，不是原 canonical QRP 的等价变形。",
            [[("O02_varied_spots", "圆形单元", "q=7，变化量=0.8"),
              ("O05_elongated", "倾斜长单元", "横纵比例=0.45，旋转=30°"),
              ("O04_varied_ribbons", "连续条带", "q=7，变化量=0.8"),
              ("O06_diagonal", "斜向排列", "频率=2.2，变化量=1")]]),
    }
    for name, (title, subtitle, rows) in boards.items():
        board = Image.new("RGB", (2 * margin + 4 * panel + 3 * gap, 138 + len(rows) * 414 + 42), BACKGROUND)
        draw = ImageDraw.Draw(board)
        draw.text((margin, 22), title, font=face[34], fill=INK)
        draw.text((margin, 80), subtitle, font=face[20], fill=MUTED)
        for row_index, row in enumerate(rows):
            y = 138 + row_index * 414
            for column, (case_id, label, note) in enumerate(row):
                x = margin + column * (panel + gap)
                with Image.open(directory / f"{case_id}_color.png") as source:
                    board.paste(source.convert("RGB").resize((panel, panel), Image.Resampling.LANCZOS), (x, y))
                draw.text((x, y + panel + 9), label, font=face[25], fill=INK)
                draw.text((x, y + panel + 47), note, font=face[17], fill=MUTED)
        draw.text((margin, board.height - 34), "完整 20×20 窗口。原图和参数均在同目录；缩略排版不参与模型生成。", font=face[17], fill=MUTED)
        board.save(directory / f"{name}.png")


def make_design_boards(directory: Path, manifest, face):
    cases = {case["id"]: case for case in manifest["cases"]}
    groups = {
        "board_geometry": ("A 路线：几何留白与轮廓", "四张图的 QRP 场相同；变化的是向内收缩和描线方式。", [
            ("T01_no_inset", "直接填充 / 未收缩"), ("T02_inset", "向内收缩 / 留白"),
            ("T03_outline", "几何线描 / 未收缩"), ("T04_inset_outline", "收缩后的几何线描")]),
        "board_range": ("A 路线：参数范围观察", "不是全参数空间保证：形状扩展为正时，连接明显增加；过度收缩也会损失小单元。", [
            ("T05_sparse", "低频边界"), ("T06_dense", "高频边界"),
            ("T07_phase_positive", "相位 (+0.6,+0.6)"), ("T08_phase_opposed", "相位 (+0.6,-0.6)"),
            ("T09_phase_negative", "相位 (-0.6,-0.6)"), ("T10_level_negative", "降低填充层级"),
            ("T11_level_positive", "提高填充层级 / 更易相连"), ("T12_rotated", "旋转与小幅相位")]),
        "board_candidates": ("QRP 直接造型 · 四组候选观察", "尚非成熟风格：轮廓更干净，但环、瓣与细颈仍会混合出现。", [
            ("U01_flower", "几何花簇"), ("U02_open", "疏朗单元"),
            ("U03_line", "细线花簇"), ("U04_slanted", "斜向线描")]),
        "board_regions": ("回到形状本身：同一 QRP 的不同等值区域", "全部不收缩、不描边、不加细节；深色表示被选中的区域。原始 QRP 场完全相同。", [
            ("T01_no_inset", "C < 0 / 原区域"), ("V01_low_02", "C < -0.2"),
            ("V02_low_035", "C < -0.35"), ("V03_low_05", "C < -0.5"),
            ("V04_high_0", "C > 0 / 互补区域"), ("V05_high_02", "C > 0.2"),
            ("V06_high_035", "C > 0.35"), ("V07_high_05", "C > 0.5")]),
    }
    panel, gap, margin = 320, 20, 28
    if manifest.get("study") == "phase":
        groups = {
            "board_phase": ("直接 QRP：相位差与共同相位的对照", "固定 q=5、Wang、频率、双色与 C<0 区域；无独立细节，无几何收缩。", [
                ("M01_reference", "参考 / 原相位为零"), ("M02_phase_a", "原参数 A=0.3"),
                ("M03_phase_b", "原参数 B=0.3"), ("M04_phase_ab", "原参数 A=B=0.3"),
                ("M05_common_30", "共同相位 30°"), ("M06_common_60", "共同相位 60°"),
                ("M07_common_90", "共同相位 90°"), ("M08_common_120", "共同相位 120°")]),
        }
    if manifest.get("study") == "control":
        groups = {
            "board_control": ("保留当前纹样：方向强度与同类变体", "每列使用相同方向权重；上排共同相位 0°，下排 60°。其他生成和绘制条件相同。", [
                ("N01_reference", "原有环瓣 / η=0"), ("N02_bias_05", "方向增强 / η=0.5"),
                ("N03_bias_10", "方向增强 / η=1.0"), ("N04_bias_15", "方向增强 / η=1.5"),
                ("N05_phase_60", "相位变体 / η=0"), ("N06_bias_05_phase_60", "相位变体 / η=0.5"),
                ("N07_bias_10_phase_60", "相位变体 / η=1.0"), ("N08_bias_15_phase_60", "相位变体 / η=1.5")]),
            "board_axes": ("两组单因素对照：旋转与方向混合", "左两张只变载波角度；右两张只变方向混合。没有改变 Wang 铺砌或配色。", [
                ("N03_bias_10", "旋转参考 / 0°"), ("N09_rotated", "载波旋转 / 30°"),
                ("N04_bias_15", "混合参考 / t=0"), ("N11_cross_05", "方向混合 / t=0.5")]),
        }
    if manifest.get("study") == "diversity":
        groups = {
            "board_diversity": ("风格跨度实验：改变 QRP 场之间的关系", "同一深浅双色；网状与团簇来自两组 QRP 的组合，不使用外部图案或随机噪声。", [
                ("X01_rings", "原有环瓣 / 基准"), ("X02_ribbons", "原有波带 / 基准"),
                ("X03_product_fill", "两场乘积 / 面状"), ("X04_product_network", "两场乘积 / 网状线描"),
                ("X05_energy_cells", "联合能量 / 围合候选"), ("X06_energy_open", "联合能量 / 较少填充"),
                ("X07_nested", "父子门控 / 成组细部"), ("X08_nested_phase", "父子门控 / 子相位变化")]),
            "board_nesting": ("层级组合如何改变纹样：父场、子场与结果", "父场指定子场出现的区域，不再把高频场仅作为边界起伏叠上去。", [
                ("X09_parent", "低频父场 / 区域来源"), ("X10_child", "高频子场 / 全画布"),
                ("X07_nested", "按父场门控子场"), ("X08_nested_phase", "仅改变子场相位")]),
            "board_grouping": ("层级组合第二轮：尺度分离与区域内条纹", "父场与门控规则不变：先增大子场频率，再改变子场的方向权重。末图为未门控的条纹参考。", [
                ("X07_nested", "初轮 / 尺度比 3.5"), ("X11_fine_clusters", "更细子场 / 尺度比 6"),
                ("X12_striped_clusters", "方向性子场 / 区域内条纹"), ("X13_stripe_child", "条纹子场 / 无区域门控")]),
            "board_selected": ("同一 QRP–Wang 方法：四类结构候选", "相同配色；保留环瓣、波带，新增交错格纹与分区条纹。风格区分来自结构，不依赖换色。", [
                ("X01_rings", "环瓣"), ("X02_ribbons", "连续波带"),
                ("X03_product_fill", "交错格纹 / 两场相乘"), ("X12_striped_clusters", "分区条纹 / 父子门控")]),
        }
    for name, (title, subtitle, selected) in groups.items():
        rows = (len(selected) + 3) // 4
        board = Image.new("RGB", (2 * margin + 4 * panel + 3 * gap, 135 + rows * 418 + 45), BACKGROUND)
        draw = ImageDraw.Draw(board)
        draw.text((margin, 20), title, font=face[34], fill=INK)
        draw.text((margin, 78), subtitle, font=face[20], fill=MUTED)
        for index, (case_id, label) in enumerate(selected):
            row, column = divmod(index, 4)
            x, y = margin + column * (panel + gap), 135 + row * 418
            with Image.open(directory / f"{case_id}_color.png") as source:
                board.paste(source.convert("RGB").resize((panel, panel), Image.Resampling.LANCZOS), (x, y))
            p = cases[case_id]["parameters"]
            draw.text((x, y + panel + 8), label, font=face[20], fill=INK)
            if manifest.get("study") == "diversity":
                draw.text((x, y + panel + 38), f"{p['relation']} · κ1={p['frequency']:g} · 层级={p['level']:g}", font=face[17], fill=MUTED)
                note = f"κ2={p['frequency']*p['second_frequency_ratio']:g} · η2={p['second_directional_bias']:g} · φ2={p['second_common_phase_degrees']:g}°" if p['relation'] != 'direct' else f"单场参考 · 方向偏置={p['directional_bias']:g}"
                draw.text((x, y + panel + 63), note, font=face[15], fill=MUTED)
            elif manifest.get("study") == "control":
                draw.text((x, y + panel + 38), f"η={p['directional_bias']:g} · φ={p['common_phase_degrees']:g}° · t={p['cross_mix']:g} · α={p['angle']:g}°", font=face[17], fill=MUTED)
                draw.text((x, y + panel + 63), f"场方向性={cases[case_id]['field_anisotropy']:.3f}（不是美感评分）", font=face[15], fill=MUTED)
            else:
                draw.text((x, y + panel + 38), f"κ={p['frequency']:g} · 层级={p['level']:g} · 收缩={p['inset']:g}", font=face[17], fill=MUTED)
                draw.text((x, y + panel + 63), f"A={p['phase_a']:g}, B={p['phase_b']:g} · 旋转={p['angle']:g}°", font=face[15], fill=MUTED)
        footer = ("完整 20×20 窗口；640px 与 1600px 出图共用固定 64 格/瓦片的轮廓采样率。"
                  if manifest.get("study") == "diversity" else
                  "完整 20×20 窗口；轮廓按固定 64 格/瓦片提取，预览与导出复用相同几何定义。")
        draw.text((margin, board.height - 33), footer, font=face[17], fill=MUTED)
        board.save(directory / f"{name}.png")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, nargs="?", default=Path("output/qrp-style-study"))
    parser.add_argument("--font", type=Path, default=Path("C:/Windows/Fonts/msyh.ttc"), help="支持中文的字体路径")
    args = parser.parse_args()
    manifest = json.loads((args.directory / "manifest.json").read_text(encoding="utf-8"))
    face = fonts(args.font)
    if manifest.get("study") in ("design", "phase", "control", "diversity"):
        make_design_boards(args.directory, manifest, face)
        print(f"Wrote A-route design boards to {args.directory.resolve()}")
        return
    if manifest.get("study") == "order":
        make_order_boards(args.directory, manifest, face)
        print(f"Wrote order-study boards to {args.directory.resolve()}")
        return
    for view in VIEWS:
        make_board(args.directory, manifest, view, face)
    make_reading_panel(args.directory, manifest, face)
    if manifest.get("study") == "directional":
        make_candidate_strip(args.directory, manifest, face)
    print(f"Wrote boards to {args.directory.resolve()}")


if __name__ == "__main__":
    main()
