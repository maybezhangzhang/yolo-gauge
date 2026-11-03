#ifndef DRAW_BOX_H
#define DRAW_BOX_H
#include "sd_io.h"
#include "yolo_decode.h"

/*
 * 在 framebuffer (BGR, fbw x fbh) 上画一个检测框 + 类名/置信度标签。
 *
 * d     : Detection, 坐标是源图像素坐标 (yolo_decode 已经换算好)
 * label : 类别名 (ASCII, 例如 "container")
 * ox,oy : 源图左上角在屏幕上的偏移 (帧居中显示时 != 0)
 */
void draw_detection(u8* fb, int fbw, int fbh,
                    const Detection* d, const char* label,
                    int ox, int oy);

#endif
