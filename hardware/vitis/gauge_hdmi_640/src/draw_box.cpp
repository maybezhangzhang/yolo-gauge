/*
 * draw_box.cpp  --  检测框 + 屏上标签
 *
 * 相对旧版本的修改:
 *   1. 旧版第一行就是 (void)label, 屏上不写字, 只有串口输出 ——
 *      现场演示时根本不知道哪个框是哪个类, 现在补上 8x16 点阵标签;
 *   2. 支持屏幕偏移 (ox, oy): 帧按原始分辨率居中贴在 1024x600 上时,
 *      框必须跟着平移;
 *   3. 旧版 set_pixel 只检查了 x, 没检查 y (越界的 y 会写到 fb 之外),
 *      现在统一用 osd.h 里的裁剪函数。
 */
#include "draw_box.h"
#include "osd.h"
#include <stdio.h>

extern float g_display_scale;

// 6 个类别的 BGR 颜色
static const u8 CLASS_COLORS[6][3] = {
    {0,   0,   255}, // ore carrier      - 红
    {0,   255, 0  }, // bulk cargo       - 绿
    {255, 0,   0  }, // container        - 蓝
    {0,   255, 255}, // general cargo    - 黄
    {255, 0,   255}, // fishing boat     - 紫
    {255, 255, 0  }  // passenger ship   - 青
};

void draw_detection(u8* fb, int fbw, int fbh,
                    const Detection* d, const char* label,
                    int ox, int oy) {
    int x1 = (int)(d->x1 * g_display_scale + 0.5f) + ox;
    int y1 = (int)(d->y1 * g_display_scale + 0.5f) + oy;
    int x2 = (int)(d->x2 * g_display_scale + 0.5f) + ox;
    int y2 = (int)(d->y2 * g_display_scale + 0.5f) + oy;

    // 夹在屏幕内
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > fbw - 1) x2 = fbw - 1;
    if (y2 > fbh - 1) y2 = fbh - 1;
    if (x2 <= x1 || y2 <= y1) return;         // 退化框直接丢弃

    int cls = d->cls % 6;
    if (cls < 0) cls = 0;
    const u8* c = CLASS_COLORS[cls];

    // 3 像素边框
    osd_box(fb, fbw, fbh, x1, y1, x2 - x1 + 1, y2 - y1 + 1, 3,
            c[0], c[1], c[2]);

    // 标签: 类名 + 置信度, 例如 "container 0.87"
    if (label && *label) {
        char txt[64];
        int sc = (int)(d->score * 100.0f + 0.5f);      /* 0..100 */
        if (sc < 0) sc = 0;
        if (sc > 100) sc = 100;
        snprintf(txt, sizeof(txt), "%s %d.%02d", label, sc / 100, sc % 100);

        int tw = osd_text_w(txt, 1);
        int th = osd_text_h(1);
        int lx = x1;
        int ly = y1 - th - 3;                 /* 默认放在框上方 */
        if (ly < 0) ly = y1 + 3;              /* 顶到屏幕就放到框内 */
        if (lx + tw + 4 > fbw) lx = fbw - tw - 4;
        if (lx < 0) lx = 0;

        /* 标签底色是类别色, 文字颜色按亮度自动取黑/白, 保证看得清
         * (纯蓝底配黑字几乎看不见, 必须用白字) */
        int luma = (c[2] * 299 + c[1] * 587 + c[0] * 114) / 1000;  /* 0..255 */
        u8 tr, tg, tb;
        if (luma > 140) { tb = 0;   tg = 0;   tr = 0;   }          /* 亮底黑字 */
        else            { tb = 255; tg = 255; tr = 255; }          /* 暗底白字 */

        osd_fill(fb, fbw, fbh, lx - 2, ly - 1, tw + 4, th + 2,
                 c[0], c[1], c[2]);           /* 类色底 */
        osd_text(fb, fbw, fbh, lx, ly, txt, 1, tb, tg, tr);
    }
}
