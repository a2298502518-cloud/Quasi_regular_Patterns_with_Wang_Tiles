"""当前 QRP–Wang 出图与结果复核入口；算法、显示和检查由各自模块拥有。"""

import argparse
import json
from pathlib import Path
import time

import numpy as np

from qrp_checks import audit, repeated_pixels, verification
from qrp_motif import core_probe
from qrp_render import board, render, source_image
from wang_qrp import CornerPhaseAtlas

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pixels', type=int, default=64)
    parser.add_argument('--cases', nargs='+', choices=['q4p8', 'q6', 'q7'], default=['q4p8', 'q6', 'q7'])
    parser.add_argument('--extent', type=int, default=24)
    parser.add_argument('--q', nargs='+', type=float, help='独立新参数路径，不读取历史输出或图像库')
    parser.add_argument('--model', choices=['basic', 'cubic-directions'], default='basic')
    parser.add_argument('--frequency', type=float, default=12.)
    parser.add_argument('--core-radius', type=float, help='显式圆形源区域半径；不冒称自动识别母题')
    parser.add_argument('--verify', action='store_true', help='复核当前输出目录，不重新选图或优化参数')
    parser.add_argument('--output', type=Path)
    parser.add_argument('--font', type=Path, default=Path('C:/Windows/Fonts/msyh.ttc'))
    args = parser.parse_args()
    if not args.q and (args.core_radius is not None or args.frequency != 12. or args.model != 'basic'):
        parser.error('修改源参数请使用 --q；冻结对照配方不能被元数据选项局部覆盖。')
    if args.pixels < 16 or args.extent < 1:
        parser.error('pixels 至少为 16，extent 至少为 1。')
    if (not np.isfinite(args.frequency) or args.frequency <= 0 or
            (args.q and any(not np.isfinite(q) or q < 1 for q in args.q)) or
            (args.core_radius is not None and (not np.isfinite(args.core_radius) or args.core_radius <= 0))):
        parser.error('需要有限 q≥1、正频率及正的指定核心半径。')
    args.output = args.output or ROOT/'output/qrp-current-run'
    args.output.mkdir(parents=True, exist_ok=True)
    defaults = json.loads((ROOT/'tools/qrp_wang_defaults.json').read_text(encoding='utf8'))
    style = defaults['render']
    if args.verify:
        records = json.loads((args.output/'results.json').read_text(encoding='utf8'))
        verification(args.output, records, args.pixels, style, args.font)
        return
    if args.q:
        rows = []
        for q in args.q:
            case = dict(name=f'q{q:g}'.replace('.', 'p'), q=q, model=args.model,
                        frequency=args.frequency, shift=[0., 0.])
            rows.append(dict(parameters=case, probe=core_probe(case, args.core_radius)))
    else:
        rows = [dict(parameters=case, probe=core_probe(case)) for case in defaults['cases']
                if case['name'] in args.cases]
    reports = []
    for row in rows:
        case, probe = row['parameters'], row['probe']
        start = time.perf_counter()
        atlas = CornerPhaseAtlas(probe, case)
        verified = audit(atlas, case, probe)
        span = args.extent
        plans = [atlas.plan(span, seed) for seed in (11, 37)]
        plans += [atlas.plan(span, 11, flip=(1, 1)), atlas.plan(2*span, 11), atlas.plan(span, 11, uniform=True)]
        pictures = render(atlas, plans, case, args.pixels, style)
        for tag, picture in zip(('a', 'b', 'switch', f'{2*span}x{2*span}', 'uniform'), pictures):
            picture.save(args.output/f'{case["name"]}-{tag}.png')
        raw = source_image(case, span, args.pixels, style)
        examples, labels = [raw, pictures[0], pictures[1]], ['连续 QRP（同尺度）', '有限关系：布局 A', '有限关系：布局 B']
        board(examples, labels, f'Q={case["q"]:g} · λ={case["frequency"]:g} · 同一 {span}×{span} 世界范围',
              args.font, side=512).save(args.output/f'{case["name"]}-comparison.png')
        board([pictures[4], pictures[0], pictures[1]], ['所有角状态相同：周期对照', '同参数：布局 A', '同参数：布局 B'],
              f'Q={case["q"]:g} · 16 种宏关系 / {len(atlas.roles)} 种单位类型 · 不依实例位置重建',
              args.font, side=512).save(args.output/f'{case["name"]}-layouts.png')
        assert np.array_equal(plans[0], plans[3][:span, :span])
        crop = pictures[3].crop((0, span*args.pixels, span*args.pixels, 2*span*args.pixels))
        assert np.array_equal(crop, pictures[0])
        reuse = repeated_pixels(plans[:2], pictures[:2], args.pixels)
        a, b = np.asarray(pictures[0]), np.asarray(pictures[2])
        changed = np.any(a != b, axis=-1)
        role_change = plans[0] != plans[2]
        allowed = np.repeat(np.repeat(role_change[::-1], args.pixels, axis=0), args.pixels, axis=1)
        assert not changed[~allowed].any()
        report = dict(parameters=case, probe=probe, connection='relational', extent=span, audit=verified, reuse=reuse,
                      observed_types_two_layouts=int(len(np.unique(np.concatenate(plans[:2])))),
                      extension_pixel_identical=True, switched_micro_tiles=int(role_change.sum()),
                      switched_pixels=int(changed.sum()), changed_outside_switch=0,
                      elapsed_seconds=time.perf_counter()-start)
        reports.append(report)
        print(json.dumps(report, ensure_ascii=False), flush=True)
        (args.output/'results.json').write_text(json.dumps(reports, ensure_ascii=False, indent=2), encoding='utf8')


if __name__ == '__main__':
    main()
