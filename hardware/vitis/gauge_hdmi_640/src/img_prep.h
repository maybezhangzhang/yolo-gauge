#ifndef IMG_PREP_H
#define IMG_PREP_H
#include "sd_io.h"
#include "letterbox.h"

/*
 * 把 BGR 帧 (src_w x src_h, 行距 src_w*3) 预处理成 416x416 CHW 的 data_t 数组。
 *
 * 处理顺序: letterbox 等比缩放(最近邻) + 灰边(114,114,114)
 *           -> BGR 变 RGB
 *           -> /255 归一化 -> *SCALE(512) -> 四舍五入成 short
 *
 * out_416 的排布和原来完全一致:
 *   out[0*416*416 + dy*416 + dx] = R
 *   out[1*416*416 + dy*416 + dx] = G
 *   out[2*416*416 + dy*416 + dx] = B
 * 这与 PC 端 tools/predictor/predict.py 的排布一致, 不要随意改。
 */
void prep_image(const u8* bgr_frame, int src_w, int src_h,
                const LetterBox* lb, data_t* out_416);

#endif
