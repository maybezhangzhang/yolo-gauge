/*
 * gauge_config.h
 *
 * Runtime dial calibration. The printed numbers on a dial are not read by the
 * network, so the numeric range has to come from somewhere: either the
 * compiled-in default below, or a small text file on the SD card
 * ("gauge_scale.txt") that describes the dial currently being measured.
 *
 * File format (one key per line, '#' starts a comment):
 *
 *     min   = 0            value printed at the minimum mark
 *     max   = 16           value printed at the maximum mark
 *     unit  = kgf/cm2      label shown on the banner
 *     clockwise = 1        1 = value grows clockwise on screen
 *     angle_min = 137.4    direction of the minimum mark, degrees
 *     angle_max = 43.7     direction of the maximum mark, degrees
 *
 * The two angle keys are optional. When they are present the reading no longer
 * depends on the detector finding the printed numbers, which is the usual
 * failure mode on dense dials; the needle angle alone is enough. When they are
 * absent the marks returned by the detector are used instead.
 *
 * Angles are image coordinates: 0 deg = +x (right), 90 deg = +y (down), so the
 * value grows clockwise on screen for a normal dial.
 */
#ifndef GAUGE_CONFIG_H
#define GAUGE_CONFIG_H

struct GaugeScale {
    float vmin;        /* value at the minimum mark                     */
    float vmax;        /* value at the maximum mark                     */
    char  unit[12];    /* unit string shown next to the reading         */
    int   clockwise;   /* 1: reading grows clockwise in image coords    */
    int   from_sd;     /* 1 when the values came from gauge_scale.txt   */

    int   have_arc;    /* 1: use the two fixed angles below             */
    float amin_deg;    /* direction of the minimum mark, degrees        */
    float amax_deg;    /* direction of the maximum mark, degrees        */
};

extern GaugeScale g_gauge_scale;

/* Restore the compiled-in default for the dial this board is set up for
 * ("made in Tomsk", 0..16 kgf/cm2, marks at 137.4 and 43.7 deg). */
void gauge_scale_default(void);

/* Override part of the configuration programmatically. */
void gauge_scale_set(float vmin, float vmax, const char* unit, int clockwise);
void gauge_scale_set_arc(float amin_deg, float amax_deg);

/* Try to load a calibration file from the SD card. Returns 1 when at least one
 * key was applied, 0 when the file is missing or unreadable (defaults stay). */
int gauge_scale_load(const char* path);

#endif
