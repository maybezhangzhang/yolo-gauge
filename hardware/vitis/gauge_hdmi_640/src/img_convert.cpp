/*
 * img_convert.cpp ---- 裸像素 -> BGR888 (最近邻缩放 + 格式转换)
 */
#include "img_convert.h"

/* 算缩放参数: 输出尺寸不超过 max_w x max_h, 保持长宽比 (16.16 定点) */
static void plan_scale(int w, int h, int max_w, int max_h,
                       int* ow, int* oh, u32* xs, u32* ys)
{
    int tw = w, th = h;

    if (tw > max_w) {
        th = (int)(((long)th * max_w) / tw);
        if (th < 1) th = 1;
        tw = max_w;
    }
    if (th > max_h) {
        tw = (int)(((long)tw * max_h) / th);
        if (tw < 1) tw = 1;
        th = max_h;
    }

    *ow = tw;
    *oh = th;
    *xs = (u32)(((u64)w << 16) / (u32)tw);
    *ys = (u32)(((u64)h << 16) / (u32)th);
}

int cam_gray_to_bgr888(u8* dst, int cap, const u8* src, int w, int h, int* pw, int* ph)
{
    int ow, oh;
    u32 xs, ys;
    int x, y;
    u32 sy = 0u;

    if (dst == 0 || src == 0 || w <= 0 || h <= 0) return 0;

    plan_scale(w, h, CAM_CONV_MAX_W, CAM_CONV_MAX_H, &ow, &oh, &xs, &ys);
    if ((long)ow * oh * 3L > (long)cap) return 0;

    for (y = 0; y < oh; y++) {
        const u8* srow = src + (u32)(sy >> 16) * (u32)w;
        u8* drow = dst + (u32)y * (u32)ow * 3u;
        u32 sx = 0u;
        for (x = 0; x < ow; x++) {
            u8 v = srow[sx >> 16];
            drow[0] = v;
            drow[1] = v;
            drow[2] = v;
            drow += 3;
            sx += xs;
        }
        sy += ys;
    }

    *pw = ow;
    *ph = oh;
    return 1;
}

int cam_rgb565_to_bgr888(u8* dst, int cap, const u8* src, int w, int h, int* pw, int* ph)
{
    int ow, oh;
    u32 xs, ys;
    int x, y;
    u32 sy = 0u;

    if (dst == 0 || src == 0 || w <= 0 || h <= 0) return 0;

    plan_scale(w, h, CAM_CONV_MAX_W, CAM_CONV_MAX_H, &ow, &oh, &xs, &ys);
    if ((long)ow * oh * 3L > (long)cap) return 0;

    for (y = 0; y < oh; y++) {
        const u8* srow = src + (u32)(sy >> 16) * (u32)w * 2u;
        u8* drow = dst + (u32)y * (u32)ow * 3u;
        u32 sx = 0u;
        for (x = 0; x < ow; x++) {
            u32 idx = (sx >> 16) * 2u;
            u32 p   = ((u32)srow[idx]) | ((u32)srow[idx + 1u] << 8);  /* 低字节在前 (OpenMV to_bytes 原样输出) */
            u32 r   = (p >> 11) & 0x1Fu;
            u32 g   = (p >> 5)  & 0x3Fu;
            u32 b   =  p        & 0x1Fu;
            drow[0] = (u8)((b << 3) | (b >> 2));   /* B */
            drow[1] = (u8)((g << 2) | (g >> 4));   /* G */
            drow[2] = (u8)((r << 3) | (r >> 2));   /* R */
            drow += 3;
            sx += xs;
        }
        sy += ys;
    }

    *pw = ow;
    *ph = oh;
    return 1;
}
