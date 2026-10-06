"""当前构造的必要证据检查；连续性证明和美观评价不能被采样测试代替。"""

from collections import defaultdict
import json
import time

import numpy as np

from qrp_source import source_jet
from qrp_motif import core_probe
from qrp_render import board, render
from wang_qrp import CornerPhaseAtlas


def audit(atlas, case, probe):
    """全有限目录的边迹、核心、梯度、关系；不把局部波长当审美分数。"""
    axis = np.linspace(0, 1, 33)
    sides = ((axis, 0*axis), (axis, 0*axis+1), (0*axis, axis), (0*axis+1, axis))
    groups = defaultdict(list)
    for tile, edges in enumerate(atlas.edges):
        for side, xy in enumerate(sides):
            groups[(side//2, int(edges[side]))].append((side, atlas.field(tile, *xy, case)))
    seam, pairs = 0., 0
    for entries in groups.values():
        low = [v for s, v in entries if s % 2 == 0]
        high = [v for s, v in entries if s % 2 == 1]
        if low and high:
            pairs += len(low)*len(high)
            seam = max(seam, *(float(abs(v-low[0]).max()) for _, v in entries))
    core_error = 0.
    angles = np.arange(64)*2*np.pi/64
    circle = np.stack((np.cos(angles), np.sin(angles)), axis=-1)@np.array(probe['transform']).T
    relative = circle*probe['radius']/case['frequency']
    for color in (0, 1):
        center = atlas.shifts[color]
        points = np.concatenate((relative, .5*relative, relative*0))+center
        actual = np.empty(points.shape[:-1]+(3,))
        for k, p in enumerate(points):
            cell = np.floor(p/atlas.size).astype(int)
            local = p-cell*atlas.size
            micro = np.minimum(np.floor(local).astype(int), atlas.size-1)
            # 全同状态足以核对所选节点四侧；所有其他角状态在该核心中权重严格为零。
            tile = atlas.ids[(15*color, int(micro[0]), int(micro[1]))]
            actual[k] = atlas.field(tile, *(local-micro), case)
        target = source_jet(points-center, case)
        core_error = max(core_error, float(abs(actual-target).max()))
    grid = np.linspace(0, 1, 17)
    x, y = np.meshgrid(grid, grid)
    ratios, worst_delta, derivative, relation_error = [], 0., 0., 0.
    waves = atlas.lattice.waves
    matrix = atlas.lattice.transform
    rank = matrix.shape[1]
    relation = np.column_stack((-matrix[rank:], np.eye(len(matrix)-rank)))
    for tile in range(len(atlas.roles)):
        phase = atlas.phases(tile, x, y, case)
        ratios.extend((np.linalg.norm(phase[..., 1:], axis=-1)/np.linalg.norm(waves, axis=-1)).ravel())
        worst_delta = max(worst_delta, float((np.linalg.norm(phase[..., 1:]-waves, axis=-1)/np.linalg.norm(waves, axis=-1)).max()))
        relation_error = max(relation_error, float(abs(np.einsum('rm,...md->...rd', relation, phase)).max(initial=0)))
    if atlas.geometry_report is not None:
        assert worst_delta <= atlas.geometry_report['analytic_relative_wave_vector_bound']+1e-10
    for tile in np.arange(0, len(atlas.roles), max(1, len(atlas.roles)//12)):
        px, py, eps = .371, .613, 1e-6
        finite = np.array([(atlas.field(tile, px+eps, py, case)[0]-atlas.field(tile, px-eps, py, case)[0])/(2*eps),
                           (atlas.field(tile, px, py+eps, case)[0]-atlas.field(tile, px, py-eps, case)[0])/(2*eps)])
        derivative = max(derivative, float(abs(finite-atlas.field(tile, px, py, case)[1:]).max()))
    assert seam < 1e-9 and core_error < 1e-9 and derivative < 1e-6 and relation_error < 1e-9
    return dict(legal_pairs=pairs, edge_jet_error=seam, protected_core_jet_error=core_error,
                finite_difference_error=derivative, sampled_wave_number_ratio=[float(min(ratios)), float(max(ratios))],
                sampled_relative_wave_vector_change_max=worst_delta,
                catalogue_size=len(atlas.roles), macro_codes=16, implicit_fitted_images=0,
                base_integer_increments=atlas.increments.tolist(), position_states=atlas.shifts.tolist(),
                cell_world_size=atlas.size.tolist(), geometry_selection=atlas.geometry_report,
                relative_position_integer_states=atlas.state_lifts.tolist(),
                certified_basic_relation_jet_error=relation_error if case['model'] == 'basic' else None,
                mean_wave_vector_relative_bias=float(np.max(np.linalg.norm(
                    (2*np.pi*atlas.increments/atlas.size[:, None]).T-waves, axis=1)/np.linalg.norm(waves, axis=1))))


def repeated_pixels(plans, pictures, pixels):
    samples, repeated = {}, 0
    for plan, picture in zip(plans, pictures):
        array = np.asarray(picture)
        for row, col in np.ndindex(plan.shape):
            patch = array[(len(plan)-1-row)*pixels:(len(plan)-row)*pixels, col*pixels:(col+1)*pixels]
            tile = int(plan[row, col])
            if tile in samples:
                assert np.array_equal(patch, samples[tile])
                repeated += 1
            else:
                samples[tile] = patch
    return dict(repeated_instances=repeated, all_repeated_patches_pixel_exact=True)


def verification(output, records, pixels, style, font):
    """一个收尾入口覆盖独立求值、相对支对照、新参数准备与一般 Q；不扩成防御性测试套件。"""
    result = dict(cases=[], general_q=[])
    for record in records:
        assert record['connection'] == 'relational', '只复核当前 relational 结果，旧路线请使用归档版本。'
        case, probe = record['parameters'], record['probe']
        start = time.perf_counter()
        atlas = CornerPhaseAtlas(probe, case)
        preparation = time.perf_counter()-start
        start = time.perf_counter()
        selected = core_probe(case, probe['radius'] if probe.get('selection', '').startswith('explicit_') else None)
        motif_seconds = time.perf_counter()-start
        assert np.allclose(selected['physical_half_bounds'], probe['physical_half_bounds'], rtol=0, atol=1e-12)
        verified = audit(atlas, case, probe)
        span = record['extent']
        plan = atlas.plan(span, 11)
        start = time.perf_counter()
        cached = render(atlas, [plan], case, pixels, style)[0]
        cached_seconds = time.perf_counter()-start
        start = time.perf_counter()
        direct = render(atlas, [plan], case, pixels, style, cached=False)[0]
        direct_seconds = time.perf_counter()-start
        assert np.array_equal(cached, direct)
        # 共享 assemble 还会逐块重求一次作核对，因此 direct_seconds 不是最优渲染器性能。
        shifted = dict(case, shift=[2., 1.3])
        shift_checks = audit(atlas, shifted, probe)
        item = dict(parameters=case, checks=verified, shifted_checks=shift_checks,
                    direct_and_cached_pixel_identical=True, parameter_constants_prepare_seconds=preparation,
                    fresh_motif_probe_seconds=motif_seconds,
                    cached_preview_seconds=cached_seconds, direct_preview_with_redundant_verification_seconds=direct_seconds)
        if case['name'] == 'q7':
            # 保持尺寸、位置和 QRP 参数，只去掉位置状态的相对整数支；这仍可接边，但不再满足原相位界。
            atlas.state_offsets = -atlas.shifts@atlas.lattice.waves.T
            atlas.state_lifts[:] = 0
            atlas.geometry_report = None
            control = audit(atlas, case, probe)
            image = render(atlas, [plan], case, pixels, style)[0]
            board([image, cached], ['同几何，仅位置变化：未协调支', '同几何，共同相对整数支'],
                  'Q=7 · 固定几何、源、布局；只比较整数支作用', font, side=640).save(output/'q7-integer-control.png')
            item['zero_relative_integer_control'] = control
        result['cases'].append(item)
    for q in (1., 5., 5.5, 8., 12., 17.):
        case = dict(q=q, model='basic', frequency=12., shift=[0., 0.])
        probe = core_probe(case, 6.)
        atlas = CornerPhaseAtlas(probe, case)
        result['general_q'].append(dict(parameters=case, probe=probe, checks=audit(atlas, case, probe)))
    (output/'verification.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(dict(verification=str(output/'verification.json'), direct_cache_cases=len(records),
                         general_q_cases=len(result['general_q'])), ensure_ascii=False), flush=True)
