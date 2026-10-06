"""保留母题的有限 Wang 相容场；只维护当前 relational 直接公式。"""

import numpy as np

from qrp_source import PhaseLattice, direction_vectors
from wang_tiles import mismatches


def transition(value, half_core, length):
    """全可用间隙中的 C2 过渡；保护区不挪到狭窄的另一个外罩里。"""
    gap = length-2*half_core
    if gap <= 0:
        raise ValueError('核心及内部位置状态占满连接区，不能继续构造。')
    t = np.clip((value-half_core)/gap, 0., 1.)
    return t**3*(10-15*t+6*t*t), 30*t*t*(1-t)**2/gap


def bounded_geometry(lattice, bounds, displacement, state_offsets, rho=.5):
    """按相位梯度充分界选最小面积整数间距，不按 Q 维护尺寸表。"""
    waves = lattice.waves
    norm = np.linalg.norm(waves, axis=1)
    half_core = bounds+displacement
    state_range = np.ptp(state_offsets, axis=0)
    rounding_bound = np.pi*np.abs(lattice.qr*np.diag(lattice.upper)).sum(axis=1)
    # 该方形尺寸本身满足界，给枚举一个来自证明的有限上限，而不是逐 Q 放大到碰巧成功。
    sufficient_square = int(np.ceil(2*half_core.max()+1.875*np.sqrt(2)*np.max((rounding_bound+state_range)/norm)/rho))+1
    minimum = np.floor(2*half_core).astype(int)+1
    best = None
    # 方形见证给面积上界；按面积遍历也包含长边超过该方形边长的细长矩形。
    for area in range(int(minimum.prod()), sufficient_square**2+1):
        for w in range(minimum[0], area//minimum[1]+1):
            if area % w:
                continue
            h = area//w
            size = np.array([w, h])
            increments = lattice.integer_state(np.diag(size))
            residual = 2*np.pi*increments-np.diag(size)@waves.T
            components = 1.875*(abs(residual)+state_range)/(size-2*half_core)[:, None]
            bound = float(np.max(np.linalg.norm(components, axis=0)/norm))
            if bound <= rho and (best is None or (w*h, bound, w, h) < best[0]):
                best = ((w*h, bound, w, h), size)
        if best is not None:
            break
    assert best is not None
    return best[1], dict(requested_relative_wave_vector_bound=rho,
                         analytic_relative_wave_vector_bound=best[0][1],
                         proven_sufficient_square_size=sufficient_square,
                         search_kind='minimum_integer_area_under_sufficient_bound',
                         floating_evaluation_not_interval_arithmetic=True)


class CornerPhaseAtlas:
    """16 个角状态组合直接定义场，再编译成物理单位 Wang tile 的有限角色。"""

    def __init__(self, probe, case):
        self.source_key = (case['q'], case['model'], case['frequency'])
        self.lattice = PhaseLattice(case)
        bounds = np.array(probe['physical_half_bounds'])
        # 两种位置状态是统一构造配方，不是按 Q 独立调出的风格参数。
        direction = np.array([1., np.sqrt(2.)])/np.sqrt(3.)
        distance = probe['radius']/case['frequency']/2
        displacement = direction*distance
        self.shifts = np.array([-displacement, displacement])
        # 共同选择相对整数支，避免分别舍入正负位置而重复引入残差。
        self.state_lifts = self.lattice.integer_state(self.shifts-self.shifts[0])
        self.state_offsets = 2*np.pi*self.state_lifts-self.shifts@self.lattice.waves.T
        self.size, self.geometry_report = bounded_geometry(self.lattice, bounds, displacement, self.state_offsets)
        self.half_core = bounds+displacement
        self.increments = self.lattice.integer_state(np.diag(self.size))
        self.roles, self.ids, self.edges, labels = [], {}, [], {}
        w, h = self.size
        for code in range(16):
            sw, se, nw, ne = self.colors(code)
            for j, i in np.ndindex(h, w):
                self.ids[(code, i, j)] = len(self.roles)
                self.roles.append((code, i, j))
                keys = [('h', sw, se, i) if j == 0 else ('inside-y', code, i, j),
                        ('h', nw, ne, i) if j == h-1 else ('inside-y', code, i, j+1),
                        ('v', sw, nw, j) if i == 0 else ('inside-x', code, i, j),
                        ('v', se, ne, j) if i == w-1 else ('inside-x', code, i+1, j)]
                self.edges.append([labels.setdefault(k, len(labels)) for k in keys])
        self.edges = np.array(self.edges)

    @staticmethod
    def colors(code):
        return np.array([(int(code) >> b) & 1 for b in range(4)])

    @staticmethod
    def node_state(x, y, seed):
        # 单点整数哈希使扩幅保持旧节点，不以“随机”声称强非周期。
        n = ((int(x)*0x9E3779B1) ^ (int(y)*0x85EBCA77) ^ int(seed)) & 0xFFFFFFFF
        n = ((n ^ (n >> 16))*0x7FEB352D) & 0xFFFFFFFF
        n = ((n ^ (n >> 15))*0x846CA68B) & 0xFFFFFFFF
        return (n ^ (n >> 16)) & 1

    def plan(self, span, seed, *, flip=None, uniform=False):
        w, h = self.size
        plan = np.empty((span, span), dtype=int)
        for row, col in np.ndindex(int(np.ceil(span/h)), int(np.ceil(span/w))):
            colors = []
            for dy, dx in ((0, 0), (0, 1), (1, 0), (1, 1)):
                color = 0 if uniform else self.node_state(col+dx, row+dy, seed)
                colors.append(color ^ (flip == (col+dx, row+dy)))
            code = sum(int(c) << b for b, c in enumerate(colors))
            for j, i in np.ndindex(h, w):
                if row*h+j < span and col*w+i < span:
                    plan[row*h+j, col*w+i] = self.ids[(code, i, j)]
        assert mismatches(plan, self.edges) == 0
        return plan

    def weights(self, x, y):
        a, da = transition(x, self.half_core[0], self.size[0])
        b, db = transition(y, self.half_core[1], self.size[1])
        weights = np.stack(((1-a)*(1-b), a*(1-b), (1-a)*b, a*b), axis=-1)
        dx = np.stack((-da*(1-b), da*(1-b), -da*b, da*b), axis=-1)
        dy = np.stack((-(1-a)*db, -a*db, (1-a)*db, a*db), axis=-1)
        return np.stack((weights, dx, dy), axis=-1)

    def phases(self, tile, x, y, case):
        if (case['q'], case['model'], case['frequency']) != self.source_key:
            raise ValueError('Q、函数族或频率变化需重建解析常量；已有图册只直接接受共同源偏移。')
        code, i, j = self.roles[tile]
        x, y = np.broadcast_arrays(x+i, y+j)
        weights = self.weights(x, y)
        corners = np.array([[0, 0], [1, 0], [0, 1], [1, 1]])
        waves = self.lattice.waves
        residual = 2*np.pi*self.increments-np.diag(self.size)@waves.T
        offsets = corners@residual+self.state_offsets[self.colors(code)]
        directions = np.array(list(direction_vectors(case['q'], case['model'])))
        phase = np.einsum('...id,im->...md', weights, offsets)
        phase[..., 0] += np.stack((x, y), axis=-1)@waves.T+directions@np.array(case['shift'])
        phase[..., 1:] += waves
        return phase

    def field(self, tile, x, y, case):
        phase = self.phases(tile, x, y, case)
        return np.concatenate((np.cos(phase[..., :1]), -np.sin(phase[..., :1])*phase[..., 1:]), axis=-1).mean(axis=-2)
