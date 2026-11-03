#include "gauge_overlay.h"
#include "gauge_reading.h"
#include "pointer_cv.h"
#include "osd.h"
#include "xil_printf.h"
#include <stdio.h>
#include <math.h>

extern float g_display_scale;

#define GAUGE_RAD_PER_DEG  0.01745329252f
#define GAUGE_DEG_PER_RAD  57.29577951f

/* Thin line drawn with the primitive we already have, so no extra graphics
 * dependency is needed for the CV needle. */
static void gauge_line(u8* fb, int fw, int fh, int x0, int y0, int x1, int y1,
                       u8 r, u8 g, u8 b)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    for (;;) {
        osd_fill(fb, fw, fh, x0, y0, 2, 2, r, g, b);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

static void gauge_mark(u8* fb, int fw, int fh, int sx, int sy, const char* label,
                       u8 r, u8 g, u8 b)
{
    osd_fill(fb, fw, fh, sx - 5, sy, 11, 1, r, g, b);
    osd_fill(fb, fw, fh, sx, sy - 5, 1, 11, r, g, b);
    if (label && label[0]) osd_text_bg(fb, fw, fh, sx + 6, sy, label, 1, r, g, b, 0, 0, 0);
}

void gauge_overlay(u8* fb, int fw, int fh, const Detection* d, int n,
                   const u8* img, int iw, int ih, int ox, int oy)
{
    int counts[4] = { 0, 0, 0, 0 };
    int ids[4] = { -1, -1, -1, -1 };

    for (int i = 0; i < n; ++i) {
        int c = d[i].cls;
        if (c < 0 || c >= 4) continue;
        ++counts[c];
        ids[c] = i;
        if (c != 3) continue;

        int x = (int)((d[i].x1 + d[i].x2) * 0.5f + 0.5f);
        int y = (int)((d[i].y1 + d[i].y2) * 0.5f + 0.5f);
        xil_printf("GAUGE_TIP x=%d y=%d score_milli=%d\r\n",
                   x, y, (int)(d[i].score * 1000.0f + 0.5f));

        int sx = (int)(x * g_display_scale + 0.5f) + ox;
        int sy = (int)(y * g_display_scale + 0.5f) + oy;
        char label[48];
        snprintf(label, sizeof(label), "TIP (%d,%d)", x, y);
        gauge_mark(fb, fw, fh, sx, sy, label, 0, 255, 255);
    }

    /* The hub is always needed. The scale ends come either from the calibration
     * file (fixed angles, robust when the printed numbers are not detected) or
     * from the detected minimum/maximum marks. */
    float bx = 0, by = 0, radius = 0;
    bool  have_hub = (counts[0] == 1);

    float amin = 0, amax = 0;
    bool  have_arc = false;
    const char* arc_src = "none";

    if (have_hub) {
        const Detection& b = d[ids[0]];
        bx = (b.x1 + b.x2) * 0.5f;
        by = (b.y1 + b.y2) * 0.5f;
    }

    if (g_gauge_scale.have_arc) {
        amin     = g_gauge_scale.amin_deg * GAUGE_RAD_PER_DEG;
        amax     = g_gauge_scale.amax_deg * GAUGE_RAD_PER_DEG;
        have_arc = true;
        arc_src  = "cfg";
    } else if (have_hub && counts[1] == 1 && counts[2] == 1) {
        const Detection& lo = d[ids[2]];
        const Detection& hi = d[ids[1]];
        float lx = (lo.x1 + lo.x2) * 0.5f, ly = (lo.y1 + lo.y2) * 0.5f;
        float hx = (hi.x1 + hi.x2) * 0.5f, hy = (hi.y1 + hi.y2) * 0.5f;

        amin = atan2f(ly - by, lx - bx);
        amax = atan2f(hy - by, hx - bx);
        have_arc = true;
        arc_src  = "det";
    }

    /* Radius of the scale: from the marks when they exist, otherwise a fraction
     * of the frame (a gauge normally fills most of it). */
    if (have_hub) {
        if (counts[1] == 1) {
            const Detection& hi = d[ids[1]];
            float hx = (hi.x1 + hi.x2) * 0.5f, hy = (hi.y1 + hi.y2) * 0.5f;
            float r = sqrtf((hx - bx) * (hx - bx) + (hy - by) * (hy - by));
            if (r > radius) radius = r;
        }
        if (counts[2] == 1) {
            const Detection& lo = d[ids[2]];
            float lx = (lo.x1 + lo.x2) * 0.5f, ly = (lo.y1 + lo.y2) * 0.5f;
            float r = sqrtf((lx - bx) * (lx - bx) + (ly - by) * (ly - by));
            if (r > radius) radius = r;
        }
        if (radius <= 8.0f) {
            float half = (iw < ih ? iw : ih) * 0.42f;
            radius = half;
        }
    }

    float reading = 0.0f;
    bool  valid   = false;
    const char* source = "none";
    int   angle_cdeg = 0;

    /* The line finder runs on every frame, not only when the tip box is weak.
     * A black line through the hub is far more stable than a 20x13 box, and
     * drawing both lets a disagreement be seen on the HDMI output. */
    PointerCv cv;
    bool cv_ok = false;
    cv.angle = 0.0f;
    cv.score = 0.0f;
    cv.len   = 0.0f;

    if (have_hub && img && radius > 8.0f) {
        float half  = (counts[0] == 1)
                    ? (((d[ids[0]].x2 - d[ids[0]].x1) + (d[ids[0]].y2 - d[ids[0]].y1)) * 0.25f)
                    : 20.0f;
        float r_in  = half * 1.05f;
        float r_out = radius * 0.92f;

        if (r_in < 3.0f) r_in = 3.0f;
        cv_ok = pointer_cv_find(img, iw, ih, bx, by, r_in, r_out, &cv);

        if (cv_ok) {
            float ca = cosf(cv.angle), sa = sinf(cv.angle);
            float r_end = r_in + cv.len;
            int sx0 = (int)(bx * g_display_scale + 0.5f) + ox;
            int sy0 = (int)(by * g_display_scale + 0.5f) + oy;
            int sx1 = (int)((bx + ca * r_end) * g_display_scale + 0.5f) + ox;
            int sy1 = (int)((by + sa * r_end) * g_display_scale + 0.5f) + oy;
            gauge_line(fb, fw, fh, sx0, sy0, sx1, sy1, 255, 0, 255);
            gauge_mark(fb, fw, fh, sx1, sy1, "CV needle", 255, 0, 255);
        }
        xil_printf("GAUGE_NEEDLE angle_cdeg=%d score_milli=%d run_px=%d ok=%d\r\n",
                   (int)roundf(cv.angle * GAUGE_DEG_PER_RAD),
                   (int)(cv.score * 1000.0f + 0.5f), (int)cv.len, cv_ok ? 1 : 0);
    }

    if (have_hub && have_arc && cv_ok) {
        valid = gauge_read_arc(amin, amax, cv.angle, &reading);
        source = "cv";
        angle_cdeg = (int)roundf(cv.angle * GAUGE_DEG_PER_RAD);

        if (counts[3] == 1) {          /* logged as a cross-check, never used */
            const Detection& t = d[ids[3]];
            float atip = atan2f((t.y1 + t.y2) * 0.5f - by, (t.x1 + t.x2) * 0.5f - bx);
            int delta = (int)roundf((cv.angle - atip) * GAUGE_DEG_PER_RAD);
            while (delta > 180) delta -= 360;
            while (delta < -180) delta += 360;
            xil_printf("GAUGE_NEEDLE tip_delta_cdeg=%d tip_score_milli=%d\r\n",
                       delta, (int)(t.score * 1000.0f + 0.5f));
        }
    } else if (have_hub && have_arc && counts[3] == 1) {
        /* Line finder failed; keep the detector box as the only fallback. */
        const Detection& t = d[ids[3]];
        float tx = (t.x1 + t.x2) * 0.5f;
        float ty = (t.y1 + t.y2) * 0.5f;
        float atip = atan2f(ty - by, tx - bx);

        valid = gauge_read_arc(amin, amax, atip, &reading);
        angle_cdeg = (int)roundf(atip * GAUGE_DEG_PER_RAD);
        source = "yolo";
        xil_printf("GAUGE_TIP fallback score_milli=%d\r\n",
                   (int)(t.score * 1000.0f + 0.5f));
    } else if (!have_hub) {
        xil_printf("GAUGE_HUB missing\r\n");
    }

    char text[112];
    if (valid) {
        int value = (int)roundf(reading * 100.0f);
        int magnitude = value < 0 ? -value : value;
        snprintf(text, sizeof(text), "READING %s%d.%02d %s | %d deg | %s/%s",
                 value < 0 ? "-" : "", magnitude / 100, magnitude % 100,
                 g_gauge_scale.unit, angle_cdeg, source, arc_src);
        xil_printf("GAUGE_READING value_centi=%d angle_cdeg=%d source=%s arc=%s\r\n",
                   value, angle_cdeg, source, arc_src);
    } else {
        const char* why = !have_hub ? "hub missing"
                        : !have_arc ? "scale marks missing"
                        : "needle not found";
        snprintf(text, sizeof(text), "READING -- | %s", why);
        xil_printf("GAUGE_READING invalid reason=%s\r\n", why);
    }

    osd_fill(fb, fw, fh, 0, fh - 24, fw, 24, 0, 0, 0);
    osd_text(fb, fw, fh, 6, fh - 20, text, 1, 0, 255, 255);
}
