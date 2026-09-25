"""QRP phase-response comparison; fixed approved designs and shared projection.

Runs from exported C++ sources, without reusing stale coefficient/image caches.
"""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import numpy as np
from PIL import Image, ImageDraw
from joint_field import (Projection, boundary_checks, source_field, sample_grid,
                         design_raster, error_metrics, design_colors)
from wang_tiles import assemble, mismatches
from board_common import fonts, BACKGROUND, INK, MUTED

ROOT = Path(__file__).resolve().parents[1]


def image_board(output, cases, font_path, name, source_name, title):
    font = fonts(font_path)
    size, gap, margin = 448, 22, 28
    board = Image.new("RGB", (2*margin+3*size+2*gap, 172+3*(size+56)+46), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((margin, 18), title, font=font[34], fill=INK)
    draw.text((margin, 69), "固定方向数、配色、尺度及 16 类预算；q=5 用二阶相位，q=8/12 用三阶相位。", font=font[20], fill=MUTED)
    for col, label in enumerate(("A=0，B=0 / 已认可基线", "A=0.8，B=0", "A=0.8，B=0.4 / 混合相位")):
        draw.text((margin+col*(size+gap), 115), label, font=font[25], fill=INK)
    for row, q in enumerate((5, 8, 12)):
        for col, state in enumerate(("base", "phase_a", "mixed")):
            case = next(c for c in cases if c["id"] == f"q{q}_{state}")
            left, top = margin+col*(size+gap), 163+row*(size+56)
            draw.text((left, top), f"{case['style']} / q={q}", font=font[20], fill=INK)
            with Image.open(output/case["id"]/f"{source_name}.png") as image:
                board.paste(image.resize((size, size), Image.Resampling.LANCZOS), (left, top+34))
    draw.text((margin, board.height-42), "各列是一套不同设计参数的库；这不等于同一库内部的母题重复已解决。", font=font[20], fill=MUTED)
    board.save(output/name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT/"output/qrp-phase-response-study")
    parser.add_argument("--exporter", type=Path, default=ROOT/("build/Release/qrp_export_sources.exe" if sys.platform == "win32" else "build/qrp_export_sources"))
    parser.add_argument("--font", type=Path, default=Path("C:/Windows/Fonts/msyh.ttc"))
    args = parser.parse_args()
    OUTPUT = args.output
    OUTPUT.mkdir(parents=True, exist_ok=True)
    BASE_PATH = ROOT/"tools/qrp_design_cases.json"
    BASE = json.loads(BASE_PATH.read_text(encoding="utf-8"))
    EXPORTER = args.exporter.resolve()
    if not EXPORTER.is_file():
        raise FileNotFoundError(f"Build qrp_export_sources first: {EXPORTER}")
    cache = {}

    def exported(q, a, b, order):
        key = (a, b, order)
        if key not in cache:
            cache[key] = json.loads(subprocess.check_output(
                [str(EXPORTER), str(a), str(b), str(order)], text=True))
        return next(c for c in cache[key] if c["channels"][0]["q"] == q)


    edges = BASE["tile_edges"]
    layouts = {key: np.array(value["ids"]).reshape(2, 2) for key, value in BASE["layouts"].items()}
    assert all(mismatches(ids, edges) == 0 for ids in layouts.values())
    projection = Projection(28, edges, 32, shared=True)
    x, y = sample_grid(768)
    vx, vy = sample_grid(96)
    rx, ry = sample_grid(1536)
    cases, report = [], {"cells": 28, "wave_scale": 32, "pixels_per_tile": 384,
        "supersampling": 2, "baseline_manifest_sha256": hashlib.sha256(BASE_PATH.read_bytes()).hexdigest(),
        "cases": {}, "phase_basis_checks": {}}
    for baseline in BASE["cases"]:
        q = baseline["channels"][0]["q"]
        order = 2 if q == 5 else 3
        previous = None
        for state, a, b in (("base", 0, 0), ("phase_a", .8, 0), ("phase_b", 0, .8), ("mixed", .8, .4)):
            case = {**exported(q, a, b, order), "id": f"q{q}_{state}",
                    "style": baseline["style"], "render": baseline["render"]}
            cases.append(case)
            if state == "base":
                assert case["channels"][0]["modes"] == baseline["channels"][0]["modes"]
            oracle_error = max(float(np.max(np.abs(source_field(case, o["tile"], *np.array(o["xy"]), kind)-o[kind])))
                               for o in case["cpp_oracle"] for kind in ("raw", "closed"))
            assert oracle_error < 1e-10
            directory = OUTPUT / case["id"]
            (directory/"tiles").mkdir(parents=True, exist_ok=True)
            print(f"Fitting {case['id']}", flush=True)
            coefficients, residual = projection.fit(case)
            np.savez_compressed(directory/"library.npz", coefficients=coefficients, mapping=projection.mapping)
            target = np.array([source_field(case, tile, vx, vy, "target") for tile in range(16)])
            actual = np.array([projection.evaluate(coefficients, tile, vx, vy) for tile in range(16)])
            library = []
            for tile in range(16):
                rgb = design_raster(projection.evaluate(coefficients, tile, x, y)[..., 0], case["render"])
                library.append(rgb)
                Image.fromarray(rgb).save(directory/"tiles"/f"tile_{tile}.png")
            for key, ids in layouts.items():
                image, exact = assemble(ids, library)
                assert exact
                image.save(directory/f"joint_{key}.png")
            raw = source_field(case, 0, rx*2-.5, ry*2-.5, "raw")
            Image.fromarray(design_raster(raw[..., 0], case["render"])).save(directory/"raw.png")
            result = {"q": q, "phase_harmonic_order": order, "phase_a": a, "phase_b": b,
                "cpp_value_gradient_oracle_max_error": oracle_error,
                "normal_equation_relative_residual": residual,
                "errors_to_unclosed_target": error_metrics(actual, target, 32),
                "display_color_disagreement": float(np.mean(np.any(design_colors(actual[..., 0], case["render"])
                    != design_colors(target[..., 0], case["render"]), axis=-1))),
                "pixel_identical_reuse_blocks": 8,
                **boundary_checks(case, lambda tile, a, b: projection.evaluate(coefficients, tile, a, b), edges)}
            if previous is None:
                previous = (target, actual)
            else:
                dt, dj = (target-previous[0])[..., 0].ravel(), (actual-previous[1])[..., 0].ravel()
                result["phase_response_to_baseline"] = {
                    "target_value_rms": float(np.sqrt(np.mean(dt*dt))),
                    "joint_value_rms": float(np.sqrt(np.mean(dj*dj))),
                    "norm_gain": float(np.linalg.norm(dj)/np.linalg.norm(dt)),
                    "cosine_similarity": float(np.dot(dt, dj)/np.linalg.norm(dt)/np.linalg.norm(dj)),
                    "relative_l2_error": float(np.linalg.norm(dj-dt)/np.linalg.norm(dt))}
            report["cases"][case["id"]] = result
            print(json.dumps(result), flush=True)

        # 同一取样窗口内比较原始场的 A/B 局部响应；模式仍从 C++ 重导出，不从 q 重造。
        checks = {}
        for harmonic in sorted({2, order}):
            columns = []
            eps = 1e-3
            for da, db in ((eps, 0), (0, eps)):
                plus, minus = exported(q, da, db, harmonic), exported(q, -da, -db, harmonic)
                columns.append(((source_field(plus, 0, vx, vy, "raw")[..., 0]
                                 -source_field(minus, 0, vx, vy, "raw")[..., 0])/(2*eps)).ravel())
            singular = np.linalg.svd(np.stack(columns, axis=1), compute_uv=False)/np.sqrt(vx.size)
            checks[str(harmonic)] = {"sampled_phase_jacobian_singular_values_rms": singular.tolist()}
        report["phase_basis_checks"][str(q)] = checks

    image_board(OUTPUT, cases, args.font, "gallery.png", "joint_A", "QRP 相位变化能否保留到 Wang 铺砌？")
    image_board(OUTPUT, cases, args.font, "source_gallery.png", "raw", "同参数的原始 QRP 相位族 / 未铺砌参考")
    font = fonts(args.font)
    counter = Image.new("RGB", (1444, 650), BACKGROUND)
    draw = ImageDraw.Draw(counter)
    draw.text((28, 18), "q=8：旧相位参数可使整个源场抵消", font=font[34], fill=INK)
    draw.text((28, 69), "原始 QRP，尚未进入 Wang；三图使用完全相同的阈值、配色和尺度。", font=font[20], fill=MUTED)
    for col, (a, b, order, title) in enumerate(((0, 0, 2, "零相位基线"),
            (np.pi/2, np.pi/2, 2, "旧二阶相位：A=B=π/2"), (np.pi/2, np.pi/2, 3, "三阶相位：A=B=π/2"))):
        case = exported(8, a, b, order)
        values = source_field(case, 0, rx*2-.5, ry*2-.5, "raw")[..., 0]
        draw.text((28+col*470, 114), title, font=font[20], fill=INK)
        image = Image.fromarray(design_raster(values, BASE["cases"][1]["render"]))
        counter.paste(image.resize((448, 448), Image.Resampling.LANCZOS), (28+col*470, 151))
        if col == 1:
            report["q8_legacy_pi_over_2_sampled_max_abs"] = float(np.max(np.abs(values)))
    counter.save(OUTPUT/"counterexample.png")
    manifest = {**BASE, "source_provenance": "Exported by qrp_export_sources; phase harmonic recorded per channel.", "cases": cases}
    (OUTPUT/"manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    (OUTPUT/"results.json").write_text(json.dumps(report, ensure_ascii=False, indent=2)+"\n", encoding="utf-8")
    print(f"Figures and evidence: {OUTPUT}", flush=True)


if __name__ == "__main__":
    main()
