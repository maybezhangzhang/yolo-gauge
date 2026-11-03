#include "pointer_cv.h"
#include <math.h>

static inline int pcv_lum3(const unsigned char* p)
{
    return (int)((19595u * p[2] + 38470u * p[1] + 7471u * p[0] + 32768u) >> 16);
}

static float pcv_ray_mean(const unsigned char* img, int w, int h,
                          float cx, float cy, float ca, float sa,
                          float r_in, float r_out)
{
    float step = (r_out - r_in) / (float)PCV_SAMPLES;
    float sum  = 0.0f;
    int   n    = 0;

    for (int i = 0; i < PCV_SAMPLES; ++i) {
        float r = r_in + step * ((float)i + 0.5f);
        int   x = (int)(cx + ca * r + 0.5f);
        int   y = (int)(cy + sa * r + 0.5f);
        if (x < 0 || y < 0 || x >= w || y >= h) continue;
        sum += (float)pcv_lum3(img + ((long)y * w + x) * 3);
        ++n;
    }
    return n ? sum / (float)n : 255.0f;
}

/* Length of the dark run that starts at r0, in pixels along the ray.
 * r0 must be outside the hub cap: the center boss is bright metal, so a run
 * started right at the hub rim would stop after a few bright samples and the
 * detector would reject a perfectly good needle. */
static float pcv_dark_run(const unsigned char* img, int w, int h,
                          float cx, float cy, float ca, float sa,
                          float r0, float r_out, float level)
{
    float r    = r0;
    float last = r0;
    int   miss = 0;

    while (r < r_out) {
        int x = (int)(cx + ca * r + 0.5f);
        int y = (int)(cy + sa * r + 0.5f);
        if (x < 0 || y < 0 || x >= w || y >= h) break;
        if ((float)pcv_lum3(img + ((long)y * w + x) * 3) < level) {
            last = r;
            miss = 0;
        } else if (++miss > 6) {
            break;
        }
        r += 1.0f;
    }
    return last - r0;
}

int pointer_cv_find(const unsigned char* img, int w, int h,
                    float cx, float cy, float r_in, float r_out,
                    PointerCv* out)
{
    static float means[PCV_STEPS];
    float face, best_v, score;
    int   best_i;

    out->ok   = 0;
    out->len  = 0.0f;
    out->score = 0.0f;
    out->angle = 0.0f;

    if (!img) return 0;
    if (cx < 0.0f || cy < 0.0f || cx >= (float)w || cy >= (float)h) return 0;
    if (r_in < 2.0f || r_out <= r_in + 6.0f) return 0;

    best_v = 1.0e9f;
    best_i = 0;
    for (int i = 0; i < PCV_STEPS; ++i) {
        float a = (float)i * (6.28318530718f / (float)PCV_STEPS);
        float m = pcv_ray_mean(img, w, h, cx, cy, cosf(a), sinf(a), r_in, r_out);
        means[i] = m;
        if (m < best_v) { best_v = m; best_i = i; }
    }

    /* Dial face level: average brightness of the brightest quartile of rays. */
    {
        float hi = -1.0e9f;
        float acc = 0.0f;
        int   cnt = 0;
        for (int i = 0; i < PCV_STEPS; ++i) if (means[i] > hi) hi = means[i];
        for (int i = 0; i < PCV_STEPS; ++i) {
            if (means[i] >= hi * 0.85f) { acc += means[i]; ++cnt; }
        }
        face = cnt ? acc / (float)cnt : hi;
    }
    if (face < 30.0f) return 0;              /* too dark to judge contrast */

    score  = (face - best_v) / face;

    /* Sub-degree refinement around the winning ray (0.05 degree steps). */
    {
        float base = (float)best_i * (6.28318530718f / (float)PCV_STEPS);
        float bs   = score;
        for (int k = -20; k <= 20; ++k) {
            if (k == 0) continue;
            float a = base + (float)k * 0.0008726646f;
            float m = pcv_ray_mean(img, w, h, cx, cy, cosf(a), sinf(a), r_in, r_out);
            float s = (face - m) / face;
            if (s > bs) { bs = s; base = a; }
        }
        out->angle = base;
        score      = bs;
    }

    out->score = score;
    {
        /* Skip the bright hub cap before measuring how far the needle reaches. */
        float r0 = r_in + 0.12f * (r_out - r_in);
        out->len = pcv_dark_run(img, w, h, cx, cy, cosf(out->angle), sinf(out->angle),
                                r0, r_out, face * 0.70f);
        out->ok  = (score > 0.12f && out->len > (r_out - r_in) * 0.25f) ? 1 : 0;
    }
    return out->ok;
}
