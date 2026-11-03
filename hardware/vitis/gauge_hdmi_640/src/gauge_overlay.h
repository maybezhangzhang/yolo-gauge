#ifndef GAUGE_OVERLAY_H
#define GAUGE_OVERLAY_H

#include "yolo_decode.h"

/* Draws the per-class tip marker, the needle line and the reading banner.
 *
 * The classic-CV ray search runs on every frame: a black line through the hub is
 * more stable than the 20x13 tip box, so the reading comes from the line (drawn
 * in magenta) and the box is only a cross-check whose angle delta is logged in
 * the GAUGE_NEEDLE line.  If the ray search finds nothing the box is used
 * instead (source=yolo).  Everything is also reported over UART.
 *
 * (fx,fy) is the source-image size, (ox,oy) the offset used to place it on the
 * framebuffer; detections are already in source coordinates. */
void gauge_overlay(u8* fb, int fw, int fh, const Detection* d, int n,
                   const u8* img, int iw, int ih, int ox, int oy);

#endif /* GAUGE_OVERLAY_H */
