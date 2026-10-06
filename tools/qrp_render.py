"""唯一显示路径：固定高度配色、线性光 2×2 超采样、纯源/铺砌和图板。"""

import numpy as np
from PIL import Image, ImageDraw, ImageFont

from qrp_source import source_jet
from wang_tiles import assemble


def sample_grid(pixels):
    axis = (np.arange(pixels)+0.5)/pixels
    return np.meshgrid(axis, 1-axis)


def decode_srgb(colors):
    colors = np.asarray(colors)/255.0
    return np.where(colors <= 0.04045, colors/12.92, ((colors+0.055)/1.055)**2.4)


def encode_srgb(linear):
    srgb = np.where(linear <= 0.0031308, linear*12.92, 1.055*linear**(1/2.4)-0.055)
    return np.floor(srgb*255+0.5).astype(np.uint8)


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
        if "accent_level" in style:
            colors[np.abs(values-style["accent_level"]) < style["accent_half_width"]] = style["accent"]
    return colors


def design_raster(values, style):
    size = values.shape[0]//2
    colors = decode_srgb(design_colors(values, style))
    return encode_srgb(colors.reshape(size, 2, size, 2, 3).mean(axis=(1, 3)))


def render(atlas, plans, case, pixels, style, *, cached=True):
    x, y = sample_grid(2*pixels)
    ids = np.unique(np.concatenate([p.ravel() for p in plans]+[np.array([0])]))
    def evaluate(t):
        return design_raster(atlas.field(int(t), x, y, case)[..., 0], style)

    if not cached:
        # 每个实例直接求公式，无图像库；仅为对照复用公共 assemble 的像素坐标约定。
        class DirectTiles:
            def __getitem__(self, tile):
                return evaluate(tile)

        return [assemble(p, DirectTiles()) for p in plans]
    tiles = {int(t): evaluate(t) for t in ids}
    return [assemble(p, tiles) for p in plans]


def source_image(case, span, pixels, style):
    x, y = sample_grid(2*pixels)
    image = Image.new('RGB', (span*pixels, span*pixels))
    for row, col in np.ndindex(span, span):
        point = np.stack((x+col-span/2, y+row-span/2), axis=-1)
        image.paste(Image.fromarray(design_raster(source_jet(point, case)[..., 0], style)),
                    (col*pixels, (span-1-row)*pixels))
    return image


def board(pictures, labels, title, font_path, side=384):
    gap, top, bottom = 16, 86, 12
    image = Image.new('RGB', ((side+gap)*len(pictures)+gap, top+side+bottom), '#f0eee8')
    draw = ImageDraw.Draw(image)
    title_font, label_font = [ImageFont.truetype(str(font_path), size) for size in (23, 19)]
    draw.text((gap, 10), title, font=title_font, fill='#182c35')
    for i, (picture, label) in enumerate(zip(pictures, labels)):
        draw.text((gap+(side+gap)*i, 51), label, font=label_font, fill='#182c35')
        image.paste(picture.resize((side, side), Image.Resampling.LANCZOS), (gap+(side+gap)*i, top))
    return image
