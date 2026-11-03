#ifndef GAUGE_READING_H
#define GAUGE_READING_H
#include <math.h>

/* Dial calibration lives in gauge_config.h: the numeric range, the unit and the
 * sweep direction are runtime values, either the compiled-in default or the
 * contents of "gauge_scale.txt" on the SD card. The reading is linear in the
 * angle between the printed minimum and maximum marks, measured in image
 * coordinates (+x = 0, +y = 90 deg, so clockwise = increasing angle). */
#include "gauge_config.h"

/* Below this tip confidence the detector is not trusted and the needle is
 * located with the classic-CV ray search instead (pointer_cv.h). */
#define GAUGE_TIP_CV_THRESHOLD 0.60f

static float gauge_wrap(float angle) {
    const float tau = 6.28318530718f;
    angle = fmodf(angle, tau);
    return angle < 0 ? angle + tau : angle;
}

/* Angle between the tip and the minimum mark, expressed on the scale that runs
 * from the minimum mark to the maximum mark in the configured direction. */
static bool gauge_from_angle(float amin, float amax, float atip,
                             float* position, float* span) {
    const float tau = 6.28318530718f;
    const float tolerance = 0.0872664626f; /* 5 degrees at the endpoints */
    const float direction = g_gauge_scale.clockwise ? 1.0f : -1.0f;

    *span     = gauge_wrap(direction * (amax - amin));
    *position = gauge_wrap(direction * (atip - amin));

    if (*span < 0.174532925f || *span > tau - 0.174532925f) return false;
    if (*position > *span) {
        if (tau - *position <= tolerance)      *position = 0.0f;
        else if (*position - *span <= tolerance) *position = *span;
        else return false;
    }
    return true;
}

static float gauge_scale(float position, float span) {
    const float lo = g_gauge_scale.vmin;
    const float hi = g_gauge_scale.vmax;

    if (position >= span) return hi;
    if (span <= 0.0f) return lo;
    return lo + (hi - lo) * position / span;
}

/* Reading from the detector's tip box centre. (lx,ly) is the minimum mark,
 * (hx,hy) the maximum mark, (tx,ty) the tip centre. */
static bool gauge_read(float bx, float by, float lx, float ly, float hx, float hy,
                       float tx, float ty, float* value) {
    float dx[3] = { lx - bx, hx - bx, tx - bx };
    float dy[3] = { ly - by, hy - by, ty - by };
    float a[3];

    for (int i = 0; i < 3; ++i) {
        if (!isfinite(dx[i]) || !isfinite(dy[i]) || dx[i] * dx[i] + dy[i] * dy[i] <= 1.0f)
            return false;
        a[i] = atan2f(dy[i], dx[i]);
    }

    float position, span;
    if (!gauge_from_angle(a[0], a[1], a[2], &position, &span)) return false;
    *value = gauge_scale(position, span);
    return isfinite(*value);
}

/* Reading from an explicit pair of scale angles (radians). Used when the arc is
 * pinned by the calibration file, so the printed numbers never have to be
 * detected at run time. */
static bool gauge_read_arc(float amin, float amax, float atip, float* value) {
    float position, span;

    if (!isfinite(amin) || !isfinite(amax) || !isfinite(atip)) return false;
    if (!gauge_from_angle(amin, amax, atip, &position, &span)) return false;
    *value = gauge_scale(position, span);
    return isfinite(*value);
}
/* Reading from an angle produced by the CV needle finder; the hub position is
 * only needed to turn the two landmark points into angles. */
static bool gauge_read_angle(float bx, float by, float lx, float ly, float hx, float hy,
                             float tip_angle, float* value) {
    float amin = atan2f(ly - by, lx - bx);
    float amax = atan2f(hy - by, hx - bx);

    return gauge_read_arc(amin, amax, tip_angle, value);
}
#endif



