"""Wang 阅读的有限组合核对与缓存重铺图；不生成 QRP，不接入生产测试。"""

import argparse
import hashlib
import itertools
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from board_common import BACKGROUND, INK, MUTED, fonts


ROOT = Path(__file__).resolve().parents[1]


def endpoint_edges(tile_id):
    sw, se, nw, ne = ((tile_id >> i) & 1 for i in range(4))
    return (2 * sw + se, 2 * nw + ne, 2 * sw + nw, 2 * se + ne)


def compatible_tables(edges):
    horizontal = [[a[3] == b[2] for b in edges] for a in edges]
    vertical = [[a[1] == b[0] for b in edges] for a in edges]
    return np.array(horizontal), np.array(vertical)


def patch_count(edges):
    horizontal, vertical = compatible_tables(edges)
    # a,b 是下行，c,d 是上行；穷举仅 16^4 项。
    return sum(bool(horizontal[a, b] and horizontal[c, d]
                    and vertical[a, c] and vertical[b, d])
               for a, b, c, d in itertools.product(range(len(edges)), repeat=4))


def combinatorial_probes():
    corners = [endpoint_edges(i) for i in range(16)]
    h, v = compatible_tables(corners)
    # 独立用角点条件核对成对标签，而非只让编码函数与自己比较。
    for a, b in itertools.product(range(16), repeat=2):
        assert h[a, b] == (((a >> 1) & 1) == (b & 1)
                           and ((a >> 3) & 1) == ((b >> 2) & 1))
        assert v[a, b] == (((a >> 2) & 1) == (b & 1)
                           and ((a >> 3) & 1) == ((b >> 1) & 1))
    corner_count = patch_count(corners)
    edge_count = patch_count(list(itertools.product(range(2), repeat=4)))
    assert (corner_count, edge_count) == (2**9, 2**12)
    # Lagae-Dutre Eq. (2)：有限置换表带来轴向周期，不以随机外观代替证明。
    permutation = np.random.default_rng(20260923).permutation(16)
    x, y = np.meshgrid(np.arange(32), np.arange(32))
    hashed = lambda a, b: permutation[(permutation[a % 16] + b) % 16] % 2
    periodic_x = np.array_equal(hashed(x, y), hashed(x + 16, y))
    periodic_y = np.array_equal(hashed(x, y), hashed(x, y + 16))
    assert periodic_x and periodic_y
    return {
        "corner_encoding_pair_checks": 512,
        "corner_set_legal_horizontal_pairs": int(h.sum()),
        "corner_set_legal_vertical_pairs": int(v.sum()),
        "legal_2x2_corner_binary": corner_count,
        "legal_2x2_edge_binary": edge_count,
        "self_repeatable_corner_ids": [i for i in range(16) if h[i, i] and v[i, i]],
        "finite_hash": {"table_length": 16, "period_x_16": periodic_x,
                        "period_y_16": periodic_y, "minimal_period_claimed": False},
    }


def make_layout(seed, size=5):
    vertices = np.random.default_rng(seed).integers(0, 2, size=(size + 1, size + 1))
    return (vertices[:-1, :-1] + 2 * vertices[:-1, 1:]
            + 4 * vertices[1:, :-1] + 8 * vertices[1:, 1:])


def mismatches(layout, edges):
    labels = np.asarray(edges)[layout]
    return int(np.count_nonzero(labels[:, :-1, 3] != labels[:, 1:, 2])
               + np.count_nonzero(labels[:-1, :, 1] != labels[1:, :, 0]))


def assemble(layout, tiles):
    # 布局 y 向上，PNG 行向下；tile 内部像素不翻转、不缩放、不混合。
    size = layout.shape[0]
    pixels = tiles[0].shape[0]
    result = np.empty((size * pixels, size * pixels, 3), dtype=np.uint8)
    for y, x in np.ndindex(layout.shape):
        row = size - 1 - y
        result[row*pixels:(row+1)*pixels, x*pixels:(x+1)*pixels] = tiles[layout[y, x]]
    exact = all(np.array_equal(
        result[(size-1-y)*pixels:(size-y)*pixels, x*pixels:(x+1)*pixels],
        tiles[layout[y, x]]) for y, x in np.ndindex(layout.shape))
    assert exact
    return Image.fromarray(result), exact


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=ROOT / "output/qrp-phase-tile-study")
    parser.add_argument("--case", default="phase_ribbons")
    parser.add_argument("--output", type=Path, default=ROOT / "output/wang-foundations")
    parser.add_argument("--font", type=Path, default=Path("C:/Windows/Fonts/msyh.ttc"))
    args = parser.parse_args()
    manifest_path = args.source / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    edges = [endpoint_edges(i) for i in range(16)]
    assert manifest["tile_edges"] == [list(e) for e in edges]
    assert manifest["layout_row_order"] == "bottom-up"
    paths = [args.source / args.case / "tiles" / f"tile_{i}.png" for i in range(16)]
    tiles = [np.asarray(Image.open(path).convert("RGB")) for path in paths]
    assert all(tile.shape == (96, 96, 3) for tile in tiles)
    a, b = make_layout(20260923), make_layout(20260924)
    shuffled = np.random.default_rng(20260925).permutation(a.flatten()).reshape(a.shape)
    assert np.array_equal(np.sort(a.flatten()), np.sort(shuffled.flatten()))
    layouts = [np.zeros_like(a), a, b, shuffled]
    counts = [mismatches(layout, edges) for layout in layouts]
    assert counts[:3] == [0, 0, 0] and counts[3] > 0

    args.output.mkdir(parents=True, exist_ok=True)
    font = fonts(args.font)
    board = Image.new("RGB", (1068, 1272), BACKGROUND)
    draw = ImageDraw.Draw(board)
    draw.text((34, 22), "同一瓦片库：匹配规则究竟做了什么？", font=font[34], fill=INK)
    draw.text((34, 75), "已有 phase_ribbons 缓存 | 16 类瓦片 | 5×5 重铺 | 不重新计算 QRP", font=font[20], fill=MUTED)
    names = ["periodic", "legal_a", "legal_b", "shuffled"]
    titles = ["01  合法，但周期重复", "02  合法重铺 A", "03  合法重铺 B", "04  打乱 A：不合法的对照"]
    notes = ["只重复 tile 0；Wang 规则并不禁止周期", "共享顶点状态决定边标签；种子 20260923",
             "仍使用相同的 16 张缓存；种子 20260924", "与 A 的瓦片数量完全相同，仅打乱位置"]
    exact_results = []
    for i, (name, layout) in enumerate(zip(names, layouts)):
        x, y = 34 + (i % 2) * 520, 125 + (i // 2) * 564
        draw.text((x, y), titles[i], font=font[25], fill=INK)
        draw.text((x, y + 36), f"内部边标签失配：{counts[i]} / 40", font=font[17], fill=MUTED)
        pattern, exact = assemble(layout, tiles)
        pattern.save(args.output / f"{name}.png")
        exact_results.append(exact)
        board.paste(pattern, (x, y + 64))
        draw.text((x, y + 544), notes[i], font=font[15], fill=MUTED)
    board.save(args.output / "same-library-comparison.png")

    result = combinatorial_probes()
    result.update({
        "case": args.case, "source_directory": str(args.source.resolve()),
        "source_manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(),
        "source_tile_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths},
        "layout_row_order": "bottom-up", "layout_seeds": [None, 20260923, 20260924, 20260925],
        "layouts": {name: {"ids": layout.tolist(), "label_mismatches": count,
                           "all_blocks_pixel_identical": exact}
                    for name, layout, count, exact in zip(names, layouts, counts, exact_results)},
        "limits": "Finite combinatorial checks and cached PNG copies; no C++ regeneration, no continuous seam or aperiodicity proof.",
    })
    (args.output / "probes.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"patch_counts": [result["legal_2x2_corner_binary"], result["legal_2x2_edge_binary"]],
                      "label_mismatches": counts, "exact_pixel_reuse": exact_results,
                      "board": str(args.output / "same-library-comparison.png")}, ensure_ascii=False))


if __name__ == "__main__":
    main()
