#ifndef POINTER_CV_H
#define POINTER_CV_H

/* Classic-CV needle finder, used when the detector is not confident about the
 * tip. The needle is the only long, dark, straight feature that starts at the
 * hub, so every ray leaving the hub is scored by how much darker it is than the
 * dial face and the darkest one wins.
 *
 * Image is 24bpp BGR with rows packed (3*w bytes per row), origin top-left,
 * +x right, +y down, angle = atan2(dy, dx) radians. */

#define PCV_STEPS 360     /* coarse angular resolution: 1 degree */
#define PCV_SAMPLES 48    /* samples taken along one ray */

typedef struct {
    float angle;   /* radians, image coordinates */
    float score;   /* 0..1, 1 = fully black ray over a white face */
    float len;     /* radius where the dark run ends (needle length) */
    int   ok;      /* 1 when the ray is dark enough to be a needle */
} PointerCv;

int pointer_cv_find(const unsigned char* img, int w, int h,
                    float cx, float cy, float r_in, float r_out,
                    PointerCv* out);

#endif /* POINTER_CV_H */
