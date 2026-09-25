"""实验图板共用的视觉样式；不依赖某一代实验或生成模型。"""

from pathlib import Path

from PIL import ImageFont


BACKGROUND = "#f0eee8"
INK = "#182c35"
MUTED = "#52646a"
ACCENT = "#9b5a25"


def fonts(font_path: Path):
    return {size: ImageFont.truetype(str(font_path), size) for size in (15, 17, 20, 25, 34)}
