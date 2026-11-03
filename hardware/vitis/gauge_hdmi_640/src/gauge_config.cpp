/*
 * gauge_config.cpp
 */
#include "gauge_config.h"
#include "ff.h"
#include "xil_printf.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Default for the bench gauge: 0..16 MPa.  gauge_scale.txt on the SD card
   overrides it, and the arc endpoints still come from the detected marks
   unless the file also gives angle_min/angle_max. */
GaugeScale g_gauge_scale = {
    0.0f, 16.0f, "MPa", 1, 0,
    0, 0.0f, 0.0f
};

void gauge_scale_default(void)
{
    g_gauge_scale.vmin      = 0.0f;
    g_gauge_scale.vmax      = 16.0f;
    g_gauge_scale.clockwise = 1;
    g_gauge_scale.from_sd   = 0;
    g_gauge_scale.have_arc  = 0;
    g_gauge_scale.amin_deg  = 0.0f;
    g_gauge_scale.amax_deg  = 0.0f;
    strcpy(g_gauge_scale.unit, "MPa");
}

void gauge_scale_set(float vmin, float vmax, const char* unit, int clockwise)
{
    g_gauge_scale.vmin      = vmin;
    g_gauge_scale.vmax      = vmax;
    g_gauge_scale.clockwise = clockwise ? 1 : 0;
    if (unit && unit[0]) {
        strncpy(g_gauge_scale.unit, unit, sizeof(g_gauge_scale.unit) - 1);
        g_gauge_scale.unit[sizeof(g_gauge_scale.unit) - 1] = 0;
    }
}

void gauge_scale_set_arc(float amin_deg, float amax_deg)
{
    g_gauge_scale.amin_deg = amin_deg;
    g_gauge_scale.amax_deg = amax_deg;
    g_gauge_scale.have_arc = 1;
}

static int read_file(const char* path, char* buf, int cap)
{
    FIL  fil;
    UINT br = 0;
    int  rc;

    rc = f_open(&fil, path, FA_READ);
    if (rc != FR_OK) return -1;

    if (f_read(&fil, buf, (UINT)(cap - 1), &br) != FR_OK) {
        f_close(&fil);
        return -2;
    }
    f_close(&fil);
    buf[(int)br] = 0;
    return (int)br;
}

static void trim(char* s)
{
    char* p = s;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (p != s) memmove(s, p, strlen(p) + 1);

    int n = (int)strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

int gauge_scale_load(const char* path)
{
    static char buf[768];
    int  len;
    int  applied = 0;
    int  have_min_angle = 0;
    int  have_max_angle = 0;
    char* line;
    char* next;

    if (!path || !path[0]) return 0;

    len = read_file(path, buf, (int)sizeof(buf));
    if (len <= 0) {
        xil_printf("GAUGE_SCALE: %s not found, using default %.2f..%.2f %s\r\n",
                   path, g_gauge_scale.vmin, g_gauge_scale.vmax, g_gauge_scale.unit);
        return 0;
    }

    line = buf;
    while (line && *line) {
        char* eq;
        next = strchr(line, '\n');
        if (next) *next++ = 0;

        eq = strchr(line, '=');
        if (eq && *line != '#') {
            char* key;
            char* val;

            *eq = 0;
            key = line;
            val = eq + 1;
            trim(key);
            trim(val);

            for (char* k = key; *k; ++k) *k = (char)tolower((unsigned char)*k);

            if (strcmp(key, "min") == 0 || strcmp(key, "value_min") == 0) {
                g_gauge_scale.vmin = (float)atof(val);
                applied = 1;
            } else if (strcmp(key, "max") == 0 || strcmp(key, "value_max") == 0) {
                g_gauge_scale.vmax = (float)atof(val);
                applied = 1;
            } else if (strcmp(key, "unit") == 0) {
                gauge_scale_set(g_gauge_scale.vmin, g_gauge_scale.vmax, val,
                                g_gauge_scale.clockwise);
                applied = 1;
            } else if (strcmp(key, "clockwise") == 0) {
                g_gauge_scale.clockwise = (atoi(val) != 0) ? 1 : 0;
                applied = 1;
            } else if (strcmp(key, "angle_min") == 0) {
                g_gauge_scale.amin_deg = (float)atof(val);
                have_min_angle = 1;
                applied = 1;
            } else if (strcmp(key, "angle_max") == 0) {
                g_gauge_scale.amax_deg = (float)atof(val);
                have_max_angle = 1;
                applied = 1;
            }
        }
        line = next;
    }

    if (have_min_angle && have_max_angle) g_gauge_scale.have_arc = 1;

    if (applied) {
        g_gauge_scale.from_sd = 1;
        xil_printf("GAUGE_SCALE: %s loaded, %.3f..%.3f %s, clockwise=%d, arc=%d %.1f/%.1f deg\r\n",
                   path, g_gauge_scale.vmin, g_gauge_scale.vmax, g_gauge_scale.unit,
                   g_gauge_scale.clockwise, g_gauge_scale.have_arc,
                   g_gauge_scale.amin_deg, g_gauge_scale.amax_deg);
    }
    return applied;
}
