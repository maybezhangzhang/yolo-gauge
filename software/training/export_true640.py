"""Export a trained upstream .pth into the board's per-layer int16-friendly float32 layout.

Same math as export_weights.py (BN fold -> gray-input fold -> |w*512| <= 32767 check
-> little-endian float32 OIHW), but size agnostic: the board IP is built for a fixed
input size, so the model json decides it (640 for the true640 bitstream).

Usage:
  python export_true640.py --checkpoint <pth> [--output <dir>] [--h-drive H:]
"""
import argparse
import copy
import hashlib
import json
import shutil
from datetime import datetime
from pathlib import Path

import numpy as np
import torch
from PIL import Image
from torch import nn

from common import ROOT, letterbox, model_for

BIN_TEMPLATE = ROOT / 'deployment/meiter640_true_sd'


def layer_map():
    result = [(f'backbone.conv{i}', f'BasicConv{i}/w.bin', f'BasicConv{i}/b.bin', True) for i in (1, 2, 3)]
    for i in (1, 2, 3):
        result += [(f'backbone.resblock_body{i}.conv{j}', f'ResBlock{i}/w{j}.bin', f'ResBlock{i}/b{j}.bin', True)
                   for j in (1, 2, 3, 4)]
    result += [('conv_for_P5', 'conv_forP5/w.bin', 'conv_forP5/b.bin', True),
               ('upsample.upsample.0', 'upsample/w.bin', 'upsample/b.bin', True)]
    for head in ('P4', 'P5'):
        result += [(f'yolo_head{head}.0', f'yolo_head{head}/w1.bin', f'yolo_head{head}/b1.bin', True),
                   (f'yolo_head{head}.1', f'yolo_head{head}/w2.bin', f'yolo_head{head}/b2.bin', False)]
    return result


def fused_copy(model):
    model = copy.deepcopy(model).eval()
    for path, _, _, bn in layer_map():
        if bn:
            block = model.get_submodule(path)
            block.conv = torch.nn.utils.fusion.fuse_conv_bn_eval(block.conv, block.bn)
            block.bn = nn.Identity()
    return model


def fold_gray_input(model):
    """Only valid when the three input channels are identical (img_prep.cpp grayscales)."""
    conv = model.backbone.conv1.conv
    with torch.no_grad():
        mean = conv.weight.double().sum(dim=1, keepdim=True).div(3).to(conv.weight.dtype)
        conv.weight.copy_(mean.expand_as(conv.weight))


def export(checkpoint, output, model_json):
    template = json.loads(Path(model_json).read_text(encoding='utf-8'))
    size = int(template['input_size'])
    raw = torch.load(checkpoint, map_location='cpu', weights_only=True)
    if 'model' in raw and isinstance(raw['model'], dict):
        raw = raw['model']
    raw = {k.split('.', 1)[1] if k.startswith('module.') else k: v for k, v in raw.items()}

    original = model_for(template)
    original.load_state_dict(raw, strict=True)
    original = original.eval()

    model = fused_copy(original)
    fold_gray_input(model)

    output = Path(output)
    if output.exists() and any(output.iterdir()):
        raise RuntimeError(f'Export dir not empty: {output}')
    output.mkdir(parents=True, exist_ok=True)

    manifest = {
        'config': template,
        'board_input_size': size,
        'checkpoint': str(checkpoint),
        'checkpoint_sha256': hashlib.sha256(Path(checkpoint).read_bytes()).hexdigest(),
        'exported_at': datetime.now().isoformat(timespec='seconds'),
        'storage': 'little-endian IEEE float32 OIHW weights; board read_params rounds(value*512) into int16',
        'input_contract': 'Three identical grayscale channels required: first conv RGB weights averaged after summation',
        'files': {},
    }

    for path, wpath, bpath, bn in layer_map():
        block = model.get_submodule(path)
        conv = block.conv if bn else block
        for name, tensor in [(wpath, conv.weight), (bpath, conv.bias)]:
            data = tensor.detach().cpu().numpy().astype('<f4')
            if not np.isfinite(data).all():
                raise ValueError(f'{name}: non-finite weights')
            scaled = np.copysign(np.floor(np.abs(data.astype(np.float64) * 512) + .5), data)
            overflow = int(np.sum((scaled < -32768) | (scaled > 32767)))
            if overflow:
                raise ValueError(f'{name}: {overflow} values overflow board int16')
            p = output / name
            p.parent.mkdir(parents=True, exist_ok=True)
            data.tofile(p)
            manifest['files'][name] = {
                'shape': list(data.shape), 'bytes': p.stat().st_size,
                'sha256': hashlib.sha256(p.read_bytes()).hexdigest(),
                'max_abs': float(np.abs(data).max())}
            # Feed back exactly what the board will read.
            readback = torch.from_numpy(np.fromfile(p, dtype='<f4').reshape(data.shape).copy())
            with torch.no_grad():
                tensor.copy_(readback)

    # Numerical check on the real deployment input path (gray -> letterbox114 -> /255).
    bmp = BIN_TEMPLATE / 'test1.bmp'
    with Image.open(bmp) as im:
        data, (scale, dx, dy) = letterbox(im, size=size)
    sample = torch.from_numpy(data)[None]
    with torch.inference_mode():
        a = original(sample)
        b = model(sample)
    errors = [float((x - y).abs().max()) for x, y in zip(a, b)]
    expected = [[27, size // 32, size // 32], [27, size // 16, size // 16]]
    actual = [list(x.shape[1:]) for x in b]
    if actual != expected:
        raise ValueError(f'output shape {actual} != expected {expected}')
    manifest['float_fusion_max_abs_error_per_head'] = errors
    manifest['fusion_check_input'] = str(bmp) + ' via deck letterbox (gray, pad114)'
    manifest['output_shapes'] = actual
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')

    template = dict(template)
    template['source_checkpoint'] = str(checkpoint)
    template['source_sha256'] = manifest['checkpoint_sha256']
    template['weights_exported_at'] = manifest['exported_at']
    template['weights_file_count'] = len(manifest['files']) + 1
    (output.parent / 'gauge_model.json').write_text(json.dumps(template, indent=2), encoding='utf-8')

    print(f'EXPORT_OK size={size} files={len(manifest["files"])} err={errors}', flush=True)
    print(f'  output      : {output}', flush=True)
    print(f'  model json  : {output.parent / "gauge_model.json"}', flush=True)
    print(f'  checkpoint  : {checkpoint} sha={manifest["checkpoint_sha256"]}', flush=True)
    return manifest


def stage(export_dir, drive):
    drive = Path(drive)
    stamp = datetime.now().strftime('%Y%m%d_%H%M')
    old = drive / 'gauge_weights'
    if old.exists():
        backup = drive / f'gauge_weights_backup_{stamp}'
        old.rename(backup)
        print(f'  backup      : {backup}', flush=True)
    shutil.copytree(export_dir, old)
    shutil.copy2(export_dir.parent / 'gauge_model.json', drive / 'gauge_model.json')
    count = sum(1 for _ in old.rglob('*') if _.is_file())
    print(f'  staged      : {old} ({count} files) + gauge_model.json', flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--checkpoint', type=Path, required=True)
    ap.add_argument('--output', type=Path, default=BIN_TEMPLATE / 'gauge_weights')
    ap.add_argument('--model-json', type=Path, default=BIN_TEMPLATE / 'gauge_model.json')
    ap.add_argument('--stage', default='', help='drive root to copy the result onto, e.g. H:')
    args = ap.parse_args()
    torch.set_num_threads(4)
    export(args.checkpoint, args.output, args.model_json)
    if args.stage:
        stage(args.output, args.stage)


if __name__ == '__main__':
    main()
