"""Shared QRP source evaluation, Hermite H1 projection and fixed display mappings.

Local coordinates (u,v), analytic first derivatives; no layout-dependent field evaluation.
The modes come from C++ exports, never reconstructed from metadata such as q.
"""

import numpy as np
from scipy.sparse import coo_matrix
from scipy.sparse.linalg import spsolve


def multiply_fields(a, b):
    return np.concatenate(((a[..., 0]*b[..., 0])[..., None],
                           a[..., 1:]*b[..., :1]+b[..., 1:]*a[..., :1]), axis=-1)


def source_field(case, tile, x, y, kind):
    """返回 (value, d/du, d/dv)；只解释序列化波，不重新生成 QRP 模态。"""
    # 固定类型可指向不同的序列化源块；仍用同一模态求值和投影实现。
    if "tile_sources" in case:
        case = case["tile_sources"][tile]
    x, y = np.broadcast_arrays(x, y)
    hx, hy = x*x*(3-2*x), y*y*(3-2*y)
    dx, dy = 6*x*(1-x), 6*y*(1-y)
    sw, se, nw, ne = ((tile >> i) & 1 for i in range(4))
    state = (1-hy)*((1-hx)*sw+hx*se) + hy*((1-hx)*nw+hx*ne)
    sx = dx*((1-hy)*(se-sw)+hy*(ne-nw))
    sy = dy*((1-hx)*(nw-sw)+hx*(ne-se))
    channels = []
    for channel in case["channels"]:
        field = np.zeros(x.shape + (3,))
        for mode in channel["modes"]:
            wx, wy = mode["wave"]
            phase = wx*x + wy*y + mode["phase"]
            px, py = np.full_like(x, wx), np.full_like(y, wy)
            if kind != "raw":
                phase += mode["vertex_shift"] * state
                px += mode["vertex_shift"] * sx
                py += mode["vertex_shift"] * sy
            if kind == "closed":
                cx, cy = mode["closure"]
                phase += cx*hx + cy*hy
                px += cx*dx
                py += cy*dy
            field[..., 0] += mode["amplitude"] * np.cos(phase)
            derivative = -mode["amplitude"] * np.sin(phase)
            field[..., 1] += derivative * px
            field[..., 2] += derivative * py
        channels.append(field)
    if case["relation"] == "direct":
        return channels[0]
    assert case["relation"] == "product" and len(channels) == 2
    return multiply_fields(*channels)


def hermite(t):
    """[左值, 左导数, 右值, 右导数]，导数自由度按网格步长缩放。"""
    return (np.stack((2*t**3-3*t*t+1, t**3-2*t*t+t,
                      -2*t**3+3*t*t, t**3-t*t), axis=-1),
            np.stack((6*t*t-6*t, 3*t*t-4*t+1,
                      -6*t*t+6*t, 3*t*t-2*t), axis=-1))


def stencil(x, y, cells):
    x, y = np.broadcast_arrays(x, y)
    ix = np.minimum(np.floor(x.ravel()*cells).astype(int), cells-1)
    iy = np.minimum(np.floor(y.ravel()*cells).astype(int), cells-1)
    bx, dx = hermite(x.ravel()*cells-ix)
    by, dy = hermite(y.ravel()*cells-iy)
    nodes, basis = [], []
    for cy in range(2):
        for cx in range(2):
            for component in range(4):
                px, py = 2*cx+(component & 1), 2*cy+(component >> 1)
                nodes.append((iy+cy, ix+cx, component))
                basis.append(np.stack((bx[:, px]*by[:, py],
                                       cells*dx[:, px]*by[:, py],
                                       cells*bx[:, px]*dy[:, py]), axis=-1))
    return nodes, np.stack(basis, axis=-1)


def dof_map(cells, edges, shared):
    """同色边共享整个 jet；角点按二值状态共享，不用外法线符号。"""
    mapping = np.empty((16, cells+1, cells+1, 4), dtype=int)
    keys = {}
    for tile, j, i, component in np.ndindex(mapping.shape):
        if shared and i in (0, cells) and j in (0, cells):
            bit = int(i == cells) + 2*int(j == cells)
            key = ("corner", (tile >> bit) & 1, component)
        elif shared and i in (0, cells):
            key = ("vertical", edges[tile][2 if i == 0 else 3], j, component)
        elif shared and j in (0, cells):
            key = ("horizontal", edges[tile][0 if j == 0 else 1], i, component)
        else:
            key = ("interior", tile, j, i, component)
        mapping[tile, j, i, component] = keys.setdefault(key, len(keys))
    return mapping, len(keys)


class Projection:
    def __init__(self, cells, edges, wave_scale, shared):
        self.cells = cells
        self.mapping, self.count = dof_map(cells, edges, shared)
        q, w = np.polynomial.legendre.leggauss(4)
        axis = ((np.arange(cells)[:, None]+(q+1)/2)/cells).ravel()
        self.x, self.y = np.meshgrid(axis, axis)
        weights = np.tile(w/2/cells, cells)
        weights = np.sqrt(np.outer(weights, weights)).ravel()
        self.weights = weights[:, None]*np.array([1, 1/wave_scale, 1/wave_scale])
        nodes, basis = stencil(self.x, self.y, cells)
        data = (basis*self.weights[..., None]).ravel()
        rows = np.repeat(np.arange(basis.shape[0]*3), 16)
        row_count = basis.shape[0]*3
        columns = []
        for tile in range(16):
            indices = np.stack([self.mapping[tile, j, i, c] for j, i, c in nodes], axis=-1)
            columns.append(np.repeat(indices[:, None, :], 3, axis=1).ravel())
        self.matrix = coo_matrix((np.tile(data, 16),
                                  (np.concatenate([rows+t*row_count for t in range(16)]),
                                   np.concatenate(columns))),
                                 shape=(row_count*16, self.count)).tocsr()
        self.normal = (self.matrix.T @ self.matrix).tocsc()

    def fit(self, case):
        targets = [source_field(case, tile, self.x, self.y, "target").reshape(-1, 3)
                   for tile in range(16)]
        rhs = np.concatenate([(value*self.weights).ravel() for value in targets])
        normal_rhs = self.matrix.T @ rhs
        coefficients = spsolve(self.normal, normal_rhs)
        residual = np.linalg.norm(self.normal @ coefficients-normal_rhs)/np.linalg.norm(normal_rhs)
        return coefficients, float(residual)

    def evaluate(self, coefficients, tile, x, y):
        nodes, basis = stencil(x, y, self.cells)
        indices = np.stack([self.mapping[tile, j, i, c] for j, i, c in nodes], axis=-1)
        result = np.einsum("nck,nk->nc", basis, coefficients[indices])
        return result.reshape(np.broadcast_shapes(np.shape(x), np.shape(y))+(3,))


def sample_grid(pixels):
    axis = (np.arange(pixels)+0.5)/pixels
    return np.meshgrid(axis, 1-axis)


def check_basis(projection):
    """一个已知双三次多项式同时核对值、梯度与步长约定。"""
    axis = np.arange(projection.cells+1)/projection.cells
    x, y = np.meshgrid(axis, axis)
    values = np.stack((x**3*y**2+2*x-3*y+1,
                       (3*x*x*y*y+2)/projection.cells,
                       (2*x**3*y-3)/projection.cells,
                       6*x*x*y/projection.cells**2), axis=-1)
    coefficients = np.zeros(projection.count)
    coefficients[projection.mapping[0].ravel()] = values.ravel()
    x, y = sample_grid(31)
    actual = projection.evaluate(coefficients, 0, x, y)
    exact = np.stack((x**3*y**2+2*x-3*y+1, 3*x*x*y*y+2, 2*x**3*y-3), axis=-1)
    error = float(np.max(np.abs(actual-exact)))
    assert error < 1e-10
    return error


def decode_srgb(colors):
    colors = np.asarray(colors)/255.0
    return np.where(colors <= 0.04045, colors/12.92, ((colors+0.055)/1.055)**2.4)


def encode_srgb(linear):
    srgb = np.where(linear <= 0.0031308, linear*12.92, 1.055*linear**(1/2.4)-0.055)
    return np.floor(srgb*255+0.5).astype(np.uint8)


def raster(values, palette):
    """与 C++ 相同的 2×2 覆盖率及线性光双色混合，不归一化标量值。"""
    size = values.shape[0]//2
    coverage = (values < 0).reshape(size, 2, size, 2).mean(axis=(1, 3))
    linear = decode_srgb(palette)
    mixed = coverage[..., None]*linear[0]+(1-coverage[..., None])*linear[1]
    return encode_srgb(mixed)


def design_colors(values, style):
    """高度到颜色的固定映射；不是改场，也不是等几何线宽描边。"""
    if style["kind"] == "bands":
        indices = np.searchsorted(style["levels"], values)
        colors = np.array(style["colors"], dtype=np.uint8)[indices]
        if "outline_half_width" in style:
            for level in style["levels"]:
                colors[np.abs(values-level) < style["outline_half_width"]] = style["outline"]
    else:
        assert style["kind"] == "contours"
        colors = np.empty(values.shape+(3,), dtype=np.uint8)
        colors[:] = style["paper"]
        for level in style["levels"]:
            colors[np.abs(values-level) < style["half_width"]] = style["ink"]
        colors[np.abs(values-style["accent_level"]) < style["accent_half_width"]] = style["accent"]
    return colors


def design_raster(values, style):
    size = values.shape[0]//2
    colors = decode_srgb(design_colors(values, style))
    return encode_srgb(colors.reshape(size, 2, size, 2, 3).mean(axis=(1, 3)))


def error_metrics(actual, target, scale):
    delta = actual-target
    return {"value_rmse": float(np.sqrt(np.mean(delta[..., 0]**2))),
            "gradient_rmse_normalized": float(np.sqrt(np.mean(np.sum(delta[..., 1:]**2, axis=-1)))/scale),
            "sign_disagreement": float(np.mean((actual[..., 0] < 0) != (target[..., 0] < 0))),
            "range": [float(actual[..., 0].min()), float(actual[..., 0].max())]}


def boundary_checks(case, evaluate, edges):
    t = np.linspace(0, 1, 257)
    points = [(t, np.zeros_like(t)), (t, np.ones_like(t)),
              (np.zeros_like(t), t), (np.ones_like(t), t)]
    candidate, target = [], []
    for tile in range(16):
        candidate.append([evaluate(tile, x, y) for x, y in points])
        target.append([source_field(case, tile, x, y, "target") for x, y in points])
    seam = np.zeros(3)
    pairs = 0
    for a, b in np.ndindex(16, 16):
        for outgoing, incoming in [(3, 2), (1, 0)]:
            if edges[a][outgoing] == edges[b][incoming]:
                seam = np.maximum(seam, np.max(np.abs(candidate[a][outgoing]-candidate[b][incoming]), axis=0))
                pairs += 1
    assert np.max(seam) < 1e-10
    bounds, conflicts = [], []
    for sides in [(0, 1), (2, 3)]:
        for label in range(4):
            traces = np.array([target[tile][side][:, 0] for tile in range(16)
                               for side in sides if edges[tile][side] == label])
            bounds.append(float(np.max(np.ptp(traces, axis=0))/2))
            conflicts.append(float(np.mean((traces.min(axis=0) < 0) & (traces.max(axis=0) > 0))))
    return {"legal_pairs_checked": pairs, "sampled_seam_max_value_dx_dy": seam.tolist(),
            "target_trace_half_range_max": max(bounds),
            "target_edge_sign_conflict_fraction_mean": float(np.mean(conflicts))}
