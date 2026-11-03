"""把任意图片转成板端能直接读的 24 位无压缩 BMP (testN.bmp)。

板端约束 (deployment/meiter640_true_board/src/main.cpp: bmp_load):
  * 只认 24 位、BI_RGB(不压缩) 的 BMP, 其它一律返回错误码
  * 宽高任意, 由 letterbox 缩放到 NET_SIZE(640); 行距按 4 字节对齐由板端自己处理
  * 文件名必须是 test1.bmp, test2.bmp ... 从 1 开始连续编号, 中断即停
  * 板端会把每个像素按 0.299R+0.587G+0.114B 转成灰度再送网络 (img_prep.cpp:110),
    所以彩色 BMP 也能用, 屏上/调试时还能看到彩色

用法:
    python make_test_bmp.py H:\\1-13-00-14HTM11CP501-2.jpg
        -> 覆盖 H:\\test1.bmp (旧文件自动备份为 test1.bmp.bak)

    python make_test_bmp.py a.jpg b.png --out-dir H:\\ --start 1
        -> H:\\test1.bmp, H:\\test2.bmp

    python make_test_bmp.py a.jpg --gray        强制灰度(旧流程的约定: L -> RGB)
    python make_test_bmp.py a.jpg --fit 640     拉伸到 640x640 正方形(默认保持原比例)
"""
from __future__ import annotations

import argparse
import shutil
from pathlib import Path

from PIL import Image

EXTS = ('.jpg', '.jpeg', '.png', '.bmp', '.webp', '.tif', '.tiff')


def convert(src: Path, dst: Path, gray: bool, fit: int | None) -> dict:
    with Image.open(src) as im:
        im = im.convert('L') if gray else im.convert('RGB')
        if fit:
            im = im.resize((fit, fit), Image.BICUBIC)
        dst.parent.mkdir(parents=True, exist_ok=True)
        if dst.exists():
            backup = dst.with_suffix(dst.suffix + '.bak')
            shutil.copy2(dst, backup)
        im.convert('RGB').save(dst, format='BMP')
        w, h = im.size
    size = dst.stat().st_size
    expect = 54 + ((w * 3 + 3) & ~3) * h
    if size != expect:
        raise RuntimeError(f'BMP 大小异常: {size} != {expect}')
    return {'src': str(src), 'dst': str(dst), 'size': [w, h], 'bytes': size,
            'bpp': 24, 'gray': gray, 'fit': fit}


def main() -> int:
    ap = argparse.ArgumentParser(description='任意图片 -> 板端 24 位 BMP (testN.bmp)')
    ap.add_argument('images', nargs='+', help='源图片 (可多个)')
    ap.add_argument('--out-dir', default='H:\\', help='输出目录, 默认 H:\\')
    ap.add_argument('--start', type=int, default=1, help='起始编号, 默认 1')
    ap.add_argument('--name', default=None, help='指定输出文件名(单张时用), 例如 test1.bmp')
    ap.add_argument('--gray', action='store_true', help='先转灰度再复制成 RGB (旧流程约定)')
    ap.add_argument('--fit', type=int, default=None, help='拉伸到 NxN, 默认保持原比例')
    args = ap.parse_args()

    out_dir = Path(args.out_dir)
    rows = []
    for i, item in enumerate(args.images):
        src = Path(item)
        if not src.is_file():
            raise SystemExit(f'找不到图片: {src}')
        if src.suffix.lower() not in EXTS:
            print(f'[warn] 未预期的扩展名, 仍然尝试: {src.name}')
        if args.name and len(args.images) == 1:
            dst = out_dir / args.name
        else:
            dst = out_dir / f'test{args.start + i}.bmp'
        info = convert(src, dst, args.gray, args.fit)
        rows.append(info)
        print(f"[ok] {info['src']} -> {info['dst']}  {info['size'][0]}x{info['size'][1]} "
              f"24bpp {info['bytes']} bytes")

    if len(rows) > 1:
        print(f'\n板端会按 test1..test{len(rows)} 连续读取; 缺一张就停在前一张。')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
