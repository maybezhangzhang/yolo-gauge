/*
 * osd.cpp  --  OSD 字符/图元绘制实现
 *
 * 说明:
 *   1. 纯 C++ 逻辑, 不依赖任何 Xilinx 驱动, 方便在 PC 上一起验证。
 *   2. 每个像素都做边界检查, 越界的框/文字不会踩坏内存。
 *   3. 字模在 font8x16.h 里: 95 个可打印字符 x 16 行, 每行 1 字节,
 *      bit7 = 最左边那一列。
 *   4. 颜色顺序全部是 (b, g, r), 与 framebuffer 字节顺序一致。
 */
#include "osd.h"
#include "font8x16.h"

/* ------------------------------------------------------------------ */
/* 基础图元                                                            */
/* ------------------------------------------------------------------ */

static void put_px(u8* fb, int fbw, int fbh, int x, int y,
                   u8 b, u8 g, u8 r) {
    if (x < 0 || y < 0 || x >= fbw || y >= fbh) return;   /* 裁剪 */
    u8* p = fb + ((long)y * fbw + x) * 3;
    p[0] = b; p[1] = g; p[2] = r;
}

void osd_fill(u8* fb, int fbw, int fbh,
              int x, int y, int w, int h,
              u8 b, u8 g, u8 r) {
    if (w <= 0 || h <= 0) return;
    /* 先在屏幕坐标系里裁剪 */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > fbw) w = fbw - x;
    if (y + h > fbh) h = fbh - y;
    if (w <= 0 || h <= 0) return;

    for (int j = 0; j < h; j++) {
        u8* p = fb + ((long)(y + j) * fbw + x) * 3;
        for (int i = 0; i < w; i++) {
            p[0] = b; p[1] = g; p[2] = r;
            p += 3;
        }
    }
}

void osd_box(u8* fb, int fbw, int fbh,
             int x, int y, int w, int h, int t,
             u8 b, u8 g, u8 r) {
    if (w <= 0 || h <= 0 || t <= 0) return;
    if (t > w / 2) t = w / 2;
    if (t > h / 2) t = h / 2;
    if (t < 1) t = 1;
    osd_fill(fb, fbw, fbh, x, y,             w, t, b, g, r);  /* 上边 */
    osd_fill(fb, fbw, fbh, x, y + h - t,     w, t, b, g, r);  /* 下边 */
    osd_fill(fb, fbw, fbh, x, y,             t, h, b, g, r);  /* 左边 */
    osd_fill(fb, fbw, fbh, x + w - t, y,     t, h, b, g, r);  /* 右边 */
}

/* ------------------------------------------------------------------ */
/* 字符                                                                */
/* ------------------------------------------------------------------ */

/* 取一个字符的 16 行点阵; 非法字符统一画 '?' */
static const unsigned char* glyph_of(char c) {
    int idx = (int)(unsigned char)c - FONT_FIRST;
    if (idx < 0 || idx > (FONT_LAST - FONT_FIRST)) idx = (int)'?' - FONT_FIRST;
    return font8x16[idx];
}

int osd_text_w(const char* s, int scale) {
    if (!s || !*s) return 0;
    int n = 0;
    while (s[n]) n++;
    return n * FONT_ADVANCE * scale - scale;   /* 末尾少一个间隔 */
}

int osd_text_h(int scale) {
    return FONT_H * scale;
}

void osd_text(u8* fb, int fbw, int fbh,
              int x, int y, const char* s, int scale,
              u8 cb, u8 cg, u8 cr) {
    if (!s || scale < 1) return;

    int cx = x;
    for (; *s; s++) {
        const unsigned char* gp = glyph_of(*s);
        for (int row = 0; row < FONT_H; row++) {
            unsigned char bits = gp[row];
            if (!bits) continue;
            for (int col = 0; col < FONT_W; col++) {
                if (!(bits & (0x80u >> col))) continue;
                /* 一个点放成 scale x scale 的方块 */
                for (int dy = 0; dy < scale; dy++) {
                    int py = y + row * scale + dy;
                    if (py < 0 || py >= fbh) continue;
                    for (int dx = 0; dx < scale; dx++) {
                        put_px(fb, fbw, fbh, cx + col * scale + dx, py,
                               cb, cg, cr);
                    }
                }
            }
        }
        cx += FONT_ADVANCE * scale;
    }
}

void osd_text_bg(u8* fb, int fbw, int fbh,
                 int x, int y, const char* s, int scale,
                 u8 fb_b, u8 fb_g, u8 fb_r,
                 u8 bg_b, u8 bg_g, u8 bg_r) {
    int w = osd_text_w(s, scale);
    int h = osd_text_h(scale);
    if (w > 0 && h > 0) {
        osd_fill(fb, fbw, fbh, x - 2, y - 1, w + 4, h + 2,
                 bg_b, bg_g, bg_r);
    }
    osd_text(fb, fbw, fbh, x, y, s, scale, fb_b, fb_g, fb_r);
}
