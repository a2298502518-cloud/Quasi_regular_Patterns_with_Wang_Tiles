"""单位 Wang tile 的边匹配与像素组装；边顺序为 S、N、W、E。"""

import numpy as np
from PIL import Image


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
    return Image.fromarray(result)
