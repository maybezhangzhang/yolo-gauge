#ifndef LETTERBOX_H
#define LETTERBOX_H

/*
 * letterbox.h  --  等比缩放 + 灰边填充 (letterbox) 的几何描述
 *
 * 为什么要 letterbox:
 *   训练时送进网络的是 416x416 的方形图, 若把 1024x600 (2.46:1) 直接拉成
 *   416x416, 目标会被横向压缩约 2.4 倍, 网络看到的形状和训练时不一样,
 *   精度会掉。正确做法是等比缩放 (短边贴合 416) 再在两侧/上下补灰边。
 *
 * 坐标约定:
 *   - "网络坐标" network coordinate: 0..NET_SIZE 的像素坐标 (416)
 *   - "归一化坐标" normalized: network / NET_SIZE, 即 0..1
 *   - "源图坐标" source: 原始帧像素坐标 (0..src_w-1, 0..src_h-1)
 *
 * 本文件只放结构体和函数声明, 实现在 img_prep.cpp 里
 * (这样不用往 Vitis 工程里新增源文件)。
 *
 * 两种模式 (由 letterbox_init 的 stretch 参数选择):
 *   stretch = 0 : letterbox, 等比缩放 + 灰边, 目标不变形 (推荐, 但要和训练口径一致)
 *   stretch = 1 : 直接拉伸到 416x416, 横纵比例各自独立 (与 PC 端
 *                 utils.resize_image(..., letterbox_image=False) 完全一致,
 *                 也就是原板端代码的行为)
 * 训练/推理口径必须和模型一致, 否则精度会掉。先用 0 试, 掉点了就改 1。
 */

#define NET_SIZE 640        /* 网络输入边长, 与训练权重对应 */

typedef struct {
    int   net;              /* 网络输入边长, 固定 NET_SIZE */
    int   src_w;            /* 源帧宽 */
    int   src_h;            /* 源帧高 */
    int   new_w;            /* 缩放后内容宽 (<= net) */
    int   new_h;            /* 缩放后内容高 (<= net) */
    int   pad_x;            /* 内容左边框宽度 (灰边) */
    int   pad_y;            /* 内容上边框高度 (灰边) */
    float scale;            /* 内容/源图 的缩放比 = new_w / src_w (stretch 模式下仅供显示) */
    float scale_y;          /* 纵向缩放比; letterbox 模式下 == scale, stretch 模式下 != scale */
    int   stretch;          /* 0 = letterbox(等比+灰边), 1 = 直接拉伸(横纵各自缩放) */
} LetterBox;

/*
 * 根据源帧尺寸算出预处理参数
 *   stretch = 0: 等比缩放 + 灰边 (new_w/new_h < net, pad_x/pad_y > 0)
 *   stretch = 1: 直接拉伸 (new_w = new_h = net, pad_x = pad_y = 0)
 */
void letterbox_init(LetterBox* lb, int src_w, int src_h, int stretch);

/* 网络坐标 (0..416, 不是归一化) -> 源图坐标; 结果自动夹到 [0, src_w-1] */
void letterbox_n2s(const LetterBox* lb, float nx, float ny, float* sx, float* sy);

#endif /* LETTERBOX_H */
