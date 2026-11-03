# -*- coding: utf-8 -*-
"""按指定输入尺寸，用 IoU 距离 k-means 从训练集标签重算 6 个 anchors。

为什么需要它：anchors 是"像素"尺度，输入尺寸一变，同一批目标框的像素值就跟着变，
旧尺寸的 anchors 再用就是错的，所以训练尺寸 = anchors 尺寸 = 部署尺寸，三者必须一致。

用法：
    python auto_anchors.py 640
    python auto_anchors.py 416
输出：
    model_data/yolo_anchors_meiter_<size>.txt   （6 对，逗号分隔，按面积从小到大）
"""
import argparse
import os
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
# 标签目录（meiter_analog_clean 的 train split）
LABEL_DIR = ROOT / "meiter_analog_clean" / "train" / "labels"
RESTARTS = 5
ITERS = 200


def load_boxes():
    """返回归一化 (w, h) 数组，来自 YOLO 标签文件。"""
    if not LABEL_DIR.is_dir():
        raise SystemExit("找不到标签目录：%s" % LABEL_DIR)
    boxes = []
    for name in sorted(os.listdir(LABEL_DIR)):
        if not name.endswith(".txt"):
            continue
        with open(LABEL_DIR / name, encoding="utf-8") as f:
            for line in f:
                parts = line.split()
                if len(parts) >= 5:
                    boxes.append((float(parts[3]), float(parts[4])))
    if not boxes:
        raise SystemExit("标签目录为空：%s" % LABEL_DIR)
    return np.asarray(boxes, dtype=np.float64)


def iou_one_to_many(box, clusters):
    """box: (2,), clusters: (k,2) -> (k,) IoU，中心对齐的近似算法（与训练一致）。"""
    inter_w = np.minimum(box[0], clusters[:, 0])
    inter_h = np.minimum(box[1], clusters[:, 1])
    inter = inter_w * inter_h
    union = box[0] * box[1] + clusters[:, 0] * clusters[:, 1] - inter
    return inter / np.maximum(union, 1e-12)


def kmeans(boxes, k, iters=ITERS):
    n = boxes.shape[0]
    clusters = boxes[np.random.choice(n, k, replace=False)].copy()
    for _ in range(iters):
        dist = np.stack([1.0 - iou_one_to_many(c, boxes) for c in clusters], axis=1)
        nearest = np.argmin(dist, axis=1)
        for j in range(k):
            members = boxes[nearest == j]
            if len(members):
                clusters[j] = members.mean(axis=0)
    dist = np.stack([1.0 - iou_one_to_many(c, boxes) for c in clusters], axis=1)
    avg_iou = float(np.mean(1.0 - np.min(dist, axis=1)))
    return clusters, avg_iou


def main():
    parser = argparse.ArgumentParser(description="按输入尺寸重算 anchors")
    parser.add_argument("size", type=int, nargs="?", default=640, help="输入边长，32 的倍数")
    parser.add_argument("--k", type=int, default=6, help="anchors 数量，yolov4-tiny 用 6")
    parser.add_argument("--out", default=None, help="输出 txt 路径")
    args = parser.parse_args()

    if args.size % 32 != 0:
        raise SystemExit("输入尺寸必须是 32 的倍数，当前 %d" % args.size)

    np.random.seed(0)
    norm = load_boxes()                     # 归一化 w,h
    boxes = norm * float(args.size)         # 换算成该尺寸下的像素 w,h
    print("训练框数量: %d" % len(boxes))
    print("像素宽高中位数 @%d: %.1f x %.1f" % (args.size, np.median(boxes[:, 0]), np.median(boxes[:, 1])))

    best, best_iou = None, -1.0
    for _ in range(RESTARTS):
        clusters, avg_iou = kmeans(boxes, args.k)
        if avg_iou > best_iou:
            best, best_iou = clusters, avg_iou

    order = np.argsort(best[:, 0] * best[:, 1])
    best = best[order]

    out_path = Path(args.out) if args.out else (ROOT / "model_data" / ("yolo_anchors_meiter_%d.txt" % args.size))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    text = ", ".join("%.1f,%.1f" % (w, h) for w, h in best) + "\n"
    out_path.write_text(text, encoding="utf-8")

    print("平均 IoU: %.4f" % best_iou)
    for i, (w, h) in enumerate(best):
        print("  anchor[%d] = %.1f x %.1f" % (i, w, h))
    print("已写入: %s" % out_path)


if __name__ == "__main__":
    main()
