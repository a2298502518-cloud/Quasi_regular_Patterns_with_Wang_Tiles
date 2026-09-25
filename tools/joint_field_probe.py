"""QRP 共享边界研究：默认运行设计样例，旧三组保留为诊断对照。

不接入 C++ 主线。输入波向量/权重/相位及 Wang 目录来自 C++ 导出的 manifest，
旧闭合场的独立求值必须与其 PNG 缓存核对。详见 docs/qrp-joint-field-study.md。
依赖：NumPy、SciPy、Pillow。
"""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from board_common import BACKGROUND, INK, MUTED, fonts
from joint_field import (Projection, source_field, multiply_fields, sample_grid, check_basis,
                         raster, design_colors, design_raster, error_metrics, boundary_checks)
from wang_tiles import assemble, endpoint_edges, mismatches


ROOT = Path(__file__).resolve().parents[1]


def make_boards(output, cases, font_path):
    font = fonts(font_path)
    size, margin, gap = 384, 24, 18
    columns = [("raw", "原始 QRP / 全局参考"), ("target_A", "未闭合目标 / 有接缝"),
               ("closed_A", "原逐模态闭合 / 合法"), ("joint_A", "总场联合拟合 / 合法")]
    board = Image.new("RGB", (margin*2+4*size+3*gap, 120+len(cases)*440+55), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 18), "共享边界联合拟合：是否更少改变 QRP 形态？", font=font[34], fill=INK)
    draw.text((margin, 69), "同尺度 4×4 窗口；q=5，κ=3.15，L=4；后三列同瓦片 ID；统一零阈值、双色与采样。", font=font[20], fill=MUTED)
    for row, case in enumerate(cases):
        for col, (name, title) in enumerate(columns):
            left, top = margin+col*(size+gap), 120+row*440
            draw.text((left, top), f"{case['style']} · {title}", font=font[17], fill=INK)
            with Image.open(output/case["id"]/f"{name}.png") as im:
                board.paste(im.resize((size, size), Image.Resampling.LANCZOS), (left, top+34))
    draw.text((margin, board.height-44), "原始参考不与局部目标逐点对齐；联合拟合不等于精确 QRP，也未保证零等值线拓扑。", font=font[20], fill=MUTED)
    board.save(output/"comparison.png")
    reuse = Image.new("RGB", (2*size+2*margin+gap, 110+len(cases)*440+55), BACKGROUND)
    draw = ImageDraw.Draw(reuse)
    draw.text((margin, 18), "同一联合拟合瓦片库，两个合法布局", font=font[25], fill=INK)
    draw.text((margin, 62), "每种风格只求解一次、烘焙 16 张；铺砌仅复制像素。", font=font[20], fill=MUTED)
    for row, case in enumerate(cases):
        for col, layout in enumerate(("A", "B")):
            left, top = margin+col*(size+gap), 110+row*440
            draw.text((left, top), f"{case['style']} / {layout}", font=font[20], fill=INK)
            with Image.open(output/case["id"]/f"joint_{layout}.png") as im:
                reuse.paste(im.resize((size, size), Image.Resampling.LANCZOS), (left, top+34))
    draw.text((margin, reuse.height-44), "这是有限窗口中的重铺证据，不是严格非周期性证明。", font=font[20], fill=MUTED)
    reuse.save(output/"reuse.png")
    product = next(case for case in cases if case["relation"] == "product")
    composition = Image.new("RGB", (margin*2+3*size+2*gap, 584), BACKGROUND)
    draw = ImageDraw.Draw(composition)
    draw.text((margin, 18), "格纹对照：更小的场值误差，不等于保留交叉结构", font=font[25], fill=INK)
    draw.text((margin, 61), "左：逐模态闭合后相乘；中：直接拟合乘积；右：分别拟合通道总场，再相乘。", font=font[20], fill=MUTED)
    for col, (name, title) in enumerate((("closed_A", "旧方法 / 保留乘积关系"),
                                        ("joint_A", "总场拟合 / 不约束乘积关系"),
                                        ("factorized_A", "通道总场拟合 / 保留乘积关系"))):
        left = margin+col*(size+gap)
        draw.text((left, 111), title, font=font[20], fill=INK)
        with Image.open(output/product["id"]/f"{name}.png") as im:
            composition.paste(im.resize((size, size), Image.Resampling.LANCZOS), (left, 148))
    draw.text((margin, 548), "同一布局、类型预算和采样。保留乘积代数关系仍不等于证明所有交点数目或位置不变。", font=font[17], fill=MUTED)
    composition.save(output/"composition.png")


def design_boards(output, cases, font_path):
    font = fonts(font_path)
    size, gap, margin = 576, 24, 30
    for name, title, subtitle in (
        ("raw", "新的 QRP 测试源纹样", "未铺砌的全局参考；不同方向数与高度分层，不再只显示零阈值正负分区。"),
        ("joint_A", "新的测试纹样 · 共享边界 Wang 铺砌", "每组固定 16 类内容库，展示 2×2 合法布局；图像直接来自数值场，无接缝后处理。")):
        board = Image.new("RGB", (2*margin+3*size+2*gap, 770), BACKGROUND)
        draw = ImageDraw.Draw(board)
        draw.text((margin, 18), title, font=font[34], fill=INK)
        draw.text((margin, 70), subtitle, font=font[20], fill=MUTED)
        for col, case in enumerate(cases):
            left = margin+col*(size+gap)
            draw.text((left, 118), f"{case['style']}  /  q={case['channels'][0]['q']}", font=font[25], fill=INK)
            with Image.open(output/case["id"]/f"{name}.png") as im:
                board.paste(im.resize((size, size), Image.Resampling.LANCZOS), (left, 163))
        board.save(output/("source_gallery.png" if name == "raw" else "gallery.png"))
    columns = [("raw", "原始 QRP 参考"), ("closed_A", "逐模态闭合"), ("joint_A", "共享边界拟合")]
    size = 448
    board = Image.new("RGB", (2*margin+3*size+2*gap, 115+3*(size+60)+55), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "同一设计输入与显示方式，比较铺砌造成的改变", font=font[25], fill=INK)
    draw.text((margin, 65), "三列使用相同高度阈值、色序和采样；原始参考不与局部目标逐点对齐。", font=font[20], fill=MUTED)
    for row, case in enumerate(cases):
        for col, (name, title) in enumerate(columns):
            left, top = margin+col*(size+gap), 115+row*(size+60)
            draw.text((left, top), f"{case['style']} · {title}", font=font[20], fill=INK)
            with Image.open(output/case["id"]/f"{name}.png") as im:
                board.paste(im.resize((size, size), Image.Resampling.LANCZOS), (left, top+38))
    draw.text((margin, board.height-44), "设计样例不是风格保真的证明。零阈值旧案例仍作为历史诊断保留。", font=font[20], fill=MUTED)
    board.save(output/"comparison.png")


def run_design(args, manifest, manifest_path):
    edges = manifest["tile_edges"]
    assert edges == [list(endpoint_edges(tile)) for tile in range(16)]
    size, pixels = manifest["grid_size"], manifest["pixels_per_tile"]
    layouts = {key: np.array(data["ids"]).reshape(size, size) for key, data in manifest["layouts"].items()}
    assert all(mismatches(layout, edges) == 0 for layout in layouts.values())
    scale = manifest["source_span"]*manifest["cases"][0]["channels"][0]["frequency"]
    print("Fitting the new QRP design cases...", flush=True)
    projection = Projection(args.cells, edges, scale, shared=True)
    x, y = sample_grid(pixels*2)
    vx, vy = sample_grid(96)
    report = {"cells": args.cells, "source_manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
              "grid_size": size, "pixels_per_tile": pixels, "source_span": manifest["source_span"],
              "supersampling": 2, "shared_dofs": projection.count, "cases": {}}
    args.output.mkdir(parents=True, exist_ok=True)
    for case in manifest["cases"]:
        print(case["id"], flush=True)
        oracle_error = 0.0
        for oracle in case["cpp_oracle"]:
            for kind in ("raw", "closed"):
                actual = source_field(case, oracle["tile"], *np.array(oracle["xy"]), kind)
                oracle_error = max(oracle_error, float(np.max(np.abs(actual-oracle[kind]))))
        assert oracle_error < 1e-10
        coefficients, residual = projection.fit(case)
        directory = args.output/case["id"]
        (directory/"tiles").mkdir(parents=True, exist_ok=True)
        np.savez_compressed(directory/"library.npz", coefficients=coefficients, mapping=projection.mapping)
        libraries = {name: [] for name in ("target", "closed", "joint")}
        sampled = {name: [] for name in libraries}
        for tile in range(16):
            for name in libraries:
                evaluate = (lambda a, b: projection.evaluate(coefficients, tile, a, b)) if name == "joint" else (
                    lambda a, b: source_field(case, tile, a, b, name))
                libraries[name].append(design_raster(evaluate(x, y)[..., 0], case["render"]))
                sampled[name].append(evaluate(vx, vy))
            Image.fromarray(libraries["joint"][-1]).save(directory/"tiles"/f"tile_{tile}.png")
        for name, library in libraries.items():
            for key, layout in layouts.items():
                im, exact = assemble(layout, library)
                assert exact
                im.save(directory/f"{name}_{key}.png")
        rx, ry = sample_grid(pixels*2*size)
        # 全局参考居中；局部目标和铺砌不进行此全局坐标求值。
        raw = source_field(case, 0, rx*size-(size-1)/2, ry*size-(size-1)/2, "raw")
        Image.fromarray(design_raster(raw[..., 0], case["render"])).save(directory/"raw.png")
        target = np.array(sampled["target"])
        errors = {}
        for name in ("closed", "joint"):
            actual = np.array(sampled[name])
            errors[name] = error_metrics(actual, target, scale)
            errors[name]["display_color_disagreement"] = float(np.mean(np.any(
                design_colors(actual[..., 0], case["render"]) != design_colors(target[..., 0], case["render"]), axis=-1)))
        report["cases"][case["id"]] = {
            "style": case["style"], "q": case["channels"][0]["q"], "render": case["render"],
            "cpp_value_gradient_oracle_max_error": oracle_error,
            "normal_equation_relative_residual": residual, "errors_to_unclosed_target": errors,
            "pixel_identical_reuse_blocks": 2*size*size,
            **boundary_checks(case, lambda tile, a, b: projection.evaluate(coefficients, tile, a, b), edges)}
        print(json.dumps(report["cases"][case["id"]], ensure_ascii=False), flush=True)
    (args.output/"results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    design_boards(args.output, manifest["cases"], args.font)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT/"tools/qrp_design_cases.json")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--cells", type=int)
    parser.add_argument("--font", type=Path, default=Path("C:/Windows/Fonts/msyh.ttc"))
    args = parser.parse_args()
    manifest_path = args.source if args.source.is_file() else args.source/"manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    is_design = manifest.get("presentation") == "design"
    args.cells = args.cells or (28 if is_design else 12)
    args.output = args.output or ROOT/("output/qrp-design-tile-study" if is_design else "output/qrp-joint-field-study")
    if is_design:
        run_design(args, manifest, manifest_path)
        return
    assert manifest["model"] == "qrp-phase-compatible-tiles-v1"
    assert manifest["tile_count"] == 16 and manifest["layout_row_order"] == "bottom-up"
    edges = manifest["tile_edges"]
    assert edges == [list(endpoint_edges(tile)) for tile in range(16)]
    cases = manifest["cases"]
    scale = manifest["source_span"]*cases[0]["channels"][0]["frequency"]
    assert all(channel["frequency"]*manifest["source_span"] == scale
               for case in cases for channel in case["channels"])
    layouts = {key: np.array(layout["ids"]).reshape(manifest["grid_size"], -1)[:4, :4]
               for key, layout in manifest["layouts"].items()}
    assert all(mismatches(layout, edges) == 0 for layout in layouts.values())
    args.output.mkdir(parents=True, exist_ok=True)
    print("Assembling constrained and unconstrained projection operators...", flush=True)
    joint = Projection(args.cells, edges, scale, shared=True)
    free = Projection(args.cells, edges, scale, shared=False)
    basis_error = check_basis(free)
    pixels = manifest["pixels_per_tile"]
    x, y = sample_grid(pixels*2)
    # 独立于四点 Gauss 拟合网格的验证点；不以训练残差充当保真度。
    vx, vy = sample_grid(pixels)
    palette = [manifest["ink"], manifest["paper"]]
    report = {"model": "joint-final-field-hermite-probe-v1", "cells": args.cells,
              "source_manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
              "source_directory": str(args.source.resolve()), "wave_scale": scale,
              "shared_dofs": joint.count, "unconstrained_dofs": free.count,
              "bicubic_value_gradient_check_max_error": basis_error,
              "layout_ids_bottom_up": {key: value.tolist() for key, value in layouts.items()},
              "cases": {}}
    for case in cases:
        print(f"Fitting {case['id']}...", flush=True)
        directory = args.output/case["id"]
        (directory/"tiles").mkdir(parents=True, exist_ok=True)
        coefficients, residual = joint.fit(case)
        free_coefficients, free_residual = free.fit(case)
        np.savez_compressed(directory/"library.npz", coefficients=coefficients, mapping=joint.mapping)
        tiles = {name: [] for name in ("target", "closed", "joint")}
        sampled = {name: [] for name in ("target", "closed", "joint", "free")}
        cpp_error = 0
        for tile in range(16):
            for name in ("target", "closed"):
                tiles[name].append(raster(source_field(case, tile, x, y, name)[..., 0], palette))
                sampled[name].append(source_field(case, tile, vx, vy, name))
            value = joint.evaluate(coefficients, tile, x, y)
            tiles["joint"].append(raster(value[..., 0], palette))
            Image.fromarray(tiles["joint"][-1]).save(directory/"tiles"/f"tile_{tile}.png")
            sampled["joint"].append(joint.evaluate(coefficients, tile, vx, vy))
            sampled["free"].append(free.evaluate(free_coefficients, tile, vx, vy))
            with Image.open(args.source/case["id"]/"tiles"/f"tile_{tile}.png") as im:
                cpp_error = max(cpp_error, int(np.max(np.abs(np.asarray(im.convert("RGB")).astype(int)
                                                             - tiles["closed"][-1].astype(int)))))
        assert cpp_error == 0, f"Serialized-mode evaluation differs from C++ PNG by {cpp_error}"
        for name, library in tiles.items():
            for key, layout in layouts.items():
                im, exact = assemble(layout, library)
                assert exact
                im.save(directory/f"{name}_{key}.png")
        rx, ry = sample_grid(pixels*2*4)
        raw = raster(source_field(case, 0, 4*rx, 4*ry, "raw")[..., 0], palette)
        Image.fromarray(raw).save(directory/"raw.png")
        target = np.array(sampled["target"])
        result = {"normal_equation_relative_residual": residual,
                  "free_normal_equation_relative_residual": free_residual,
                  "cpp_closed_png_max_channel_error": cpp_error,
                  "pixel_identical_reuse_blocks": 2*4*4,
                  "errors_to_unclosed_target": {name: error_metrics(np.array(sampled[name]), target, scale)
                                                for name in ("closed", "joint", "free")},
                  **boundary_checks(case, lambda tile, x, y: joint.evaluate(coefficients, tile, x, y), edges)}
        if case["relation"] == "product":
            channel_coefficients = [joint.fit({"relation": "direct", "channels": [channel]})[0]
                                    for channel in case["channels"]]

            def factorized(tile, x, y):
                return multiply_fields(*(joint.evaluate(c, tile, x, y) for c in channel_coefficients))

            library, samples = [], []
            (directory/"factorized_tiles").mkdir(exist_ok=True)
            for tile in range(16):
                library.append(raster(factorized(tile, x, y)[..., 0], palette))
                samples.append(factorized(tile, vx, vy))
                Image.fromarray(library[-1]).save(directory/"factorized_tiles"/f"tile_{tile}.png")
            np.savez_compressed(directory/"factorized_library.npz",
                                coefficients=np.array(channel_coefficients), mapping=joint.mapping)
            for key, layout in layouts.items():
                im, exact = assemble(layout, library)
                assert exact
                im.save(directory/f"factorized_{key}.png")
            result["factorized_projection"] = {
                "errors_to_unclosed_target": error_metrics(np.array(samples), target, scale),
                "pixel_identical_reuse_blocks": 32,
                **boundary_checks(case, factorized, edges)}
        report["cases"][case["id"]] = result
        print(json.dumps({case["id"]: result}, ensure_ascii=False), flush=True)
    (args.output/"results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    make_boards(args.output, cases, args.font)
    print(f"Figures: {args.output.resolve()}")


if __name__ == "__main__":
    main()
