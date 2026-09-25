"""Endpoint-coded Wang catalog and pixel-only assembly; layouts have bottom-up rows."""

import numpy as np
from PIL import Image


def endpoint_edges(tile_id):
    sw, se, nw, ne = ((tile_id >> i) & 1 for i in range(4))
    return (2 * sw + se, 2 * nw + ne, 2 * sw + nw, 2 * se + ne)


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
