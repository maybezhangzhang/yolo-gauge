"""在 PyCharm 中运行，使用独立测试集评估仪表检测模型。

可选：python evaluate_gauge.py --model logs_meiter_v1/ep070-loss0.028-val_loss0.109.pth
"""
import argparse
from datetime import datetime
from pathlib import Path
import json
import os

from PIL import Image
from tqdm import tqdm

from utils.utils import get_classes
from utils.utils_map import get_map
from yolo import YOLO

ROOT = Path(__file__).resolve().parent
MODEL = ROOT / "logs_meiter_v1" / "best_epoch_weights.pth"
CLASSES = ROOT / "model_data" / "gauge_classes.txt"
TEST_LIST = ROOT / "meiter_test.txt"


def main(model_path=None):
    model_path = Path(model_path) if model_path else MODEL
    # 项目中 anchors 等默认路径是相对路径。
    os.chdir(ROOT)
    for path in (model_path, CLASSES, TEST_LIST):
        if not path.is_file():
            raise FileNotFoundError(f"缺少文件：{path}；数据列表请先运行 prepare_gauge.py 生成。")

    class_names, _ = get_classes(str(CLASSES))
    records = []
    seen = set()
    for line in TEST_LIST.read_text(encoding="utf-8").splitlines():
        if not line.strip():
            continue
        parts = line.split()
        image_path = Path(parts[0])
        if not image_path.is_file():
            raise FileNotFoundError(image_path)
        image_id = image_path.stem
        if image_id in seen:
            raise ValueError(f"测试图片名称重复：{image_id}")
        seen.add(image_id)
        boxes = []
        for text in parts[1:]:
            left, top, right, bottom, cls = map(int, text.split(","))
            if not 0 <= cls < len(class_names) or right <= left or bottom <= top:
                raise ValueError(f"无效测试标注：{image_id}: {text}")
            boxes.append((left, top, right, bottom, cls))
        records.append((image_id, image_path, boxes))
    if not records:
        raise ValueError("测试列表为空")

    # 每次使用独立目录，避免混入以前的预测结果。
    output = ROOT / "map_out_gauge" / datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    for folder in ("ground-truth", "detection-results"):
        (output / folder).mkdir(parents=True, exist_ok=True)

    model = YOLO(
        model_path=str(model_path), classes_path=str(CLASSES),
        input_shape=[640, 640], phi=0,
        confidence=0.001, nms_iou=0.5, letterbox_image=True,
    )
    print(f"独立测试集：{len(records)} 张图片，输出目录：{output}")
    for image_id, image_path, boxes in tqdm(records, desc="Test"):
        truth = "".join(
            f"{class_names[cls]} {left} {top} {right} {bottom}\n"
            for left, top, right, bottom, cls in boxes
        )
        (output / "ground-truth" / f"{image_id}.txt").write_text(truth, encoding="utf-8")
        with Image.open(image_path) as image:
            model.get_map_txt(image_id, image, class_names, str(output))

    # 保存各类别 AP、Precision、Recall 到 results/results.txt。
    # 关闭旧版绘图接口；不影响指标计算。
    map50 = get_map(0.5, False, score_threhold=0.5, path=str(output))
    summary = {
        "model": str(model_path), "test_list": str(TEST_LIST),
        "images": len(records), "classes": class_names,
        "metric": "VOC mAP@0.5", "map50": float(map50),
        "prediction_confidence": 0.001, "nms_iou": 0.5,
        "precision_recall_confidence": 0.5,
    }
    (output / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(f"测试集 mAP@0.5 = {map50:.2%}")
    print(f"分类别结果：{output / 'results' / 'results.txt'}")
    print("这是目标检测指标，不是仪表读数误差。")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="独立测试集 mAP@0.5 评估")
    parser.add_argument("--model", default=None, help="权重路径，默认 logs_meiter_v1/best_epoch_weights.pth")
    main(parser.parse_args().model)
