/*
 * main.cpp -- gauge meter detection on Zynq-7020 (XC7Z020)
 *
 * Data path:
 *   SD card BMP (test1.bmp ... testN.bmp)  --\
 *   or OpenMV camera frame over UART          >-- letterbox 640x640
 *                                                  -> YOLOv4-tiny on the conv accelerator
 *                                                  -> decode + NMS
 *                                                  -> boxes / labels on a 1024x600 HDMI frame buffer
 *
 * If at least one testN.bmp exists on the SD card the firmware runs in static
 * image mode (no camera, no UART handshake) and loops over the images.
 * Otherwise it falls back to the OpenMV camera link.
 *
 * DDR map (see board/README.md):
 *   0x20000000  frame buffer      1024*600*3
 *   0x21000000  source image buf  4 MB
 *   0x21500000  camera frame buf  for the OpenMV path
 *   .bss        feature maps      ~30 MB (static buffers in yolo4_tiny.h / basic_block.h)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "xil_printf.h"
#include "xil_cache.h"
#include "xil_io.h"
#include "xil_exception.h"
#include "sleep.h"
#include "xtime_l.h"
#include "xparameters.h"
#include "xaxivdma.h"
#include "display_ctrl_hdmi/display_ctrl_hdmi.h"
#include "vdma_api/vdma_api.h"
#include "ff.h"

#include "sd_io.h"
#include "basic_op.h"
#include "yolo4_tiny.h"
#include "img_prep.h"
#include "yolo_decode.h"
#include "draw_box.h"
#include "osd.h"
#include "letterbox.h"
#include "cam_link.h"
#include "gauge_config.h"
#include "gauge_overlay.h"
#include "img_convert.h"

#define VDMA_ID      XPAR_AXIVDMA_0_DEVICE_ID
#define DISP_VTC_ID  XPAR_VTC_0_DEVICE_ID

#define DDR_BASE     (XPAR_PS7_DDR_0_S_AXI_BASEADDR)
#define FB_ADDR      (DDR_BASE + 0x20000000u)
#define SRC_ADDR     (DDR_BASE + 0x21000000u)

#define CAM_BUF_ADDR (DDR_BASE + 0x21500000u)
#define CAM_BUF_BYTES ((u32)CAM_LINK_MAX_W * (u32)CAM_LINK_MAX_H * 2u)

#define CAM_AUTO_DETECT 1

#define DISP_W       1024
#define DISP_H       600
#define FB_BYTES     ((u32)DISP_W * DISP_H * 3)

#define SRC_BUF_BYTES (4u * 1024u * 1024u)
#define BMP_ROW_TMP   8192u
#define MAX_IMGS      999
#define IMG_FMT       "test%d.bmp"

#define ENABLE_VSYNC_WAIT 1

#define PREP_LETTERBOX 1

#define VDMA_FRMCNT_REG 0x28u

static XAxiVdma    vdma;
static DisplayCtrl dispCtrl;
static u8*         fb = (u8*)FB_ADDR;

static u8* const   src_buf = (u8*)SRC_ADDR;
static u8          bmp_row_tmp[BMP_ROW_TMP];

static data_t  in_img[3 * 640 * 640];
static data_t  out0[OUT_CH * 20 * 20];
static data_t  out1[OUT_CH * 40 * 40];
static Detection dets[128];

static Yolo4_Tiny* g_yolo = 0;
static LetterBox   g_lb;

static u8* const   cam_rx = (u8*)CAM_BUF_ADDR;
static int         cam_w = 0, cam_h = 0;

float g_display_scale = 1.0f;

static const char* CLASS_NAMES[NUM_CLASSES] = {"base", "maximum", "minimum", "tip"};

/* ------------------------------------------------------------------------
 * CPU fault diagnostics.
 *
 * A data/prefetch abort on bare metal leaves no trace by default, so the three
 * handlers below latch the fault status (DFAR/DFSR/IFAR/IFSR) and the return
 * address into g_fault[] and print one line on UART1.  g_fault stays readable
 * over JTAG even when no serial console is attached.
 * --------------------------------------------------------------------- */
volatile u32 g_fault[6];

static void fault_report(const char* tag)
{
    u32 far, fsr, lr;
    __asm volatile ("mov %0, lr" : "=r"(lr));
    __asm volatile ("mrc p15, 0, %0, c6, c0, 0" : "=r"(far));   /* DFAR */
    __asm volatile ("mrc p15, 0, %0, c5, c0, 0" : "=r"(fsr));   /* DFSR */

    g_fault[0] = 0xA5A5A5A5u;
    g_fault[1] = far;
    g_fault[2] = fsr;
    g_fault[3] = lr;
    xil_printf("[FAULT %s] FAR=0x%08x FSR=0x%08x LR=0x%08x\r\n",
               tag, far, fsr, lr);
    for (;;) {
        /* stay here so the debugger sees a stable PC */
    }
}

static void data_abort_handler(void)     { fault_report("data"); }
static void prefetch_abort_handler(void) { fault_report("prefetch"); }
static void undefined_handler(void)      { fault_report("undef"); }

static u32 now_us(void) {
#if (COUNTS_PER_SECOND / 1000000) > 0
    XTime t;
    XTime_GetTime(&t);
    return (u32)(t / (u64)(COUNTS_PER_SECOND / 1000000));
#else
    XTime t;
    XTime_GetTime(&t);
    return (u32)((t * 1000000ULL) / (u64)COUNTS_PER_SECOND);
#endif
}

static inline u32 rd32(const u8* p) {
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}
static inline u32 rd16(const u8* p) {
    return (u32)p[0] | ((u32)p[1] << 8);
}

static int bmp_load(const char* name, u8* dst, u32 dst_cap, int* pw, int* ph) {
    FIL     fil;
    UINT    br = 0;
    FRESULT fr;
    u8      head[54];

    fr = f_open(&fil, name, FA_READ);
    if (fr != FR_OK) return -1;
    fr = f_read(&fil, head, 54, &br);
    if (fr != FR_OK || br != 54) { f_close(&fil); return -2; }
    if (head[0] != 'B' || head[1] != 'M') { f_close(&fil); return -3; }

    u32   off  = rd32(head + 0x0A);
    int   w    = (int)rd32(head + 0x12);
    int   h    = (int)rd32(head + 0x16);
    u32   bpp  = rd16(head + 0x1C);
    u32   comp = rd32(head + 0x1E);

    if (bpp != 24u || comp != 0u) { f_close(&fil); return -4; }

    int topdown = 0;
    if (h < 0) { h = -h; topdown = 1; }
    if (w <= 0 || h <= 0) { f_close(&fil); return -5; }

    u32 rowbytes = (u32)w * 3u;
    u32 stride   = (rowbytes + 3u) & ~3u;
    u32 datasize = stride * (u32)h;

    if (stride > BMP_ROW_TMP || datasize > dst_cap) { f_close(&fil); return -6; }

    fr = f_lseek(&fil, off);
    if (fr != FR_OK) { f_close(&fil); return -7; }
    fr = f_read(&fil, dst, datasize, &br);
    f_close(&fil);
    if (fr != FR_OK || br != datasize) return -8;

    if (!topdown) {
        for (int i = 0; i < h / 2; i++) {
            u8* a = dst + (u32)i * stride;
            u8* b = dst + (u32)(h - 1 - i) * stride;
            memcpy(bmp_row_tmp, a, stride);
            memcpy(a, b, stride);
            memcpy(b, bmp_row_tmp, stride);
        }
    }
    if (stride != rowbytes) {
        for (int i = 0; i < h; i++)
            memmove(dst + (u32)i * rowbytes, dst + (u32)i * stride, rowbytes);
    }

    *pw = w;
    *ph = h;
    return 0;
}

static int frame_exists(int idx) {
    FIL  f;
    char n[16];
    snprintf(n, sizeof(n), IMG_FMT, idx);
    if (f_open(&f, n, FA_READ) != FR_OK) return 0;
    f_close(&f);
    return 1;
}

static int find_img_count(void) {
    int n = 1;
    while (n <= MAX_IMGS && frame_exists(n)) n++;
    return n - 1;
}

static void fb_clear_black(void) {
    memset(fb, 0, FB_BYTES);
    Xil_DCacheFlushRange((u32)fb, FB_BYTES);
}

static void fb_show(const u8* src, int w, int h, int* pox, int* poy) {
    if (w <= 0 || h <= 0) { *pox = 0; *poy = 0; g_display_scale = 1.0f; return; }
    float sx = (float)DISP_W / (float)w;
    float sy = (float)DISP_H / (float)h;
    g_display_scale = sx < sy ? sx : sy;
    int dw = (int)((float)w * g_display_scale + 0.5f);
    int dh = (int)((float)h * g_display_scale + 0.5f);
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    int ox = (DISP_W - dw) / 2;
    int oy = (DISP_H - dh) / 2;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;
    for (int y = 0; y < dh; y++) {
        int syi = (int)((long)y * h / dh);
        int dy = y + oy;
        if (dy >= DISP_H) break;
        for (int x = 0; x < dw; x++) {
            int sxi = (int)((long)x * w / dw);
            const u8* sp = src + ((long)syi * w + sxi) * 3;
            u8* dp = fb + ((long)dy * DISP_W + ox + x) * 3;
            dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2];
        }
    }
    *pox = ox;
    *poy = oy;
}

#if ENABLE_VSYNC_WAIT

static int vsync_ok = 1;

static void wait_frame_start(void) {
    if (!vsync_ok) return;
    u32 t0 = now_us();
    u32 c0 = Xil_In32(XPAR_AXIVDMA_0_BASEADDR + VDMA_FRMCNT_REG);
    for (;;) {
        if (Xil_In32(XPAR_AXIVDMA_0_BASEADDR + VDMA_FRMCNT_REG) != c0) return;
        if ((u32)(now_us() - t0) > 30000u) {
            vsync_ok = 0;
            xil_printf("vsync counter idle -> tearing guard disabled\r\n");
            return;
        }
    }
}
#else
static void wait_frame_start(void) { }
#endif

static void hud_draw_3lines(const char* l1, const char* l2, const char* l3,
                            int r2, int g2, int b2, int r3, int g3, int b3) {
    int w1 = osd_text_w(l1, 2);
    int w2 = osd_text_w(l2, 1);
    int w3 = osd_text_w(l3, 1);
    int bw = w1;
    if (w2 > bw) bw = w2;
    if (w3 > bw) bw = w3;
    bw += 12;

    osd_fill(fb, DISP_W, DISP_H, 0, 0, bw, 78, 0, 0, 0);
    osd_fill(fb, DISP_W, DISP_H, 0, 77, bw, 2, 0, 160, 0);
    osd_text(fb, DISP_W, DISP_H, 6,  4, l1, 2, 255, 255, 255);
    osd_text(fb, DISP_W, DISP_H, 6, 40, l2, 1, r2, g2, b2);
    osd_text(fb, DISP_W, DISP_H, 6, 58, l3, 1, r3, g3, b3);
}

static void draw_ready_hud(int nimgs) {
    char l1[64], l2[64], l3[64];
    snprintf(l1, sizeof(l1), "YOLOv4-tiny 640x640");
    if (nimgs > 0) {
        snprintf(l2, sizeof(l2), "READY: %d image(s) found", nimgs);
        snprintf(l3, sizeof(l3), "waiting for OpenMV camera frame");
        hud_draw_3lines(l1, l2, l3, 160, 255, 160, 200, 220, 255);
    } else {
        snprintf(l2, sizeof(l2), "ERROR: no testN.bmp on SD card");
        snprintf(l3, sizeof(l3), "copy test1.bmp... to SD root");
        hud_draw_3lines(l1, l2, l3, 255, 120, 120, 200, 220, 255);
    }
}

static void draw_busy_hud(const char* name) {
    char l1[64], l2[64], l3[64];
    snprintf(l1, sizeof(l1), "YOLOv4-tiny 640x640");
    snprintf(l2, sizeof(l2), "detecting %s ...", name);
    snprintf(l3, sizeof(l3), "please wait (~1s)");
    hud_draw_3lines(l1, l2, l3, 255, 220, 120, 200, 220, 255);
}

static void draw_hud(int img, int nimgs, int nd,
                     int t_read, int t_prep, int t_infer, int t_dec, int t_all) {
    char l1[64], l2[96], l3[64];

    snprintf(l1, sizeof(l1), "YOLOv4-tiny 640x640");
    snprintf(l2, sizeof(l2), "infer %4d ms | read %3d | prep %3d | dec %2d | all %4d",
             t_infer, t_read, t_prep, t_dec, t_all);
    snprintf(l3, sizeof(l3), "frame %d/%d  det %d  static image",
             img, nimgs, nd);

    hud_draw_3lines(l1, l2, l3, 160, 255, 160, 200, 220, 255);
}

typedef struct {
    int prep, infer, dec, all, nd;
} DetStat;

static void detect_src(int w, int h, int ox, int oy, const char* tag,
                       int read_ms, u32 t_start, DetStat* st)
{
    u32 t2 = now_us();
    if (g_lb.src_w != w || g_lb.src_h != h) {
        letterbox_init(&g_lb, w, h, !PREP_LETTERBOX);
        xil_printf("prep: src %dx%d -> %dx%d pad(%d,%d) mode=%s\r\n",
                   w, h, g_lb.new_w, g_lb.new_h, g_lb.pad_x, g_lb.pad_y,
                   PREP_LETTERBOX ? "letterbox" : "stretch");
    }
    yolo_set_letterbox(&g_lb);
    prep_image(src_buf, w, h, &g_lb, in_img);
    u32 t3 = now_us();

    xil_printf("[%s] forward start: %dx%d, prep %dms -> waiting conv IP\r\n",
               tag, w, h, (int)((t3 - t2) / 1000));
    Xil_DCacheFlushRange((u32)in_img, sizeof(in_img));
    g_yolo->forward(in_img, out0, out1);
    Xil_DCacheInvalidateRange((u32)out0, sizeof(out0));
    Xil_DCacheInvalidateRange((u32)out1, sizeof(out1));
    u32 t4 = now_us();

    int nd = yolo_decode(out0, out1, dets, 128);
    u32 t5 = now_us();

    for (int i = 0; i < nd; i++) {
        draw_detection(fb, DISP_W, DISP_H, &dets[i],
                       CLASS_NAMES[dets[i].cls], ox, oy);
    }
    Xil_DCacheFlushRange((u32)fb, FB_BYTES);

    xil_printf("[%s] read=%3dms prep=%3dms infer=%4dms dec=%2dms total=%4dms det=%d\r\n",
               tag, read_ms,
               (int)((t3 - t2) / 1000), (int)((t4 - t3) / 1000),
               (int)((t5 - t4) / 1000), (int)((t5 - t_start) / 1000), nd);

    for (int i = 0; i < nd; i++) {
        int sc = (int)(dets[i].score * 100.0f + 0.5f);
        xil_printf("    %s %d.%02d  (%d,%d)-(%d,%d)\r\n",
                   CLASS_NAMES[dets[i].cls], sc / 100, sc % 100,
                   (int)(dets[i].x1 + 0.5f), (int)(dets[i].y1 + 0.5f),
                   (int)(dets[i].x2 + 0.5f), (int)(dets[i].y2 + 0.5f));
    }

    gauge_overlay(fb, DISP_W, DISP_H, dets, nd, src_buf, w, h, ox, oy);
    Xil_DCacheFlushRange((u32)fb, FB_BYTES);

    st->prep  = (int)((t3 - t2) / 1000);
    st->infer = (int)((t4 - t3) / 1000);
    st->dec   = (int)((t5 - t4) / 1000);
    st->all   = (int)((t5 - t_start) / 1000);
    st->nd    = nd;
}

static int cam_task(void)
{
    int w = 0, h = 0, fmt = 0;
    int cw = 0, ch = 0;
    int ox = 0, oy = 0;
    char l1[64], l2[64], l3[64];

    if (cam_link_get_frame(cam_rx, (int)CAM_BUF_BYTES, &w, &h, &fmt) != 1)
        return 0;

    u32 t0 = now_us();

    int ok = (fmt == CAM_FMT_GRAY8)
           ? cam_gray_to_bgr888(src_buf, (int)SRC_BUF_BYTES, cam_rx, w, h, &cw, &ch)
           : cam_rgb565_to_bgr888(src_buf, (int)SRC_BUF_BYTES, cam_rx, w, h, &cw, &ch);
    if (!ok) {
        xil_printf("cam: %dx%d frame dropped (does not fit %dx%d)\r\n",
                   w, h, DISP_W, DISP_H);
        cam_link_ack();
        return 1;
    }
    u32 t1 = now_us();

    if (cw != cam_w || ch != cam_h) {
        fb_clear_black();
        cam_w = cw;
        cam_h = ch;
        xil_printf("cam: frame %dx%d %s -> display %dx%d\r\n", w, h,
                   fmt ? "rgb565" : "gray8", cw, ch);
    }
    wait_frame_start();
    fb_show(src_buf, cw, ch, &ox, &oy);

    snprintf(l1, sizeof(l1), "CAM %dx%d %s", w, h, fmt ? "RGB565" : "GRAY8");
    snprintf(l2, sizeof(l2), "frame #%u  detecting ...", (unsigned)cam_link_frames());
    snprintf(l3, sizeof(l3), "link drops %u", (unsigned)cam_link_drops());
    hud_draw_3lines(l1, l2, l3, 255, 220, 120, 200, 220, 255);
    Xil_DCacheFlushRange((u32)fb, FB_BYTES);

    DetStat st;
    detect_src(cw, ch, ox, oy, "cam", (int)((t1 - t0) / 1000), t0, &st);

    snprintf(l2, sizeof(l2), "infer %4d ms | prep %3d | dec %2d | all %4d",
             st.infer, st.prep, st.dec, st.all);
    snprintf(l3, sizeof(l3), "frame #%u  det %d  drops %u",
             (unsigned)cam_link_frames(), st.nd, (unsigned)cam_link_drops());
    hud_draw_3lines(l1, l2, l3, 160, 255, 160, 200, 220, 255);
    Xil_DCacheFlushRange((u32)fb, FB_BYTES);

    cam_link_ack();
    return 1;
}

int main(void) {
    xil_printf("\r\n=== YOLOv4-tiny HDMI auto camera mode (1024x600) ===\r\n");

    {
        u32 devcfg = Xil_In32(0xF800700Cu);
        xil_printf("diag: devcfg INT_STATUS=0x%08x PCFG_DONE=%d (expect 1)\r\n",
                   devcfg, (int)(devcfg & 1u));
    }

    Xil_ExceptionInit();
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_DATA_ABORT_INT,
                                 (Xil_ExceptionHandler)data_abort_handler, (void*)0);
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_PREFETCH_ABORT_INT,
                                 (Xil_ExceptionHandler)prefetch_abort_handler, (void*)0);
    Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_UNDEFINED_INT,
                                 (Xil_ExceptionHandler)undefined_handler, (void*)0);
    Xil_ExceptionDisable();   /* interrupts are not used by this application */

    VideoMode vd_mode = VMODE_1024x600;
    xil_printf("init: vdma...\r\n");
    run_vdma_frame_buffer(&vdma, VDMA_ID, vd_mode.width, vd_mode.height,
                          FB_ADDR, 0, 0, ONLY_READ);
    xil_printf("init: vdma ok, vtc...\r\n");
    DisplayInitialize(&dispCtrl, DISP_VTC_ID);
    DisplaySetMode(&dispCtrl, &vd_mode);
    DisplayStart(&dispCtrl);
    fb_clear_black();
    xil_printf("HDMI 1024x600 ready\r\n");

    int nimgs = 0;
    int sdrc = SD_Init();
    if (sdrc != XST_SUCCESS) {
        xil_printf("ERROR: SD mount failed; weights and images cannot be loaded\r\n");
    } else {
        nimgs = find_img_count();
        xil_printf("SD: %d test image(s) found\r\n", nimgs);
        gauge_scale_load("gauge_scale.txt");
    }

    conv_init();
    g_yolo = new Yolo4_Tiny();
    xil_printf("loading gauge weights...\r\n");
    g_yolo->load_weight("gauge_weights");
    xil_printf("weights loaded, failed files: %d\r\n", (int)g_sd_status[3]);

    /* Never run inference on zeroed weights: show the diagnosis instead. */
    if (g_sd_status[3] != 0 || g_sd_status[7] == 0) {
        char l1[64], l2[64], l3[64];
        snprintf(l1, sizeof(l1), "SD/WEIGHTS ERROR  mount=%d open=%d",
                 (int)g_sd_status[0], (int)g_sd_status[1]);
        snprintf(l2, sizeof(l2), "failed files: %d  size %d vs %d",
                 (int)g_sd_status[3], (int)g_sd_status[4], (int)g_sd_status[5]);
        snprintf(l3, sizeof(l3), "put gauge_weights + testN.bmp on SD");
        hud_draw_3lines(l1, l2, l3, 255, 90, 90, 255, 220, 120);
        Xil_DCacheFlushRange((u32)fb, FB_BYTES);
        xil_printf("FATAL: weights unusable, idling. mount=%d open=%d fails=%d size=%d/%d\r\n",
                   (int)g_sd_status[0], (int)g_sd_status[1], (int)g_sd_status[3],
                   (int)g_sd_status[4], (int)g_sd_status[5]);
        for (;;) {
            sleep(1);
        }
    }

    g_lb.src_w = 0;
    g_lb.src_h = 0;

    if (nimgs > 0) {
        int n = (nimgs > MAX_IMGS) ? MAX_IMGS : nimgs;
        draw_ready_hud(n);
        Xil_DCacheFlushRange((u32)fb, FB_BYTES);
        xil_printf("\r\nmode: STATIC IMAGE x%d (no camera, no handshake)\r\n", n);

        for (;;) {
            for (int i = 1; i <= n; i++) {
                char name[32];
                int w = 0, h = 0;
                snprintf(name, sizeof(name), IMG_FMT, i);

                u32 t0 = now_us();
                int rc = bmp_load(name, src_buf, SRC_BUF_BYTES, &w, &h);
                int read_ms = (int)((now_us() - t0) / 1000);
                if (rc != 0) {
                    xil_printf("IMG %s: load failed rc=%d\r\n", name, rc);
                    continue;
                }
                xil_printf("IMG %s: %dx%d, read %dms\r\n", name, w, h, read_ms);

                int ox = 0, oy = 0;
                fb_clear_black();
                wait_frame_start();
                fb_show(src_buf, w, h, &ox, &oy);
                draw_busy_hud(name);
                Xil_DCacheFlushRange((u32)fb, FB_BYTES);

                DetStat st;
                detect_src(w, h, ox, oy, name, read_ms, t0, &st);

                draw_hud(i, n, st.nd, read_ms, st.prep, st.infer, st.dec, st.all);
                Xil_DCacheFlushRange((u32)fb, FB_BYTES);

                sleep(3);
            }
        }
    }

    xil_printf("\r\nmode: CAMERA (no testN.bmp on SD card)\r\n");
    cam_link_init();
    cam_link_diag();
    /* Clear the framebuffer before drawing the banner: on a warm start the DDR
       still holds the previous frame, and the VDMA keeps scanning it, so without
       this the screen would show a stale image behind the text. */
    fb_clear_black();
    hud_draw_3lines("CAMERA AUTO MODE", "waiting for OpenMV frame", "no testN.bmp on SD card",
                    160, 255, 160, 200, 220, 255);
    Xil_DCacheFlushRange((u32)fb, FB_BYTES);

    for (;;) {
        cam_link_pump();
        if (cam_task()) continue;
    }

    return 0;
}
