"""当前母题选区：统一启发式探针或显式圆形区域，不宣称自动审美识别。"""

import numpy as np

from qrp_source import expression_jet

COLLAR = .2  # 保持既有探针的冻结包围盒语义；单位为世界长度。


def _probe_jet(point, case):
    """保留原选区探针对 ±shift 的平均，不把该诊断场当作最终源场。"""
    point = case['frequency']*point
    shift = np.asarray(case['shift'])
    result = expression_jet(case['q'], point[..., 0]+shift[0], point[..., 1]+shift[1], case['model'])
    if np.any(shift):
        result += expression_jet(case['q'], point[..., 0]-shift[0], point[..., 1]-shift[1], case['model'])
        result *= .5
    result[..., 1:] *= case['frequency']
    return result


def motif_probe(case, radial_samples=1025, angular_samples=512):
    """启发式选取第一圈明显方向调制；不把它当作自动识别美感或闭合轮廓证明。"""
    normalized = dict(case, frequency=1.)
    eps = 1e-4
    ex, ey = np.eye(2)*eps
    curvature = -np.column_stack(((_probe_jet(ex, normalized)[1:]-_probe_jet(-ex, normalized)[1:])/(2*eps),
                                  (_probe_jet(ey, normalized)[1:]-_probe_jet(-ey, normalized)[1:])/(2*eps)))
    eigenvalues, eigenvectors = np.linalg.eigh((curvature+curvature.T)/2)
    if eigenvalues.min() <= 1e-6:
        raise ValueError('中心不是非退化二维峰：不能自动定义本探针的中心母题，需要显式指定源区域。')
    transform = (eigenvectors*np.sqrt(eigenvalues.mean()/eigenvalues))@eigenvectors.T
    radii = np.linspace(0., 32., radial_samples)
    angles = np.arange(angular_samples)*2*np.pi/angular_samples
    directions = np.stack((np.cos(angles), np.sin(angles)), axis=-1)@transform.T
    points = radii[:, None, None]*directions[None, :, :]
    values = _probe_jet(points, normalized)[..., 0]
    contrast = np.ptp(values, axis=1)
    peaks = np.flatnonzero((contrast[1:-1] > contrast[:-2]) & (contrast[1:-1] >= contrast[2:])
                           & (contrast[1:-1] > .35))+1
    if not len(peaks):
        raise ValueError('在规定搜索范围内未找到方向调制圈；不自动扩大搜索或伪造选区。')
    peak = int(peaks[0])
    radius = float(radii[peak]+np.pi/np.sqrt(eigenvalues.sum()))
    bounds = radius*np.linalg.norm(transform, axis=1)/case['frequency']
    base = np.ceil(2*(bounds+COLLAR)).astype(int)
    return dict(transform=transform.tolist(), curvature_eigenvalues=eigenvalues.tolist(),
                peak_radius=float(radii[peak]), peak_contrast=float(contrast[peak]), radius=radius,
                physical_half_bounds=bounds.tolist(), base=base.tolist(),
                selection='first_sampled_angular_contrast_peak_above_0.35_plus_curvature_margin')


def core_probe(case, radius=None):
    if radius is None:
        return motif_probe(case)
    bounds = radius/case['frequency']
    return dict(transform=np.eye(2).tolist(), radius=radius, physical_half_bounds=[bounds, bounds],
                base=[int(np.ceil(2*(bounds+.125)))]*2,
                selection='explicit_circular_source_region_not_automatic_motif_detection')
