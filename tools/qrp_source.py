"""当前线性相位 QRP 源及认证整数频率关系；不依赖 C++ 导出或旧拟合器。"""

from fractions import Fraction
from functools import lru_cache

import numpy as np


def direction_vectors(q, model):
    """m=1,...,floor(Q)；三次方向族变换方向分量，不变换空间坐标。"""
    if model not in ('basic', 'cubic-directions'):
        raise ValueError('当前仅支持 basic 与 cubic-directions 线性相位源。')
    for i in range(1, int(np.floor(q))+1):
        angle = 2*np.pi*i/q
        cx, cy = np.cos(angle), np.sin(angle)
        yield (cx**3, cy**3) if model == 'cubic-directions' else (cx, cy)


def expression_jet(q, x, y, model):
    """值及模型坐标的一阶梯度，归一化除以实际项数。"""
    x, y = np.broadcast_arrays(x, y)
    result = np.zeros(x.shape+(3,))
    count = int(np.floor(q))
    for cx, cy in direction_vectors(q, model):
        phase = x*cx+y*cy
        value, derivative = np.cos(phase), -np.sin(phase)
        result += np.stack((value, derivative*cx, derivative*cy), axis=-1)
    return result/count


def source_jet(point, case):
    transformed = case['frequency']*point+case['shift']
    jet = expression_jet(case['q'], transformed[..., 0], transformed[..., 1], case['model'])
    jet[..., 1:] *= case['frequency']
    return jet


def _polynomial_remainder(dividend, divisor):
    value = list(dividend)
    quotient = [0]*max(1, len(value)-len(divisor)+1)
    for high in range(len(value)-1, len(divisor)-2, -1):
        coefficient = value[high]
        quotient[high-len(divisor)+1] = coefficient
        for j, entry in enumerate(divisor):
            value[high-len(divisor)+1+j] -= coefficient*entry
    return quotient, value[:len(divisor)-1]


@lru_cache(None)
def _cyclotomic(n):
    polynomial = [-1]+[0]*(n-1)+[1]
    for d in range(1, n):
        if n % d == 0:
            polynomial, remainder = _polynomial_remainder(polynomial, _cyclotomic(d))
            assert not any(remainder)
    return tuple(polynomial)


def frequency_relations(q, spectrum):
    """规则源频率的圆分关系；所有有限浮点 q 依其精确有理值处理，无逐 q 表。"""
    count = len(spectrum['waves'])
    numerator = Fraction(float(q)).numerator
    if numerator > 2*count*count:
        matrix = np.eye(count, dtype=int)
        degree = None
    else:
        polynomial = _cyclotomic(numerator)
        degree = len(polynomial)-1
        if degree >= count:
            matrix = np.eye(count, dtype=int)
        else:
            matrix = np.array([_polynomial_remainder([0]*i+[1], polynomial)[1]+[0]*max(0, degree-i-1)
                               for i in range(count)], dtype=int)
    generators = spectrum['waves'][:matrix.shape[1]]
    error = float(np.max(np.abs(matrix@generators-spectrum['waves'])))
    assert error < 1e-10
    return matrix, {'rational_q': str(Fraction(float(q))), 'cyclotomic_degree': degree,
                    'generator_count': matrix.shape[1], 'relation_count': count-matrix.shape[1],
                    'frequency_reconstruction_max_error': error, 'matrix': matrix.tolist()}


class PhaseLattice:
    """线性 QRP 的共同整数格；几何图册和取支算术分别拥有自己的状态。"""

    def __init__(self, case):
        if case['model'] not in ('basic', 'cubic-directions'):
            raise ValueError('当前仅支持基础与三次方向线性相位源。')
        self.waves = case['frequency']*np.array(list(direction_vectors(case['q'], case['model'])))
        self.transform = (frequency_relations(case['q'], dict(waves=self.waves))[0]
                          if case['model'] == 'basic' else np.eye(len(self.waves), dtype=int))
        self.qr, self.upper = np.linalg.qr(self.transform.astype(float), mode='reduced')

    def integer_state(self, centers):
        target = centers@self.waves.T/(2*np.pi)
        projected = target@self.qr
        state = np.zeros_like(projected, dtype=np.int64)
        # 格基上的 nearest-plane：只决定整数支，不拟合图像，不声称全局最近格点。
        for j in reversed(range(self.upper.shape[1])):
            state[:, j] = np.rint((projected[:, j]-state[:, j+1:]@self.upper[j, j+1:])/self.upper[j, j])
        return state@self.transform.T
