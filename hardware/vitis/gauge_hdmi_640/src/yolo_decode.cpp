#include "yolo_decode.h"
#include <cmath>

/*
 * yolo_decode.cpp  --  网络输出 -> 检测框 (含 letterbox 反变换)
 *
 * 相对旧版本的关键修改:
 *   旧版把归一化坐标直接乘 1024 / 600, 等价于假设"网络看到的 416x416
 *   就是整幅 1024x600 拉压后的结果"。改成 letterbox 之后网络输入里有
 *   灰边, 必须先把坐标从网络坐标映射回源图坐标 (去掉灰边、除以缩放
 *   比例), 否则所有框会整体缩小并向中间偏。映射统一由 letterbox_n2s()
 *   完成, 保证与 img_prep.cpp 的预处理严格互逆。
 */

// Anchors and masks come from the exported checkpoint.

#define INPUT 640.0f    // 网络输入边长, 必须与 NET_SIZE 一致

// letterbox 参数 (由 yolo_set_letterbox 传入)
static LetterBox g_lb;
static int       g_lb_valid = 0;

void yolo_set_letterbox(const LetterBox* lb) {
    if (!lb) { g_lb_valid = 0; return; }
    g_lb = *lb;
    g_lb_valid = 1;
}

static inline float sigmoidf(float x) {
    if (x > 40.0f) return 1.0f;
    if (x < -40.0f) return 0.0f;
    return 1.0f / (1.0f + expf(-x));
}

// 解码单个特征图
static int decode_head(const data_t* feat, int H, int W, const int* mask,
                       Detection* tmp, int tmp_cap) {
    int n = 0;
    float stride = INPUT / (float)H;

    for (int a = 0; a < 3; a++) {
        float aw = ANCHORS[mask[a]][0] / stride;
        float ah = ANCHORS[mask[a]][1] / stride;
        for (int row = 0; row < H; row++) {
            for (int col = 0; col < W; col++) {
                int base = (a * (5 + NUM_CLASSES)) * H * W + row * W + col;
                float vx = feat[base + 0 * H * W] / (float)SCALE;
                float vy = feat[base + 1 * H * W] / (float)SCALE;
                float vw = feat[base + 2 * H * W] / (float)SCALE;
                float vh = feat[base + 3 * H * W] / (float)SCALE;
                float conf = sigmoidf(feat[base + 4 * H * W] / (float)SCALE);

                float best_p = -1.0f;
                int best_c = 0;
                for (int c = 0; c < NUM_CLASSES; c++) {
                    float p = sigmoidf(feat[base + (5 + c) * H * W] / (float)SCALE);
                    if (p > best_p) { best_p = p; best_c = c; }
                }
                float score = conf * best_p;
                if (score < CONF_THRES) continue;
                if (n >= tmp_cap) return n;

                float x = sigmoidf(vx) + col;
                float y = sigmoidf(vy) + row;
                float w = expf(fmaxf(-20.0f, fminf(20.0f, vw))) * aw;
                float h = expf(fmaxf(-20.0f, fminf(20.0f, vh))) * ah;
                // 归一化到 [0,1] 后乘 416 => 网络像素坐标
                float cx = x / (float)W * INPUT;
                float cy = y / (float)H * INPUT;
                float bw = w / (float)W * INPUT;
                float bh = h / (float)H * INPUT;

                float nx1 = cx - bw * 0.5f;
                float ny1 = cy - bh * 0.5f;
                float nx2 = cx + bw * 0.5f;
                float ny2 = cy + bh * 0.5f;

                if (g_lb_valid) {
                    // 网络坐标 -> 源图坐标 (去灰边 + 反缩放)
                    letterbox_n2s(&g_lb, nx1, ny1, &tmp[n].x1, &tmp[n].y1);
                    letterbox_n2s(&g_lb, nx2, ny2, &tmp[n].x2, &tmp[n].y2);
                } else {
                    // 没有设置 letterbox 时退化成旧行为 (整幅拉伸)
                    tmp[n].x1 = nx1 / INPUT * 1024.0f;
                    tmp[n].y1 = ny1 / INPUT * 600.0f;
                    tmp[n].x2 = nx2 / INPUT * 1024.0f;
                    tmp[n].y2 = ny2 / INPUT * 600.0f;
                }
                if (tmp[n].x2 < tmp[n].x1) { float t = tmp[n].x1; tmp[n].x1 = tmp[n].x2; tmp[n].x2 = t; }
                if (tmp[n].y2 < tmp[n].y1) { float t = tmp[n].y1; tmp[n].y1 = tmp[n].y2; tmp[n].y2 = t; }

                tmp[n].score = score;
                tmp[n].cls = best_c;
                n++;
            }
        }
    }
    return n;
}

static float iou(const Detection& a, const Detection& b) {
    float ix1 = a.x1 > b.x1 ? a.x1 : b.x1;
    float iy1 = a.y1 > b.y1 ? a.y1 : b.y1;
    float ix2 = a.x2 < b.x2 ? a.x2 : b.x2;
    float iy2 = a.y2 < b.y2 ? a.y2 : b.y2;
    float iw = ix2 - ix1; if (iw < 0) iw = 0;
    float ih = iy2 - iy1; if (ih < 0) ih = 0;
    float inter = iw * ih;
    float ua = (a.x2 - a.x1) * (a.y2 - a.y1) +
               (b.x2 - b.x1) * (b.y2 - b.y1) - inter;
    return ua > 0 ? inter / ua : 0.0f;
}

int yolo_decode(const data_t* out0, const data_t* out1, Detection* dets, int max_dets) {
    static Detection tmp[3 * 20 * 20 + 3 * 40 * 40];
    int cap = (int)(sizeof(tmp) / sizeof(tmp[0]));

    int n = decode_head(out0, 20, 20, MASK0, tmp, cap);
    n += decode_head(out1, 40, 40, MASK1, tmp + n, cap - n);

    // 按 score 降序排序 (候选框数量不多, 冒泡足够)
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (tmp[j].score > tmp[i].score) {
                Detection t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t;
            }
        }
    }

    // 同类 NMS
    int m = 0;
    for (int i = 0; i < n && m < max_dets; i++) {
        int keep = 1;
        for (int j = 0; j < m; j++) {
            if (dets[j].cls == tmp[i].cls && iou(dets[j], tmp[i]) >= NMS_THRES) {
                keep = 0; break;
            }
        }
        if (keep) dets[m++] = tmp[i];
    }
    return m;
}
