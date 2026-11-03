/*
 * img_convert.h ---- 串口收来的裸像素 -> 工程内部统一使用的 BGR888
 *
 * 现有 letterbox / prep_image / draw_box 全部按 "BGR888, 顶行在前, 行距 w*3"
 * 工作 (和 bmp_load 的输出一致), 所以串口图必须先转成这个格式再往下走。
 *
 * 顺带把 "显示区域放不下" 的图按比例缩到屏内 (1024x600), 保证 fb_show 不会
 * 越界, 也保证画框坐标是对着屏幕上那张图的。
 */
#ifndef IMG_CONVERT_H_INCLUDED
#define IMG_CONVERT_H_INCLUDED

#include "xil_types.h"

/* 输出上限: 屏是 1024x600, 超过就按比例缩 */
#define CAM_CONV_MAX_W 1024
#define CAM_CONV_MAX_H 600

/*
 * src : w x h 灰度 (每像素 1 字节) / RGB565 (每像素 2 字节, 高字节在前)
 * dst : 输出 BGR888, 容量 cap 字节
 * 返回 1 成功, 0 表示放不下或者参数非法
 */
int cam_gray_to_bgr888(u8* dst, int cap, const u8* src, int w, int h, int* pw, int* ph);
int cam_rgb565_to_bgr888(u8* dst, int cap, const u8* src, int w, int h, int* pw, int* ph);

#endif /* IMG_CONVERT_H_INCLUDED */
