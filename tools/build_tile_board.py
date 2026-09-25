"""复核瓦片像素复用并排版 C++ 原图，不在 Python 中实现生成模型。"""

import argparse
import hashlib
import json
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

from board_common import BACKGROUND, INK, MUTED, fonts


COLORS = ("#e5684b", "#31a494", "#a36bc5", "#d8a82d")


# 只用于读取 schema 2 之前的研究证据；新输出的标题、分组和父子关系均来自 C++ manifest。
LEGACY_CASE_METADATA = {
    "rings": ("环形组织", True, "", ""),
    "ribbons": ("方向波带", True, "", ""),
    "product": ("双通道格纹", True, "", ""),
    "parent": ("低频父场", False, "", ""),
    "child_rings": ("细环形子场", False, "", ""),
    "child_stripes": ("条纹子场", False, "", ""),
    "stripe_parent": ("带状父场", False, "", ""),
    "child_cross": ("横向子场", False, "", ""),
    "nested_rings": ("分簇细环形", True, "parent", "child_rings"),
    "nested_stripes": ("分簇条纹", True, "parent", "child_stripes"),
    "nested_cross": ("带内横向短条", True, "stripe_parent", "child_cross"),
}


def read_manifest(directory):
    manifest = json.loads((directory / "manifest.json").read_text(encoding="utf-8"))
    if manifest.get("schema", 1) < 2:
        for case in manifest["cases"]:
            label, featured, parent, child = LEGACY_CASE_METADATA[case["style"]]
            case.update(label=label, featured=featured, parent_style=parent, child_style=child)
    return manifest


def featured_cases(manifest):
    # 固定/匹配来源两例共用一个风格行，不让图板重新定义实验分组。
    cases = {}
    for case in manifest["cases"]:
        if case["featured"]:
            cases.setdefault(case["style"], case)
    return list(cases.values())


def style_rows(manifest):
    return [(case["style"], f"{index:02d}  {case['label']}")
            for index, case in enumerate(featured_cases(manifest), 1)]


def organization_rows(manifest):
    return [(case["label"], case["parent_style"], case["child_style"], case["style"])
            for case in featured_cases(manifest) if case["relation"] == "nested"]


def load(path):
    with Image.open(path) as source:
        return source.convert("RGB")


def verify(directory, manifest):
    size, pixels = manifest["grid_size"], manifest["pixels_per_tile"]
    instances = 0
    control_instances = 0
    for case in manifest["cases"]:
        root = directory / case["id"]
        tiles = [load(root / "tiles" / f"tile_{i}.png") for i in range(16)]
        assert all(tile.size == (pixels, pixels) for tile in tiles)
        layouts = [(f"layout_{name}.png", layout["ids"], False) for name, layout in manifest["layouts"].items()]
        if "periodic_control" in case:
            layouts.append((case["periodic_control"], [0] * (size * size), True))
        for name, ids, control in layouts:
            result = load(root / name)
            assert result.size == (size * pixels, size * pixels)
            assert len(result.getcolors(result.width * result.height)) > 1
            for index, tile_id in enumerate(ids):
                y, x = divmod(index, size)
                top = (size - 1 - y) * pixels
                actual = result.crop((x * pixels, top, (x + 1) * pixels, top + pixels))
                assert actual.tobytes() == tiles[tile_id].tobytes(), (case["id"], name, x, y)
                if control:
                    control_instances += 1
                else:
                    instances += 1
    styles = {case["style"] for case in manifest["cases"]} if manifest.get("include_references", True) else set()
    for style in styles:
        assert load(directory / f"reference_{style}.png").size == (size * pixels, size * pixels)
    source_count = len(manifest["cases"]) * (16 + len(manifest["layouts"])) + len(styles)
    source_count += sum("periodic_control" in case for case in manifest["cases"])
    print(f"Verified: {source_count} source PNGs; {instances} Wang-layout + {control_instances} periodic-control instances exactly reuse cached RGB pixels.")


def comparison(directory, manifest, face):
    panel, margin, gap = 320, 28, 20
    board = Image.new("RGB", (2 * margin + 3 * panel + 2 * gap, 202 + 366 * len(style_rows(manifest))), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "QRP 独立瓦片：造型保留对照", font=face[34], fill=INK)
    draw.text((margin, 74), "固定双色、同一 QRP 参数；后两列使用相同的 8×8 合法 Wang 布局 A", font=face[20], fill=MUTED)
    for column, label in enumerate(("全局 QRP 参考（未铺砌）", "固定来源瓦片", "边界匹配来源瓦片")):
        draw.text((margin + column * (panel + gap), 118), label, font=face[20], fill=INK)
    for row, (style, label) in enumerate(style_rows(manifest)):
        top = 158 + row * 366
        draw.text((margin, top), label, font=face[20], fill=INK)
        paths = [directory / f"reference_{style}.png"] + [directory / f"{policy}_{style}" / "layout_A.png" for policy in ("fixed", "matched")]
        for column, path in enumerate(paths):
            board.paste(load(path).resize((panel, panel), Image.Resampling.LANCZOS), (margin + column * (panel + gap), top + 32))
    draw.text((margin, board.height - 36), "共享边 C¹ 连续不等于原 QRP 造型无损；匹配分数也不是美感分数。", font=face[17], fill=MUTED)
    board.save(directory / "board_comparison.png")


def reuse(directory, manifest, case_id, face):
    panel, margin, gap, header = 432, 28, 24, 130
    board = Image.new("RGB", (2 * margin + 2 * panel + gap, 1220), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "同一套 16 张瓦片，原样复用于两种布局", font=face[25], fill=INK)
    draw.text((margin, 65), f"{case_id} · 彩色边和数字仅作诊断，不参与纹样生成", font=face[17], fill=MUTED)
    root = directory / case_id
    for i in range(16):
        y, x = divmod(i, 4)
        left, top = margin + x * 108, header + y * 108
        tile = load(root / "tiles" / f"tile_{i}.png").resize((88, 88), Image.Resampling.LANCZOS)
        board.paste(tile, (left + 6, top + 5))
        south, north, west, east = manifest["tile_edges"][i]
        for endpoints, color in (((left+6, top+5, left+93, top+5), north), ((left+6, top+92, left+93, top+92), south), ((left+6, top+5, left+6, top+92), west), ((left+93, top+5, left+93, top+92), east)):
            draw.line(endpoints, fill=COLORS[color], width=3)
        draw.text((left + 39, top + 90), str(i), font=face[15], fill=INK)
    draw.text((margin, header - 30), "固定瓦片库（边色编码端点状态）", font=face[20], fill=INK)
    for name, column, row in (("A", 1, 0), ("B", 0, 1), ("B", 1, 1)):
        left = margin + column * (panel + gap)
        top = header + row * 520
        draw.text((left, top - 30), f"布局 {name}" + ("：类型编号" if row == 1 and column == 1 else "：实际成图"), font=face[20], fill=INK)
        board.paste(load(root / f"layout_{name}.png").resize((panel, panel), Image.Resampling.LANCZOS), (left, top))
        if row == 1 and column == 1:
            cell = panel // manifest["grid_size"]
            layout = manifest["layouts"][name]
            for index, tile_id in enumerate(layout["ids"]):
                y, x = divmod(index, manifest["grid_size"])
                px, py = left + x * cell, top + (manifest["grid_size"] - 1 - y) * cell
                draw.rectangle((px, py, px + cell, py + cell), outline="#d26b3a")
                draw.rectangle((px + 2, py + 2, px + 25, py + 21), fill=BACKGROUND)
                draw.text((px + 3, py + 1), str(tile_id), font=face[15], fill=INK)
        else:
            x, y = manifest["layouts"][name]["marked"]
            cell = panel // manifest["grid_size"]
            px, py = left + x * cell, top + (manifest["grid_size"] - 1 - y) * cell
            draw.rectangle((px, py, px + cell - 1, py + cell - 1), outline="#ed5737", width=3)
            count = manifest["layouts"][name]["ids"].count(6)
            draw.text((left, top + panel + 7), f"红框为同一张类型 6；本布局共出现 {count} 次。", font=face[15], fill=MUTED)
            draw.text((left, top + panel + 28), "源 PNG 与所有对应实例的 RGB 完全一致。", font=face[15], fill=MUTED)
    draw.text((margin, 1162), "边标签：00 / 01 / 10 / 11；南北从左向右，西东从下向上。", font=face[17], fill=MUTED)
    draw.text((margin, 1190), "有限合法铺砌，不声称严格非周期，也不保证大图外边界可重复。", font=face[17], fill=MUTED)
    board.save(directory / "board_reuse.png")


def phase_comparison(directory, manifest, previous, face):
    panel, margin, gap = 320, 28, 20
    board = Image.new("RGB", (2 * margin + 4 * panel + 3 * gap, 202 + 366 * len(style_rows(manifest))), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "从混合场值，转为约束各模态相位", font=face[34], fill=INK)
    draw.text((margin, 74), "三类 QRP 参数、同一布局与双色表达；相位闭合是受约束扩展，不是原 QRP 的无损裁切", font=face[20], fill=MUTED)
    for column, label in enumerate(("全局 QRP 参考（未铺砌）", "上一轮：固定来源混合", "上一轮：匹配来源混合", "本轮：相位兼容生成")):
        draw.text((margin + column * (panel + gap), 118), label, font=face[20], fill=INK)
    for row, (style, label) in enumerate(style_rows(manifest)):
        top = 158 + row * 366
        draw.text((margin, top), label, font=face[20], fill=INK)
        paths = [directory / f"reference_{style}.png"]
        paths += [previous / f"{policy}_{style}" / "layout_A.png" for policy in ("fixed", "matched")]
        paths += [directory / f"phase_{style}" / "layout_A.png"]
        for column, path in enumerate(paths):
            board.paste(load(path).resize((panel, panel), Image.Resampling.LANCZOS), (margin + column * (panel + gap), top + 32))
    draw.text((margin, board.height - 36), "相位约束保留模态振幅，却会改变局部波矢；不把贯通核心带的保证等同于所有轮廓拓扑不变。", font=face[17], fill=MUTED)
    board.save(directory / "board_phase_comparison.png")


def phase_patterns(directory, manifest, face):
    panel, margin, gap = 384, 28, 20
    board = Image.new("RGB", (2*margin+3*panel+2*gap, 588), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "相位兼容 QRP 瓦片：三类实际成图", font=face[34], fill=INK)
    draw.text((margin, 73), "每类 16 张固定瓦片 · 同一合法 Wang 布局 · 不混合标量来源 · 不做接缝后处理", font=face[20], fill=MUTED)
    for column, (style, title) in enumerate(style_rows(manifest)):
        x = margin + column*(panel+gap)
        draw.text((x, 116), title, font=face[25], fill=INK)
        board.paste(load(directory / f"phase_{style}" / "layout_A.png").resize((panel,panel),Image.Resampling.LANCZOS), (x,155))
    draw.text((margin, 553), "研究候选：保留 QRP 模态振幅，修改相位以闭合边界；不等价于原始全局 QRP。", font=face[17], fill=MUTED)
    board.save(directory / "board_patterns.png")


def phase_scale_comparison(directory, manifest, face):
    panel, margin, gap = 320, 28, 20
    board = Image.new("RGB", (2*margin+3*panel+2*gap, 202 + 366 * len(style_rows(manifest))), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "相位闭合的代价：瓦片承载多少结构？", font=face[34], fill=INK)
    draw.text((margin, 74), "相同 QRP 参数、32×32 源视野与成图尺寸；网格和布局随瓦片跨度改变", font=face[20], fill=MUTED)
    for column, label in enumerate(("全局 QRP 参考", "8×8 瓦片，源跨度 L=4", "4×4 瓦片，源跨度 L=8")):
        draw.text((margin+column*(panel+gap), 118), label, font=face[20], fill=INK)
    for row, (style, label) in enumerate(style_rows(manifest)):
        top = 158+row*366
        draw.text((margin, top), label, font=face[20], fill=INK)
        paths = [directory/f"reference_{style}.png", directory/f"phase_{style}"/"layout_A.png", directory/"larger-tiles"/f"phase_{style}"/"layout_A.png"]
        for column, path in enumerate(paths):
            board.paste(load(path).resize((panel,panel),Image.Resampling.LANCZOS), (margin+column*(panel+gap),top+32))
    draw.text((margin, board.height - 36), "更大瓦片可降低相位修正的相对梯度上界；不保证审美更好，也不恢复原全局 QRP。", font=face[17], fill=MUTED)
    board.save(directory/"board_phase_scale.png")


def wang_value_comparison(directory, manifest, face):
    styles = style_rows(manifest)
    panel, margin, gap = 320, 28, 20
    board = Image.new("RGB", (2*margin+3*panel+2*gap, 202 + 366 * len(style_rows(manifest))), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "同一套内容：普通重复与 Wang 组合", font=face[34], fill=INK)
    draw.text((margin, 74), "左列只重复库内可自匹配的类型 0；右两列复用同库的多种类型，均为合法铺砌", font=face[20], fill=MUTED)
    for column, label in enumerate(("单一瓦片周期重复", "多类型 Wang 布局 A", "同库重新铺砌：布局 B")):
        draw.text((margin+column*(panel+gap), 118), label, font=face[20], fill=INK)
    for row, (style, label) in enumerate(styles):
        top = 158+row*366
        draw.text((margin, top), label, font=face[20], fill=INK)
        for column, name in enumerate(("repeat_0.png", "layout_A.png", "layout_B.png")):
            board.paste(load(directory/f"phase_{style}"/name).resize((panel,panel),Image.Resampling.LANCZOS), (margin+column*(panel+gap),top+32))
    draw.text((margin, board.height - 36), "Wang 提供库内结构的合法重组与变化；不声称消除全部重复感，或已证明严格非周期和商业价值。", font=face[17], fill=MUTED)
    board.save(directory/"board_wang_value.png")


def organization_boards(directory, manifest, previous, previous_manifest, face):
    panel, margin, gap = 320, 28, 20
    rows = organization_rows(manifest)
    board = Image.new("RGB", (2*margin+4*panel+3*gap, 202 + 366 * len(style_rows(manifest))), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "兼容通道的父子组织：来源与结果", font=face[34], fill=INK)
    draw.text((margin, 74), "父场决定区域，子场决定内部纹样；每个结果先烘焙 16 张固定瓦片，再独立铺砌", font=face[20], fill=MUTED)
    for column, label in enumerate(("父场负值区", "未门控子场", "门控结果 / 布局 A", "同库重铺 / 布局 B")):
        draw.text((margin+column*(panel+gap), 118), label, font=face[20], fill=INK)
    for row, (label, parent, child, result) in enumerate(rows):
        top = 158+row*366
        draw.text((margin, top), f"0{row+1}  {label}", font=face[20], fill=INK)
        paths = [(parent,"A"),(child,"A"),(result,"A"),(result,"B")]
        for column, (name, layout) in enumerate(paths):
            board.paste(load(directory/f"phase_{name}"/f"layout_{layout}.png").resize((panel,panel),Image.Resampling.LANCZOS),
                        (margin+column*(panel+gap),top+32))
    draw.text((margin, board.height - 36), "门控边缘会收缩、截短子结构，不是完整图元的无损摆放；局部连续不代表全部轮廓拓扑不变。", font=face[17], fill=MUTED)
    board.save(directory/"board_organization.png")
    if previous is None:
        return
    board = Image.new("RGB", (2*margin+3*panel+2*gap, 962), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "从铺满全幅，到由父场组织局部细节", font=face[34], fill=INK)
    draw.text((margin, 74), "同一源视野和双色表达；下排增加父子尺度分工，瓦片跨度见实验记录", font=face[20], fill=MUTED)
    items = [(previous, case["style"], "已有 / " + case["label"]) for case in featured_cases(previous_manifest)]
    items += [(directory,row[3],row[0]) for row in rows]
    for index, (root, name, label) in enumerate(items):
        row, column = divmod(index,3)
        x, y = margin+column*(panel+gap), 130+row*394
        draw.text((x,y), label, font=face[25], fill=INK)
        board.paste(load(root/f"phase_{name}"/"layout_A.png").resize((panel,panel),Image.Resampling.LANCZOS), (x,y+42))
    draw.text((margin, 925), "三种新组织是同一父子门控关系的不同通道配置，不冒充三套独立算法。", font=face[17], fill=MUTED)
    board.save(directory/"board_style_span.png")


def organization_resolution_board(directory, resolved, manifest, face):
    panel, margin, gap = 320, 28, 20
    span = manifest["source_span"]
    board = Image.new("RGB", (2*margin+3*panel+2*gap, 202 + 366 * len(style_rows(manifest))), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "父场需要足够的相位分辨率", font=face[34], fill=INK)
    draw.text((margin, 74), "QRP 参数、源视野和总像素数不变；瓦片跨度由全部通道的闭合误差确定", font=face[20], fill=MUTED)
    for column, label in enumerate(("初轮结果 / L=4", f"父场区域 / L={span:g}", f"修正后结果 / L={span:g}")):
        draw.text((margin+column*(panel+gap), 118), label, font=face[20], fill=INK)
    rows = [(label, parent, result) for label, parent, _, result in organization_rows(manifest)]
    for row, (label,parent,result) in enumerate(rows):
        top = 158+row*366
        draw.text((margin, top), f"0{row+1}  {label}", font=face[20], fill=INK)
        paths = [directory/f"phase_{result}"/"layout_A.png",resolved/f"phase_{parent}"/"layout_A.png",resolved/f"phase_{result}"/"layout_A.png"]
        for column, path in enumerate(paths):
            board.paste(load(path).resize((panel,panel),Image.Resampling.LANCZOS),(margin+column*(panel+gap),top+32))
    draw.text((margin, board.height - 36), "布局随瓦片跨度改变；限制的是平均闭合波矢误差，不是完整局部梯度误差或美感分数。", font=face[17], fill=MUTED)
    board.save(directory/"board_resolution.png")


def retiling_study(directory, manifest, previous, previous_manifest, face):
    styles = style_rows(manifest)
    wang_value_comparison(directory, manifest, face)
    panel, margin, gap = manifest["pixels_per_tile"], 28, 20
    row_height = panel + 50
    board = Image.new("RGB", (2*margin+3*panel+2*gap, 200+len(styles)*row_height), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "同位置原像素局部：四张瓦片的交会处", font=face[25], fill=INK)
    draw.text((margin, 68), f"原图 {manifest['grid_size']*panel}px；每格截取中央 {panel}px，不缩放、不显示边框", font=face[17], fill=MUTED)
    for column, label in enumerate(("单一瓦片周期重复", "多类型 Wang 布局 A", "同库重铺 / 布局 B")):
        draw.text((margin+column*(panel+gap), 108), label, font=face[20], fill=INK)

    # 只测量已有 RGB 图片，不重写模型公式；逐像素变化不是感知或美感评分。
    def changed_fraction(first, second):
        r, g, b = ImageChops.difference(first, second).split()
        different = ImageChops.lighter(ImageChops.lighter(r, g), b)
        return 1.0 - different.histogram()[0] / (first.width * first.height)

    metrics = {"grid_size": manifest["grid_size"], "pixels_per_tile": panel,
               "source_span": manifest["source_span"], "previous_library_pngs_identical": None,
               "definition": "RGB pixel inequality at one-tile x/y lags over non-wrapped overlap; not a perception score",
               "layout_type_counts": {name: [layout["ids"].count(i) for i in range(16)]
                                      for name, layout in manifest["layouts"].items()}, "cases": []}
    if previous is not None:
        old = previous_manifest
        for key in ("source_span", "pixels_per_tile", "supersampling", "level", "ink", "paper", "tile_edges"):
            assert old[key] == manifest[key], ("library setting changed", key)
        old_cases = {case["id"]: case for case in old["cases"]}
        count = 0
        for case in manifest["cases"]:
            for channel in old_cases[case["id"]]["channels"]:
                channel.setdefault("vertex_phase_offsets", [[0, 0], [0, 0]])
            for key in ("channels", "relation", "parent_level", "gate_width"):
                assert old_cases[case["id"]][key] == case[key], ("QRP recipe changed", case["id"], key)
            for tile in range(16):
                relative = Path(case["id"]) / "tiles" / f"tile_{tile}.png"
                assert hashlib.sha256((previous / relative).read_bytes()).digest() == hashlib.sha256((directory / relative).read_bytes()).digest(), relative
                count += 1
        metrics["previous_library_pngs_identical"] = count
        print(f"Verified: {count} source tile PNGs byte-identical to previous study.")

    for row, (style, label) in enumerate(styles):
        top = 150 + row*row_height
        draw.text((margin, top), label, font=face[20], fill=INK)
        root = directory / f"phase_{style}"
        record = {"id": root.name, "one_tile_shift_changed_fraction": {}}
        for column, name in enumerate(("repeat_0.png", "layout_A.png", "layout_B.png")):
            source = load(root/name)
            left = (source.width-panel)//2
            board.paste(source.crop((left,left,left+panel,left+panel)), (margin+column*(panel+gap),top+32))
            width, height = source.size
            record["one_tile_shift_changed_fraction"][name] = {
                "x": changed_fraction(source.crop((0,0,width-panel,height)), source.crop((panel,0,width,height))),
                "y": changed_fraction(source.crop((0,0,width,height-panel)), source.crop((0,panel,width,height)))}
        record["layout_A_vs_B_changed_fraction"] = changed_fraction(load(root/"layout_A.png"), load(root/"layout_B.png"))
        metrics["cases"].append(record)
    draw.text((margin, board.height-33), "全貌中的重复与此处的局部变化应分别判断；没有重新生成逐实例内容。", font=face[17], fill=MUTED)
    board.save(directory/"board_retiling_detail.png")
    (directory/"retiling_metrics.json").write_text(json.dumps(metrics, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")


def phase_state_comparison(directory, manifest, candidate, face):
    panel, margin, gap = 384, 28, 20
    row_height = panel+50
    styles = style_rows(manifest)
    for detail in (False, True):
        board = Image.new("RGB", (2*margin+3*panel+2*gap, 200+len(styles)*row_height), BACKGROUND)
        draw = ImageDraw.Draw(board)
        draw.text((margin,20), "内部状态：从源坐标平移到 QRP 相对相位", font=face[25], fill=INK)
        draw.text((margin,68), "只改变父场的状态编码；子场、16 种瓦片类型、合法布局和采样保持不变", font=face[17], fill=MUTED)
        for column, label in enumerate(("原方法 / 布局 A", "相对相位 / 同一布局 A", "相对相位 / 重铺布局 B")):
            draw.text((margin+column*(panel+gap),108), label, font=face[20], fill=INK)
        for row, (style,label) in enumerate(styles):
            top = 150+row*row_height
            draw.text((margin,top), label, font=face[20], fill=INK)
            for column, (root,layout) in enumerate(((directory,"A"),(candidate,"A"),(candidate,"B"))):
                source = load(root/f"phase_{style}"/f"layout_{layout}.png")
                if detail:
                    left = (source.width-panel)//2
                    sample = source.crop((left,left,left+panel,left+panel))
                else:
                    sample = source.resize((panel,panel),Image.Resampling.LANCZOS)
                board.paste(sample,(margin+column*(panel+gap),top+32))
        note = "同一位置的 384px 原像素局部，不缩放。" if detail else "大范围全貌缩略图；局部形状请同时查看 detail 图，不以像素差异代替审美判断。"
        draw.text((margin,board.height-33),note,font=face[17],fill=MUTED)
        board.save(directory/("board_phase_states_detail.png" if detail else "board_phase_states.png"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("--case")
    parser.add_argument("--previous", type=Path, help="所比较的上一轮研究输出目录")
    parser.add_argument("--font", type=Path, default=Path("C:/Windows/Fonts/msyh.ttc"))
    args = parser.parse_args()
    manifest = read_manifest(args.directory)
    verify(args.directory, manifest)
    face = fonts(args.font)
    previous_manifest = read_manifest(args.previous) if args.previous is not None else None
    phase = manifest["model"] == "qrp-phase-compatible-tiles-v1"
    organization = manifest.get("experiment") == "organization"
    retiling = manifest.get("experiment") == "retiling"
    if retiling:
        retiling_study(args.directory,manifest,args.previous,previous_manifest,face)
        candidate = args.directory / "relative-phase"
        if (candidate / "manifest.json").exists():
            candidate_manifest = read_manifest(candidate)
            verify(candidate,candidate_manifest)
            retiling_study(candidate,candidate_manifest,None,None,face)
            phase_state_comparison(args.directory,manifest,candidate,face)
            reuse(candidate,candidate_manifest,featured_cases(candidate_manifest)[0]["id"],face)
    elif organization:
        organization_boards(args.directory,manifest,args.previous,previous_manifest,face)
        wang_value_comparison(args.directory,manifest,face)
        resolved = args.directory / "resolved-scale"
        if (resolved / "manifest.json").exists():
            resolved_manifest = read_manifest(resolved)
            verify(resolved,resolved_manifest)
            organization_boards(resolved,resolved_manifest,args.previous,previous_manifest,face)
            wang_value_comparison(resolved,resolved_manifest,face)
            organization_resolution_board(args.directory,resolved,resolved_manifest,face)
            reuse(resolved,resolved_manifest,featured_cases(resolved_manifest)[0]["id"],face)
    elif phase:
        phase_patterns(args.directory, manifest, face)
        wang_value_comparison(args.directory, manifest, face)
        larger = args.directory / "larger-tiles"
        if (larger / "manifest.json").exists():
            larger_manifest = read_manifest(larger)
            verify(larger, larger_manifest)
            phase_scale_comparison(args.directory, manifest, face)
            reuse(larger, larger_manifest, featured_cases(larger_manifest)[0]["id"], face)
        if args.previous is not None:
            phase_comparison(args.directory, manifest, args.previous, face)
    else:
        comparison(args.directory, manifest, face)
    default_case = featured_cases(manifest)[0]["id"]
    reuse(args.directory, manifest, args.case or default_case, face)


if __name__ == "__main__":
    main()
