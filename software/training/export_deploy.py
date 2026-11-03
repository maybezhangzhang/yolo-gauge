# -*- coding: utf-8 -*-
"""把训练好的权重导出成板端可直接使用的 ONNX，并生成部署说明。

为什么单独写一个导出脚本：
    yolo.py 自带的 convert_to_onnx() 只支持单输出（只给了一个 "output" 名字，
    dynamic_axes=None），而 yolov4-tiny 是两个输出头（stride32 + stride16），
    必须按名字分别导出，板端才能各按各的 anchors/stride 解码。

产物（默认写到 <repo>/deploy/）：
    yolov4tiny_gauge_<size>_fp32.onnx     双输出 ONNX（output0=stride32, output1=stride16）
    deploy_spec.json                      板端解码所需的全部参数（尺寸/anchors/stride/类别/预处理）
    README_deploy.md                      给板端开发者看的解码与集成说明

用法：
    python export_deploy.py                                   # 640 + logs_meiter_v1/best_epoch_weights.pth
    python export_deploy.py --size 416                        # 用 416 的 anchors 导出
    python export_deploy.py --weights logs_meiter_v1/last_epoch_weights.pth --out deploy_int8src
"""
import argparse
import datetime
import json
import sys
from pathlib import Path

import numpy as np
import torch

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))

from nets.yolo import YoloBody                                   # noqa: E402
from utils.utils import get_anchors, get_classes                # noqa: E402

OPSET = 12


def build_mask(num_anchors):
    """yolov4-tiny 两个头各 3 个先验框：head0(P5,stride32) 用大框 [3,4,5]，head1(P4,stride16) 用小框 [0,1,2]。"""
    if num_anchors != 6:
        raise SystemExit('本导出脚本按 yolov4-tiny 的 6 个 anchors 设计，当前 anchors 有 %d 个' % num_anchors)
    return [[3, 4, 5], [0, 1, 2]]


def load_state_dict(path):
    ckpt = torch.load(str(path), map_location='cpu')
    if isinstance(ckpt, dict) and 'model' in ckpt and not any(
            k.startswith(('backbone', 'yolo_head')) for k in ckpt):
        ckpt = ckpt['model']
    state = {}
    for k, v in ckpt.items():
        state[k[7:] if k.startswith('module.') else k] = v
    return state


def main():
    parser = argparse.ArgumentParser(description='导出 ONNX + 部署说明')
    parser.add_argument('--weights', default='logs_meiter_v1/best_epoch_weights.pth', help='训练产出的 .pth')
    parser.add_argument('--size', type=int, default=640, help='输入边长，必须和训练时一致')
    parser.add_argument('--anchors', default=None, help='anchors 文件，默认 model_data/yolo_anchors_meiter_<size>.txt')
    parser.add_argument('--classes', default='model_data/gauge_classes.txt')
    parser.add_argument('--out', default='deploy', help='产物目录')
    parser.add_argument('--opset', type=int, default=OPSET)
    parser.add_argument('--simplify', action='store_true', help='尝试 onnxsim 简化（没装就跳过）')
    parser.add_argument('--no-check', action='store_true', help='跳过 onnxruntime 数值对齐校验')
    args = parser.parse_args()

    size = args.size
    if size % 32 != 0:
        raise SystemExit('--size 必须是 32 的倍数')

    weights = Path(args.weights)
    if not weights.is_absolute():
        weights = ROOT / weights
    if not weights.is_file():
        raise SystemExit('找不到权重文件：%s（先训练出 logs_meiter_v1/best_epoch_weights.pth）' % weights)

    anchors_file = Path(args.anchors) if args.anchors else ROOT / 'model_data' / ('yolo_anchors_meiter_%d.txt' % size)
    if not anchors_file.is_file():
        raise SystemExit('找不到 anchors 文件：%s（先运行 python auto_anchors.py %d）' % (anchors_file, size))

    class_names, num_classes = get_classes(str(ROOT / args.classes))
    anchors, num_anchors = get_anchors(str(anchors_file))
    anchors_mask = build_mask(num_anchors)
    anchors = np.asarray(anchors, dtype=np.float64)

    print('权重     : %s' % weights)
    print('anchors  : %s' % anchors_file)
    print('尺寸     : %d x %d' % (size, size))
    print('类别 (%d): %s' % (num_classes, class_names))

    net = YoloBody(anchors_mask, num_classes, phi=0)
    state = load_state_dict(weights)
    missing, unexpected = net.load_state_dict(state, strict=False)
    if missing or unexpected:
        raise SystemExit('权重和网络结构不匹配。\n缺失: %s\n多余: %s' % (missing[:8], unexpected[:8]))
    net.eval()

    out_dir = Path(args.out)
    if not out_dir.is_absolute():
        out_dir = ROOT / out_dir
    out_dir.mkdir(parents=True, exist_ok=True)
    onnx_path = out_dir / ('yolov4tiny_gauge_%d_fp32.onnx' % size)

    dummy = torch.randn(1, 3, size, size)
    with torch.no_grad():
        torch.onnx.export(
            net, dummy, str(onnx_path),
            input_names=['images'],
            output_names=['output0', 'output1'],
            opset_version=args.opset,
            do_constant_folding=True,
            dynamo=False,          # 用稳定的 TorchScript 导出器，Vitis AI 兼容性最好
        )
    print('已导出 ONNX: %s (%.2f MB)' % (onnx_path, onnx_path.stat().st_size / 1024 / 1024))

    try:
        import onnx
        model = onnx.load(str(onnx_path))
        onnx.checker.check_model(model)
        print('onnx.checker: OK')
        if args.simplify:
            try:
                import onnxsim
                model, ok = onnxsim.simplify(model)
                if ok:
                    onnx.save(model, str(onnx_path))
                    print('onnxsim: 已简化')
            except ImportError:
                print('onnxsim 未安装，跳过简化')
    except ImportError:
        print('未安装 onnx，跳过结构校验（pip install onnx）')

    # ---- 数值对齐：确保板端拿到的图和 torch 端一致 ----
    if not args.no_check:
        try:
            import onnxruntime as ort
            sess = ort.InferenceSession(str(onnx_path), providers=['CPUExecutionProvider'])
            ref = sess.run(None, {'images': dummy.numpy()})
            with torch.no_grad():
                mine = net(dummy)
            for i, (a, b) in enumerate(zip(ref, mine)):
                diff = float(np.max(np.abs(a - b.numpy())))
                print('output%d 最大绝对误差: %.3e  shape=%s' % (i, diff, tuple(a.shape)))
                if diff > 1e-3:
                    print('  [警告] 误差偏大，检查是否有算子被改写')
        except ImportError:
            print('未安装 onnxruntime，跳过数值对齐（pip install onnxruntime）')

    # ---- 板端解析需要的参数 ----
    strides = [32, 16]
    levels = []
    for i, s in enumerate(strides):
        idx = anchors_mask[i]
        levels.append({
            'output': 'output%d' % i,
            'stride': s,
            'grid': [size // s, size // s],
            'channels': len(idx) * (5 + num_classes),
            'anchors_px': [[float(anchors[j][0]), float(anchors[j][1])] for j in idx],
            'attr_layout': ('[bs, {c}, {g}, {g}] -> reshape [bs, {a}, {k}, {g}, {g}] '
                            '-> permute to [bs, {g}, {g}, {a}, {k}]').format(
                                c=len(idx) * (5 + num_classes), g=size // s, a=len(idx), k=5 + num_classes),
        })

    spec = {
        'created_at': datetime.datetime.now().isoformat(timespec='seconds'),
        'task': 'gauge detection (base / maximum / minimum / tip)',
        'model': {'name': 'yolov4-tiny', 'params': int(sum(p.numel() for p in net.parameters())),
                  'forward_outputs': 2},
        'input': {
            'name': 'images', 'shape': [1, 3, size, size], 'layout': 'NCHW', 'dtype': 'float32',
            'color': 'RGB',
            'scale': 'x / 255.0',
            'mean': None, 'std': None,
            'resize': '直接 resize 到 %dx%d（训练时是直接缩放，没有 letterbox）' % (size, size),
            'letterbox_if_needed': {'pad_value': 114, 'note': '只有长宽比差异很大时才需要，且要把框按 pad/scale 反映射'},
        },
        'outputs': levels,
        'decode': {
            'center': 'bx = (sigmoid(p0) + gx) * stride ; by = (sigmoid(p1) + gy) * stride',
            'size': 'bw = exp(p2) * anchor_w ; bh = exp(p3) * anchor_h   (anchor 用本文件 anchors_px，已经是该尺寸下的像素值)',
            'box': 'x1=bx-bw/2, y1=by-bh/2, x2=bx+bw/2, y2=by+bh/2   （像素坐标，除以 %d 得到 0~1 归一化）' % size,
            'score': 'conf = sigmoid(p4) ; cls = sigmoid(p5:5+nc) ; score = conf * cls',
            'post': 'conf 阈值 0.5，NMS IoU 0.3（训练时评估用的就是这两个值）',
        },
        'classes': class_names,
        'anchors_file': str(anchors_file.name),
        'anchors_all_px': anchors.tolist(),
    }
    spec_path = out_dir / 'deploy_spec.json'
    spec_path.write_text(json.dumps(spec, ensure_ascii=False, indent=2), encoding='utf-8')
    print('已写入部署参数: %s' % spec_path)

    (out_dir / 'README_deploy.md').write_text(
        render_readme(size, num_classes, class_names, anchors, anchors_mask, strides, onnx_path.name),
        encoding='utf-8')
    print('已写入说明文档: %s' % (out_dir / 'README_deploy.md'))
    print('\n完成。板端只需要：ONNX（先转成 DPU 的 xmodel）+ deploy_spec.json。')


def render_readme(size, num_classes, class_names, anchors, anchors_mask, strides, onnx_name):
    lines = []
    lines.append('# 表盘检测模型 部署说明（YOLOv4-tiny / %d x %d）\n' % (size, size))
    lines.append('本文件由 `export_deploy.py` 自动生成，所有数值都和训练时一致，直接照抄即可。\n')
    lines.append('\n## 1. 文件\n')
    lines.append('| 文件 | 说明 |\n|---|---|\n')
    lines.append('| `%s` | 双输出 ONNX，opset 12，输入 `images` (1,3,%d,%d) float32 RGB |\n' % (onnx_name, size, size))
    lines.append('| `deploy_spec.json` | 解码所需的 anchors / stride / 类别等全部参数 |\n')
    lines.append('\n## 2. 预处理（必须完全一致）\n')
    lines.append('1. BGR 读到图后转 **RGB**；\n')
    lines.append('2. 直接 resize 到 **%d x %d**（训练时没有 letterbox，别加 pad）；\n' % (size, size))
    lines.append('3. 像素值 **除以 255**，**不要**减 mean / 除 std；\n')
    lines.append('4. HWC -> CHW，整体做成 (1,3,%d,%d) float32。\n' % (size, size))
    lines.append('\n## 3. 输出与解码\n')
    lines.append('两个输出头：`output0` 是 stride 32（%dx%d 网格），`output1` 是 stride 16（%dx%d 网格）。\n'
                 % (size // 32, size // 32, size // 16, size // 16))
    g32 = size // 32
    a = len(anchors_mask[0])
    k = 5 + num_classes
    lines.append('以 `output0` 为例，形状是 (1, %d, %d, %d)，先 reshape 成 (1, %d, %d, %d, %d)'
                 % (a * k, g32, g32, a, k, g32, g32))
    lines.append('再 permute 成 (1, %d, %d, %d, %d)，即 `out[0][gy][gx][a]` 是第 a 个锚在网格 (gx,gy) 的 %d 个数。\n'
                 % (g32, g32, a, k, k))
    lines.append('```c\n')
    lines.append('for (gx, gy, a):\n')
    lines.append('    p   = out[0][gy][gx][a]              // p[0..3] 位置项, p[4] obj, p[5..] cls\n')
    lines.append('    bx  = (sigmoid(p[0]) + gx) * stride   // stride = 32 (output0) / 16 (output1)\n')
    lines.append('    by  = (sigmoid(p[1]) + gy) * stride\n')
    lines.append('    bw  = exp(p[2]) * anchor_w[a]         // anchor 见 deploy_spec.json 的 anchors_px\n')
    lines.append('    bh  = exp(p[3]) * anchor_h[a]\n')
    lines.append('    conf = sigmoid(p[4]);  cls = argmax(sigmoid(p[5..]))\n')
    lines.append('    x1 = (bx - bw/2) / %d;  y1 = (by - bh/2) / %d;   // 归一化坐标\n' % (size, size))
    lines.append('    x2 = (bx + bw/2) / %d;  y2 = (by + bh/2) / %d\n' % (size, size))
    lines.append('```\n')
    lines.append('\n后处理：`score = conf * cls_sigmoid`，阈值 **0.5**，同类 NMS IoU **0.3**（与训练期评估一致）。\n')
    lines.append('\n## 4. anchors（像素，已按 %d 输入换算好）\n' % size)
    lines.append('| head | stride | anchors (w,h) |\n|---|---|---|\n')
    for i, s in enumerate(strides):
        row = ', '.join('(%.1f, %.1f)' % (anchors[j][0], anchors[j][1]) for j in anchors_mask[i])
        lines.append('| output%d | %d | %s |\n' % (i, s, row))
    lines.append('\n类别顺序（务必一致）：`%s`\n' % ', '.join(class_names))
    lines.append('\n## 5. 上板注意\n')
    lines.append('- %d 输入的单帧计算量约 **16.1 GFLOPs**（416 约 6.8 GFLOPs），Zynq-7020 的 DPU 属于小算力档，\n'
                 '  640 会明显吃紧（帧率个位数）；如果板端要实时，**重新用 416 训练一次**（`train.bat 416`）再导出，\n'
                 '  因为 anchors 是像素尺度，尺寸必须和训练一致，不能拿 640 的权重直接导 416。\n' % size)
    lines.append('- 量化（INT8）建议用一批真实表盘图做校准集，注意校准图也要 640/416 原分辨率缩放后的分布。\n')
    lines.append('- DPU 上输入通常是 NHWC，`images` 可能需要自己插一个 transpose；转换脚本里说明清楚。\n')
    return ''.join(lines)


if __name__ == '__main__':
    main()
