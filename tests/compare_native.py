"""迁移对照工具；正式 C++ 程序不调用 Python，也不依赖这里生成的材料。"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from qrp_motif import core_probe
from qrp_render import render, source_image
from wang_qrp import CornerPhaseAtlas


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--exe', type=Path, default=ROOT/'build/Release/qrp_generate.exe')
    parser.add_argument('--output', type=Path, default=ROOT/'output/native-migration-check')
    parser.add_argument('--hd', action='store_true', help='额外对照已有 Q7 高清交付；默认不运行大图检查')
    args = parser.parse_args()
    style = json.loads((ROOT/'tools/qrp_wang_defaults.json').read_text(encoding='utf8'))['render']
    cases = [(4.8, 'cubic-directions', None, [0., 0.]),
             (5.5, 'basic', None, [0., 0.]), (6., 'basic', None, [0., 0.]),
             (7., 'basic', None, [0., 0.]), (8., 'basic', None, [0., 0.]),
             (12., 'basic', None, [0., 0.]), (7., 'basic', 6., [.3, .2]),
             (7., 'basic', None, [.3, .2])]
    reports = []
    for index, (q, model, radius, shift) in enumerate(cases):
        directory = args.output/f'{index}-q{q:g}-{model}'
        command = [str(args.exe), '--q', str(q), '--model', model, '--shift', *map(str, shift),
                   '--extent', '12', '--pixels', '24', '--output', str(directory)]
        if radius is not None:
            command += ['--core-radius', str(radius)]
        subprocess.run(command, check=True)
        native = json.loads((directory/'parameters.json').read_text(encoding='utf8'))
        case = dict(q=q, model=model, frequency=12., shift=shift)
        probe = core_probe(case, radius)
        atlas = CornerPhaseAtlas(probe, case)
        np.testing.assert_allclose(native['probe']['radius'], probe['radius'], atol=1e-10, rtol=0)
        np.testing.assert_allclose(native['probe']['transform'], probe['transform'], atol=1e-10, rtol=0)
        np.testing.assert_array_equal(native['cell_world_size'], atlas.size)
        np.testing.assert_array_equal(native['relation_matrix'], atlas.lattice.transform)
        np.testing.assert_array_equal(native['base_integer_increments'], atlas.increments)
        np.testing.assert_allclose(native['analytic_relative_wave_vector_bound'],
                                   atlas.geometry_report['analytic_relative_wave_vector_bound'], atol=1e-10, rtol=0)
        jet_error = 0.
        for sample in native['samples']:
            expected = atlas.field(sample['tile'], *sample['point'], case)
            jet_error = max(jet_error, float(abs(expected-sample['jet']).max()))
        assert jet_error < 1e-9, jet_error
        plans = [atlas.plan(12, seed) for seed in (11, 37)]
        for key, plan in zip(('layout_a', 'layout_b'), plans):
            np.testing.assert_array_equal(native[key], plan.ravel())
        expected = [source_image(case, 12, 24, style), *render(atlas, plans, case, 24, style)]
        pixel_reports = []
        for name, picture in zip(('source', 'wang-a', 'wang-b'), expected):
            with Image.open(directory/f'{name}.png') as image:
                actual = np.array(image.convert('RGB'))
            reference = np.array(picture)
            changed = int(np.any(actual != reference, axis=-1).sum())
            pixel_reports.append(dict(image=name, changed_pixels=changed))
            np.testing.assert_array_equal(actual, reference)
        report = dict(parameters=case, jet_max_error=jet_error, pixels=pixel_reports,
                      macro_size=native['cell_world_size'], native_elapsed_seconds=native['elapsed_seconds'])
        reports.append(report)
        print(json.dumps(report), flush=True)
    hd = None
    if args.hd:
        directory = args.output/'Q07_HD'
        subprocess.run([str(args.exe), '--q', '7', '--extent', '24', '--pixels', '192',
                        '--output', str(directory)], check=True)
        hd = []
        for name, old_name in (('source', 'PureQRP'), ('wang-a', 'Wang_A_Seed11'), ('wang-b', 'Wang_B_Seed37')):
            with Image.open(directory/f'{name}.png') as actual:
                assert actual.mode == 'RGB' and actual.size == (4608, 4608)
                assert abs(actual.info['dpi'][0]-300) < .01 and actual.info['srgb'] == 0
                with Image.open(ROOT/'output/QRP_Wang_HD_2026-09-30/Q07'/f'Q07_Basic_{old_name}_4608x4608.png') as reference:
                    np.testing.assert_array_equal(np.array(actual.convert('RGB')), np.array(reference.convert('RGB')))
            hd.append(dict(image=name, size=[4608, 4608], mode='RGB', pixel_identical=True))
        print('Q7: three 4608x4608 RGB8 / sRGB / 300 DPI images matched the delivered PNG pixels.')
    (args.output/'summary.json').write_text(json.dumps(dict(cases=reports, hd=hd), indent=2), encoding='utf8')
    print(f'{len(reports)} cases: scalar/gradient, geometry, integer branches, layouts and {3*len(reports)} PNG arrays matched.')


if __name__ == '__main__':
    main()
