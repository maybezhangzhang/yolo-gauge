#ifndef OSD_H
#define OSD_H

#include "sd_io.h"      /* u8 等类型 */

/*
 * osd.h  --  屏幕字符/图元绘制 (OSD = On Screen Display)
 *
 * framebuffer 是 24 位 BGR (和 BMP 一样的字节序), 行距 = fbw*3。
 * 所有颜色参数都按 (b, g, r) 顺序, 和 framebuffer 字节顺序一致。
 *
 * 字符集只有 ASCII 0x20..0x7E (8x16 点阵, 来自 font8x16.h),
 * 所以屏上文字只能用英文/数字/标点 —— 汉字不上屏, 但可以走串口。
 */

/* 填充矩形 (自动裁剪到屏幕内) */
void osd_fill(u8* fb, int fbw, int fbh,
              int x, int y, int w, int h,
              u8 b, u8 g, u8 r);

/* 矩形边框, t = 线宽 */
void osd_box(u8* fb, int fbw, int fbh,
             int x, int y, int w, int h, int t,
             u8 b, u8 g, u8 r);

/* 画字符串, scale=1 -> 8x16, scale=2 -> 16x32, 字符间距 1*scale 像素 */
void osd_text(u8* fb, int fbw, int fbh,
              int x, int y, const char* s, int scale,
              u8 b, u8 g, u8 r);

/* 带背景块的字符串 (标签用, 保证任何底色上都看得清) */
void osd_text_bg(u8* fb, int fbw, int fbh,
                 int x, int y, const char* s, int scale,
                 u8 fb_b, u8 fb_g, u8 fb_r,      /* 文字色 */
                 u8 bg_b, u8 bg_g, u8 bg_r);     /* 背景色 */

/* 字符串像素宽度 (-1 是去掉最后一列间距), 用于排版 */
int osd_text_w(const char* s, int scale);
int osd_text_h(int scale);

#endif /* OSD_H */
