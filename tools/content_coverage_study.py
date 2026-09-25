"""Content-coverage/type-assignment comparison using one shared QRP projection.

Defaults reproduce the 2026-09-25 experiment; phase transfer freezes the zero-phase assignment.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw
from board_common import BACKGROUND, INK, MUTED, fonts
from joint_field import (Projection, source_field, sample_grid, design_raster,
                         design_colors, error_metrics, boundary_checks)
from wang_tiles import assemble, make_layout, mismatches

ROOT = Path(__file__).resolve().parents[1]


def traces(case, count):
    axis = np.linspace(0, 1, count)
    zero, one = np.zeros_like(axis), np.ones_like(axis)
    return np.array([source_field(case, 0, x, y, "raw") for x, y in
                     ((axis, zero), (axis, one), (zero, axis), (one, axis))])


def assign_sources(pool, edges):
    # 同色边的 (f, fu/k, fv/k) 方差。包含同一瓦片上同色的相对两边，不遗漏自重复约束。
    features = np.array([traces(case, 65) for case in pool])
    weights = np.ones(65)/64
    weights[[0, -1]] *= .5
    features *= np.sqrt(weights)[None, None, :, None]*np.array([1, 1/32, 1/32])
    groups = edges+np.array([0, 0, 4, 4])
    slots = [np.where(groups == color) for color in range(8)]

    def energy(assignment):
        result = 0.0
        for tiles, sides in slots:
            f = features[assignment[tiles], sides]
            result += np.sum((f-f.mean(axis=0))**2)
        return float(result/64)

    assignment = np.arange(16)
    history = [energy(assignment)]
    while True:
        best, best_value = None, history[-1]
        for a in range(16):
            for b in range(a+1, 16):
                candidate = assignment.copy()
                candidate[a], candidate[b] = candidate[b], candidate[a]
                value = energy(candidate)
                if value < best_value-1e-12:
                    best, best_value = candidate, value
        if best is None:
            break
        assignment = best
        history.append(best_value)
    assert np.array_equal(np.sort(assignment), np.arange(16))
    return assignment, {"objective": "mean within-label squared H1 trace deviation",
                        "quadrature_points": 65, "initial_assignment": list(range(16)),
                        "history": history, "pair_swap_local_optimum_only": True}


def coverage(samples, interior):
    # 固定坐标上的内容变化，不是感知风格数，也不消除旋转/平移等价。
    matrix = samples[..., 0][:, interior]
    centered = matrix-matrix.mean(axis=0)
    eigen = np.maximum(np.linalg.eigvalsh(centered@centered.T/matrix.shape[1]), 0)
    return {"interior_between_type_rms": float(np.sqrt(np.mean(centered**2))),
            "interior_effective_rank": float(eigen.sum()**2/np.sum(eigen**2))}


def lag_correlation(samples, layout):
    field = np.block([[samples[int(tile), ..., 0] for tile in row] for row in layout[::-1]])
    field -= field.mean()
    step = samples.shape[1]
    result = []
    for a, b in ((field[:, :-step], field[:, step:]), (field[:-step, :], field[step:, :])):
        result.append(float(np.sum(a*b)/np.sqrt(np.sum(a*a)*np.sum(b*b))))
    return result


def layout_seam(projection, coefficients, layout):
    s = np.linspace(0, 1, 129)
    peak = np.zeros(3)
    for y, x in np.ndindex(layout.shape):
        a = int(layout[y, x])
        if x+1 < layout.shape[1]:
            b = int(layout[y, x+1])
            delta = projection.evaluate(coefficients, a, np.ones_like(s), s)-projection.evaluate(coefficients, b, np.zeros_like(s), s)
            peak = np.maximum(peak, np.max(np.abs(delta), axis=0))
        if y+1 < layout.shape[0]:
            b = int(layout[y+1, x])
            delta = projection.evaluate(coefficients, a, s, np.ones_like(s))-projection.evaluate(coefficients, b, s, np.zeros_like(s))
            peak = np.maximum(peak, np.max(np.abs(delta), axis=0))
    return peak.tolist()


def boards(output, base, transfer, zero_phase_output, font_path):
    font = fonts(font_path)
    size, margin, gap = 512, 28, 24
    board = Image.new("RGB", (2*margin+3*size+2*gap, 160+3*(size+55)+55), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "内容覆盖与 QRP 控制，能否同时保留下来？" if transfer else "同一 QRP 设计，怎样扩大 Wang 库的内容覆盖？", font=font[34], fill=INK)
    draw.text((margin, 72), "后两列固定同一源块位置与类型分配，只改 QRP 相位；每列各自生成一套可复用内容库。" if transfer else "每列 16 类、同一 2×2 布局和显示配方；后两列使用同一批源块，仅分配方式不同。", font=font[20], fill=MUTED)
    titles = ([("baseline", "原库 / A=B=0"), ("matched", "覆盖库 / A=B=0"), ("matched", "覆盖库 / A=0.8，B=0.4")] if transfer else
              [("baseline", "原库 / 同一区域轻微形变"), ("naive", "覆盖库 / 直接分配源块"), ("matched", "覆盖库 / 按同色边内容分配")])
    for col, (_, title) in enumerate(titles):
        draw.text((margin+col*(size+gap), 119), title, font=font[25], fill=INK)
    for row, case in enumerate(base["cases"]):
        q = case["channels"][0]["q"]
        for col, (name, _) in enumerate(titles):
            left, top = margin+col*(size+gap), 170+row*(size+55)
            draw.text((left, top), f"{case['style']} / q={q}", font=font[20], fill=INK)
            directory = zero_phase_output if transfer and col < 2 else output
            with Image.open(directory/f"q{q}_{name}"/"joint_A.png") as im:
                board.paste(im.resize((size, size), Image.Resampling.LANCZOS), (left, top+34))
    draw.text((margin, board.height-42), "均为共享边界拟合后的实际铺砌；内容覆盖和接缝代价分开测量，不把低相关直接当作更美。", font=font[20], fill=MUTED)
    board.save(output/"comparison.png")
    if transfer:
        return
    board = Image.new("RGB", (2*margin+3*size+2*gap, 145+3*(size+55)+50), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "同一批 QRP 源块：拟合究竟改变了哪里？", font=font[34], fill=INK)
    draw.text((margin, 73), "原始源块有接缝；中、右只用同一套固定的相容库重铺。色带、阈值和采样完全一致。", font=font[20], fill=MUTED)
    for row, case in enumerate(base["cases"]):
        q = case["channels"][0]["q"]
        for col, (name, title) in enumerate((("target_A", "匹配分配 / 未拟合目标"), ("joint_A", "同一分配 / 合法布局 A"), ("joint_B", "同一内容库 / 合法布局 B"))):
            left, top = margin+col*(size+gap), 130+row*(size+55)
            draw.text((left, top), f"q={q} · {title}", font=font[20], fill=INK)
            with Image.open(output/f"q{q}_matched"/f"{name}.png") as im:
                board.paste(im.resize((size, size), Image.Resampling.LANCZOS), (left, top+34))
    board.save(output/"target_and_reuse.png")
    board = Image.new("RGB", (2*margin+3*size+2*gap, 145+3*(size+55)+55), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 20), "较大窗口：相同类型像素在不同位置直接复用", font=font[34], fill=INK)
    draw.text((margin, 72), "后两列瓦片数量完全相同；第三列只打乱位置，用于检查匹配规则是否真的约束内容。", font=font[20], fill=MUTED)
    for row, case in enumerate(base["cases"]):
        q = case["channels"][0]["q"]
        for col, (method, name, title) in enumerate((("baseline", "joint_wide_A", "原库 / 合法"),
                ("matched", "joint_wide_A", "覆盖库 / 合法"), ("matched", "shuffled", "覆盖库 / 非法打乱"))):
            left, top = margin+col*(size+gap), 130+row*(size+55)
            draw.text((left, top), f"q={q} · {title}", font=font[20], fill=INK)
            with Image.open(output/f"q{q}_{method}"/f"{name}.png") as im:
                board.paste(im.resize((size, size), Image.Resampling.LANCZOS), (left, top+34))
    draw.text((margin, board.height-42), "有限窗口只检查重复感与重铺；没有声称证明严格非周期性。", font=font[20], fill=MUTED)
    board.save(output/"wide_and_labels.png")


def write_manifest(output, base, phase, cases, pools, layouts, centers):
    # 每批源模态只存一次；类型分配只存索引，不把旧的全局核对值误写成逐类型核对值。
    serialized = []
    for case in cases:
        if "tile_sources" not in case:
            serialized.append(case)
            continue
        q = case["tile_sources"][0]["channels"][0]["q"]
        serialized.append({"id": case["id"], "q": q, "style": case["style"], "render": case["render"],
                           "source_pool": str(q), "source_assignment": case["source_assignment"]})
    manifest = {**base, "model": "qrp-content-coverage-probe-v1", "presentation": "coverage",
                "source_provenance": "Same QRP designs; source patches exported from qrp_export_sources.",
                "source_centers": centers, "global_phase": phase, "source_pools": pools,
                "wide_layouts": {key: value.tolist() for key, value in layouts.items() if key.startswith("wide")},
                "cases": serialized}
    (output/"manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase-transfer", action="store_true")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--zero-phase-output", type=Path, default=ROOT/"output/qrp-content-coverage-study")
    parser.add_argument("--exporter", type=Path, default=ROOT/("build/Release/qrp_export_sources.exe" if sys.platform == "win32" else "build/qrp_export_sources"))
    parser.add_argument("--font", type=Path, default=Path("C:/Windows/Fonts/msyh.ttc"))
    args = parser.parse_args()
    TRANSFER = args.phase_transfer
    PHASE = (.8, .4) if TRANSFER else (0, 0)
    OUTPUT = args.output or ROOT/("output/qrp-content-coverage-phase-transfer" if TRANSFER else "output/qrp-content-coverage-study")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    BASE_PATH = ROOT/"tools/qrp_design_cases.json"
    BASE = json.loads(BASE_PATH.read_text(encoding="utf-8"))
    EDGES = np.array(BASE["tile_edges"])
    EXPORTER = args.exporter.resolve()
    if not EXPORTER.is_file():
        raise FileNotFoundError(f"Build qrp_export_sources first: {EXPORTER}")
    CENTERS = [(16*x, 16*y) for y in range(4) for x in range(4)]
    LAYOUTS = {key: np.array(value["ids"]).reshape(2, 2) for key, value in BASE["layouts"].items()}
    LAYOUTS.update(wide_A=make_layout(20260925, 4), wide_B=make_layout(20260926, 4))
    assert all(mismatches(layout, EDGES) == 0 for layout in LAYOUTS.values())
    SHUFFLED = np.random.default_rng(20260927).permutation(LAYOUTS["wide_A"].ravel()).reshape(4, 4)
    assert mismatches(SHUFFLED, EDGES) > 0
    assert np.array_equal(np.sort(SHUFFLED.ravel()), np.sort(LAYOUTS["wide_A"].ravel()))



    projection = Projection(28, EDGES, 32, shared=True)
    x, y = sample_grid(768)
    vx, vy = sample_grid(96)
    interior = (vx >= .15) & (vx <= .85) & (vy >= .15) & (vy <= .85)
    cases, pools, responses = [], {}, {}
    if TRANSFER:
        zero_manifest = json.loads((args.zero_phase_output/"manifest.json").read_text(encoding="utf-8"))
    report = {"source_manifest_sha256": hashlib.sha256(BASE_PATH.read_bytes()).hexdigest(),
              "global_phase": PHASE,
              "cells": 28, "source_span": 16, "wave_scale": 32, "source_centers": CENTERS,
              "pixels_per_tile": 384, "supersampling": 2, "layouts": {k: v.tolist() for k, v in LAYOUTS.items()},
              "shuffled_wide_A": SHUFFLED.tolist(), "shuffled_label_mismatches": mismatches(SHUFFLED, EDGES),
              "source_assignment": {}, "cases": {}}

    for baseline in BASE["cases"]:
        q = baseline["channels"][0]["q"]
        print(f"Exporting q={q} source pool", flush=True)
        pool = []
        for i, (cx, cy) in enumerate(CENTERS):
            data = json.loads(subprocess.check_output([str(EXPORTER), str(PHASE[0]), str(PHASE[1]), str(2 if q == 5 else 3), str(cx-8), str(cy-8)], text=True))
            patch = next(c for c in data if c["channels"][0]["q"] == q)
            patch["id"] = f"q{q}_patch_{i}"
            patch["source_center"] = [cx, cy]
            pool.append(patch)
        pools[str(q)] = pool
        oracle_error = max(float(np.max(np.abs(source_field(p, o["tile"], *np.array(o["xy"]), kind)-o[kind])))
            for p in pool for o in p["cpp_oracle"] for kind in ("raw", "closed"))
        assert oracle_error < 1e-10
        if TRANSFER:
            assignment_path = args.zero_phase_output/"results.json"
            assignment_data = json.loads(assignment_path.read_text(encoding="utf-8"))
            assignment = np.array(assignment_data["source_assignment"][str(q)]["assignment"])
            optimization = {"frozen_zero_phase_assignment": True,
                            "assignment_report_sha256": hashlib.sha256(assignment_path.read_bytes()).hexdigest()}
        else:
            assignment, optimization = assign_sources(pool, EDGES)
        report["source_assignment"][str(q)] = {**optimization, "assignment": assignment.tolist(),
                                             "cpp_value_gradient_oracle_max_error": oracle_error}
        print(f"q={q} assignment: {assignment.tolist()}; {'frozen' if TRANSFER else optimization['history']}", flush=True)
        source_images = [design_raster(source_field(p, 0, x, y, "raw")[..., 0], baseline["render"]) for p in pool]

        for name, ordering in (("baseline", None), ("naive", np.arange(16)), ("matched", assignment)):
            if TRANSFER and name != "matched":
                continue
            case = {**baseline, "id": f"q{q}_{name}"}
            if ordering is not None:
                case["tile_sources"] = [pool[i] for i in ordering]
                case["source_assignment"] = ordering.tolist()
            cases.append(case)
            directory = OUTPUT/case["id"]
            (directory/"tiles").mkdir(parents=True, exist_ok=True)
            print(f"Fitting/rendering {case['id']}", flush=True)
            coefficients, residual = projection.fit(case)
            library = [design_raster(projection.evaluate(coefficients, t, x, y)[..., 0], case["render"]) for t in range(16)]
            targets = ([design_raster(source_field(case, t, x, y, "target")[..., 0], case["render"]) for t in range(16)]
                       if ordering is None else [source_images[i] for i in ordering])
            np.savez_compressed(directory/"library.npz", coefficients=coefficients, mapping=projection.mapping)
            for t, image in enumerate(library):
                Image.fromarray(image).save(directory/"tiles"/f"tile_{t}.png")
            for key, ids in LAYOUTS.items():
                for kind, images in (("joint", library), ("target", targets)):
                    image, exact = assemble(ids, images)
                    assert exact
                    image.save(directory/f"{kind}_{key}.png")
            image, exact = assemble(SHUFFLED, library)
            assert exact
            image.save(directory/"shuffled.png")
            target = np.array([source_field(case, t, vx, vy, "target") for t in range(16)])
            actual = np.array([projection.evaluate(coefficients, t, vx, vy) for t in range(16)])
            color_different = np.any(design_colors(actual[..., 0], case["render"]) != design_colors(target[..., 0], case["render"]), axis=-1)
            result = {"normal_equation_relative_residual": residual,
                "errors_to_target": error_metrics(actual, target, 32),
                "interior_errors_to_target": error_metrics(actual[:, interior], target[:, interior], 32),
                "boundary_zone_errors_to_target": error_metrics(actual[:, ~interior], target[:, ~interior], 32),
                "display_color_disagreement": float(color_different.mean()),
                "interior_display_color_disagreement": float(color_different[:, interior].mean()),
                "target_coverage": coverage(target, interior), "fitted_coverage": coverage(actual, interior),
                "wide_A_one_tile_lag_correlation_xy": lag_correlation(actual, LAYOUTS["wide_A"]),
                "pixel_identical_legal_reuse_blocks": sum(ids.size for ids in LAYOUTS.values()),
                "shuffled_sampled_max_value_dx_dy": layout_seam(projection, coefficients, SHUFFLED),
                **boundary_checks(case, lambda t, a, b: projection.evaluate(coefficients, t, a, b), EDGES)}
            report["cases"][case["id"]] = result
            if TRANSFER:
                zero_case = next(c for c in zero_manifest["cases"] if c["id"] == case["id"])
                assert zero_case["source_assignment"] == ordering.tolist()
                zero_pool = zero_manifest["source_pools"][str(q)]
                zero_source = {"tile_sources": [zero_pool[i] for i in ordering]}
                zero_target = np.array([source_field(zero_source, t, vx, vy, "target") for t in range(16)])
                with np.load(args.zero_phase_output/case["id"]/"library.npz") as saved:
                    assert np.array_equal(saved["mapping"], projection.mapping)
                    zero_actual = np.array([projection.evaluate(saved["coefficients"], t, vx, vy) for t in range(16)])
                dg = (target-zero_target)[..., 0].ravel()
                dt = (actual-zero_actual)[..., 0].ravel()
                responses[str(q)] = {
                    "target_response_rms": float(np.sqrt(np.mean(dg*dg))),
                    "tile_response_rms": float(np.sqrt(np.mean(dt*dt))),
                    "norm_gain": float(np.linalg.norm(dt)/np.linalg.norm(dg)),
                    "cosine_similarity": float(np.dot(dg, dt)/np.linalg.norm(dg)/np.linalg.norm(dt)),
                    "relative_l2_error": float(np.linalg.norm(dt-dg)/np.linalg.norm(dg))}
            print(json.dumps({case["id"]: result}), flush=True)
        # 分配只能重排同一批原始内容：集合层面的目标覆盖严格不变。
        if not TRANSFER:
            assert np.isclose(report["cases"][f"q{q}_naive"]["target_coverage"]["interior_between_type_rms"],
                              report["cases"][f"q{q}_matched"]["target_coverage"]["interior_between_type_rms"], atol=1e-14, rtol=0)
        (OUTPUT/"results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")

    write_manifest(OUTPUT, BASE, PHASE, cases, pools, LAYOUTS, CENTERS)
    if TRANSFER:
        (OUTPUT/"phase_response.json").write_text(json.dumps(responses, indent=2)+"\n", encoding="utf-8")
    boards(OUTPUT, BASE, TRANSFER, args.zero_phase_output, args.font)
    print(f"Evidence: {OUTPUT}", flush=True)


if __name__ == "__main__":
    main()
