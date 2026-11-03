/*
 * img_prep.cpp  --  416x416 网络输入的预处理 (含 letterbox)
 *
 * 与旧版本的区别:
 *   旧: 1024x600 直接拉成 416x416 (横向压缩 2.46 倍), 目标严重变形;
 *   新: 等比缩放 + 灰边填充, 保持长宽比 (stretch=0);
 *       也可以退回旧行为 (stretch=1, 与 PC 端 resize_image(letterbox_image=False)
 *       完全一致)—— 两种的口径必须和训练时一致, 不确定就先试 0, 掉点再换 1。
 *
 * 性能: 全整数运算, 无浮点除法在热点循环里, 1024x600 约 3~8 ms。
 */
#include "img_prep.h"

#define NET (NET_SIZE)
#define NET2 (NET * NET)

/* letterbox 灰边亮度: YOLO 系列惯例用 114 (0..255 灰), 三通道相同 */
#define PAD_LEVEL 114

/* 灰边在 data_t 域里的值 = round(114/255*SCALE) */
#define PAD_DATA ((data_t)(((PAD_LEVEL) * SCALE + 127) / 255))

void letterbox_init(LetterBox* lb, int src_w, int src_h, int stretch) {
    lb->net   = NET;
    lb->src_w = src_w;
    lb->src_h = src_h;
    lb->stretch = stretch ? 1 : 0;

    if (lb->stretch) {
        /* 直接拉伸: 横纵各自缩放, 内容铺满 416x416, 无灰边 */
        lb->new_w  = NET;
        lb->new_h  = NET;
        lb->pad_x  = 0;
        lb->pad_y  = 0;
        lb->scale   = (float)NET / (float)src_w;
        lb->scale_y = (float)NET / (float)src_h;
        return;
    }

    float sx = (float)NET / (float)src_w;   /* 按宽贴满需要的比例 */
    float sy = (float)NET / (float)src_h;   /* 按高贴满需要的比例 */
    float s  = (sx < sy) ? sx : sy;         /* 取小者 = 长边贴满, 内容完整 */

    lb->scale   = s;
    lb->scale_y = s;

    int nw = (int)(src_w * s + 0.5f);
    int nh = (int)(src_h * s + 0.5f);
    if (nw > NET) nw = NET;
    if (nh > NET) nh = NET;
    if (nw < 1)  nw = 1;
    if (nh < 1)  nh = 1;

    lb->new_w = nw;
    lb->new_h = nh;
    lb->pad_x = (NET - nw) / 2;     /* 居中留边 */
    lb->pad_y = (NET - nh) / 2;
}

void letterbox_n2s(const LetterBox* lb, float nx, float ny, float* sx, float* sy) {
    float x = nx - (float)lb->pad_x;    /* 去掉左边框 */
    float y = ny - (float)lb->pad_y;    /* 去掉上边框 */
    if (lb->scale > 1e-6f) {
        x /= lb->scale;                 /* 还原到源图尺度 */
    }
    if (lb->scale_y > 1e-6f) {
        y /= lb->scale_y;
    }
    if (x < 0.0f) x = 0.0f;
    if (y < 0.0f) y = 0.0f;
    if (x > (float)(lb->src_w - 1)) x = (float)(lb->src_w - 1);
    if (y > (float)(lb->src_h - 1)) y = (float)(lb->src_h - 1);
    *sx = x;
    *sy = y;
}

void prep_image(const u8* bgr, int sw, int sh, const LetterBox* lb, data_t* out) {
    static int xm[NET];     /* 输出列 -> 源图列 */
    static int ym[NET];     /* 输出行 -> 源图行 */

    /* 1) 三通道全部先填成灰边 */
    for (int i = 0; i < NET2; i++) {
        out[i]          = PAD_DATA;
        out[NET2 + i]   = PAD_DATA;
        out[2 * NET2 + i] = PAD_DATA;
    }

    /* 2) 预计算最近邻查表: 用整数比例, 避免逐像素累加误差 */
    for (int i = 0; i < lb->new_w; i++) {
        int v = (int)((long)i * (long)lb->src_w / (long)lb->new_w);
        if (v >= sw) v = sw - 1;
        xm[i] = v;
    }
    for (int j = 0; j < lb->new_h; j++) {
        int v = (int)((long)j * (long)lb->src_h / (long)lb->new_h);
        if (v >= sh) v = sh - 1;
        ym[j] = v;
    }

    /* 3) 只处理内容区 (其余已经是灰边) */
    for (int y = 0; y < lb->new_h; y++) {
        const u8* srow = bgr + (long)ym[y] * (long)sw * 3;
        int dy = y + lb->pad_y;
        data_t* rp = out + (long)dy * NET + lb->pad_x;
        data_t* gp = rp + NET2;
        data_t* bp = rp + 2 * NET2;
        for (int x = 0; x < lb->new_w; x++) {
            const u8* p = srow + (long)xm[x] * 3;
            /* BMP/帧缓冲是 BGR 顺序: p[0]=B p[1]=G p[2]=R */
            int gray = (19595*p[2] + 38470*p[1] + 7471*p[0] + 32768) >> 16;
            rp[x] = gp[x] = bp[x] = (data_t)((gray*SCALE + 127)/255);
        }
    }
}
