#ifndef YOLO_DECODE_H
#define YOLO_DECODE_H
#include "sd_io.h"
#include "letterbox.h"

#include "gauge_model.h"
#define CONF_THRES  0.25f
#define NMS_THRES   0.3f

typedef struct {
    float x1, y1, x2, y2;    /* 已经换算到源图 (原始帧) 像素坐标 */
    float score;
    int   cls;
} Detection;

/*
 * 告诉解码器当前用的 letterbox 参数 (由 main 每帧调用)。
 * 解码器据此把网络坐标还原成源图像素坐标 —— 不做这一步的话,
 * 画出来的框会整体偏上/偏下 (灰边被当成了图像内容)。
 */
void yolo_set_letterbox(const LetterBox* lb);

/* 解码两个特征图 (out0:13x13, out1:26x26), 返回检测框数量, 结果写入 dets */
int yolo_decode(const data_t* out0, const data_t* out1,
                Detection* dets, int max_dets);

#endif
