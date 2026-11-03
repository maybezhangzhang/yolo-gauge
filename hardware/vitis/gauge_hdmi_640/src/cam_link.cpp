/*
 * cam_link.cpp ---- 串口摄像头图像链路实现
 *
 * 设计要点:
 *  1) 全部用寄存器直接读写 (不依赖 xuartps / xscugic 驱动),
 *     这样不管 BSP 是从哪个版本的 XSA 生成的, 都能编过, 也不会和
 *     xil_printf 抢 UART0。
 *  2) 接收走环形缓冲: 中断里只做"搬字节"这一件事, 解析放在主循环。
 *  3) 解析失败只丢 1 个字节重新找帧头, 所以上电乱码、拔插都不会卡死。
 */
#include "cam_link.h"

#include "xil_io.h"
#include "xil_printf.h"
#if (CAM_LINK_USE_IRQ != 0)
#include "xscugic.h"
#include "xparameters.h"
#endif
#include "xil_exception.h"

#include <string.h>

/* ==================== 硬件地址 ==================== */
#define UART0_BASE      0xE0000000u
#define UART1_BASE      0xE0001000u

#if (CAM_LINK_UART_SEL == 0)
  #define CAM_UART_BASE   UART0_BASE
  #define CAM_UART_IRQ    59u
  #define CAM_IS_UART0    1
#else
  #define CAM_UART_BASE   UART1_BASE
  #define CAM_UART_IRQ    82u
  #define CAM_IS_UART0    0
#endif

/* GIC (Cortex-A9 中断控制器) */
#define GICD_BASE       0xF8F01000u
#define GICC_BASE       0xF8F00100u

#define SLCR_APER_CLK   0xF800012Cu   /* bit21 = UART1 的 APB 时钟使能 */
#define SLCR_UART_CLK   0xF8000154u   /* UART1 时钟控制: BIT0|BIT1|DIV=0x12 (100MHz) */

/* ==================== UART 寄存器偏移 (UG585) ==================== */
#define U_CR        0x00u   /* 控制     */
#define U_MR        0x04u   /* 模式     */
#define U_IER       0x08u   /* 中断使能 */
#define U_IDR       0x0Cu   /* 中断禁止 */
#define U_IMR       0x10u   /* 中断屏蔽状态 */
#define U_ISR       0x14u   /* 中断状态 (写 1 清除) */
#define U_BAUDGEN   0x18u   /* 波特率发生器 CD */
#define U_RXTOUT    0x1Cu   /* 接收超时 */
#define U_RXWM      0x20u   /* RX FIFO 触发水线 */
#define U_MODEMCR   0x24u
#define U_MODEMSR   0x28u
#define U_SR        0x2Cu   /* 通道状态 */
#define U_FIFO      0x30u   /* 收发 FIFO (同一地址) */
#define U_BAUDDIV   0x34u   /* 波特率分频 BDIV */

#define U_CR_RXRST  0x00000001u
#define U_CR_TXRST  0x00000002u
#define U_CR_RXEN   0x00000004u
#define U_CR_TXEN   0x00000010u
#define U_CR_RSTTO  0x00000040u

#define U_MR_8N1    0x00000020u   /* PAR=none, 8bit, 1 stop, 普通模式 */
#define U_SR_RXEMPTY 0x00000002u
#define U_ISR_RTRIG  0x00000001u  /* RX FIFO 到达水线 */
#define U_SR_RXOVR   0x00000001u  /* RX FIFO 溢出 (写 1 清除) */

/* ==================== 接收缓冲 ==================== */
#define RB_BITS     21u                 /* 2 MB: 需装下整帧 RGB565 VGA (640*480*2 + 头尾 ≈ 600KB) */
#define RB_SIZE     (1u << RB_BITS)
#define RB_MASK     (RB_SIZE - 1u)

static u8  s_rb[RB_SIZE];
static volatile u32 s_head = 0;         /* 生产者 (中断/查询) */
static volatile u32 s_tail = 0;         /* 消费者 (解析)     */

static volatile u32 s_frames = 0;       /* 成功收帧数   */
static volatile u32 s_drops  = 0;       /* 丢弃/重同步数 */
static volatile u32 s_ovf    = 0;       /* 缓冲溢出次数  */
static volatile u32 s_ovr    = 0;       /* FIFO 溢出次数 (硬件丢字节) */
static volatile u32 s_bad    = 0;       /* 校验失败帧数  */
static volatile u32 s_lastlen = 0;      /* 上一帧声明的 payload 长度 */
static volatile u32 s_lastsum = 0;      /* 上一帧计算出的累加和 */
static volatile u32 s_lastchk = 0;      /* 上一帧收到的累加和 */
static int s_ready = 0;

#define RB_COUNT()  ((s_head - s_tail) & RB_MASK)
#define RB_AT(i)    (s_rb[((s_tail) + (i)) & RB_MASK])
#define RB_DROP(n)  do { s_tail = (s_tail + (u32)(n)) & RB_MASK; } while (0)

/* 允许的 payload 上限 */
#define CAM_MAX_PAYLOAD ((u32)CAM_LINK_MAX_W * (u32)CAM_LINK_MAX_H * 2u)

/* 帧格式常量 */
#define P_HEAD0   0xA5u
#define P_HEAD1   0x5Au
#define P_TAIL0   0x5Au
#define P_TAIL1   0xA5u
#define P_VER     0x02u
#define P_HDR     12u          /* 帧头长度: 2 头 + ver + fmt + w(2) + h(2) + len(4) */
#define P_OVER    4u           /* 尾部: sum(2) + 5A A5(2) */

/* ==================== 环形缓冲写入 ==================== */
static inline void rb_put(u8 b)
{
    u32 h = s_head;
    u32 n = (h + 1u) & RB_MASK;
    if (n == s_tail) {              /* 满 */
        s_ovf++;
        return;
    }
    s_rb[h] = b;
    s_head = n;
}

/* ==================== 读 FIFO ==================== */
static volatile int s_in_pump = 0;

void cam_link_pump(void)
{
    u32 base = CAM_UART_BASE;
    int guard = 0;

    if (s_in_pump) return;   /* other context is draining */
    s_in_pump = 1;

    /* 最多搬 4KB, 防止 ISR 里呆太久 */
    while (guard++ < 4096) {
        u32 sr = Xil_In32(base + U_SR);

        if (sr & U_SR_RXOVR) {                 /* 硬件 FIFO 溢出: 已经丢字节了 */
            Xil_Out32(base + U_SR, U_SR_RXOVR); /* 写 1 清除 */
            s_ovr++;
        }
        if (sr & U_SR_RXEMPTY) break;
        rb_put((u8)(Xil_In32(base + U_FIFO) & 0xFFu));
    }
    s_in_pump = 0;
}

/* ==================== 中断 ==================== */
#if (CAM_LINK_USE_IRQ != 0)
static XScuGic s_gic;
#endif

static void cam_uart_isr(void* ref)
{
    u32 base = CAM_UART_BASE;
    (void)ref;
    u32 st = Xil_In32(base + U_ISR);

    if (st != 0u) Xil_Out32(base + U_ISR, st);   /* 写 1 清除 */
    cam_link_pump();
}

static void cam_gic_isr(void* ref)
{
    (void)ref;
    u32 id = Xil_In32(GICC_BASE + 0x0Cu) & 0x3FFu;   /* ICCIAR 取中断号 */
    if (id == CAM_UART_IRQ) cam_uart_isr((void*)0);
    Xil_Out32(GICC_BASE + 0x10u, id);                /* ICCEOIR 结束 */
}

static void gic_enable(u32 irq)
{
    u32 w = irq / 32u, b = irq % 32u;
    u32 t = irq / 4u,  s = irq % 4u;
    u32 c = irq / 16u, s2 = (irq % 16u) * 2u;
    u32 v;

    Xil_Out32(GICD_BASE + 0x000u, 0x00000001u);              /* 分发器使能 */
    Xil_Out32(GICD_BASE + 0x100u + w * 4u, 1u << b);         /* 使能该中断 */

    /* 目标 CPU0 */
    v  = Xil_In32(GICD_BASE + 0x800u + t * 4u);
    v &= ~(0xFFu << (s * 8u));
    v |=  (0x01u << (s * 8u));
    Xil_Out32(GICD_BASE + 0x800u + t * 4u, v);

    /* 触发方式: 0b01 = 电平触发 (PS 外设 UART1 是电平中断, 不能设成边沿) */
    v  = Xil_In32(GICD_BASE + 0xC00u + c * 4u);
    v &= ~(0x3u << s2);
    v |=  (0x1u << s2);
    Xil_Out32(GICD_BASE + 0xC00u + c * 4u, v);

    Xil_Out32(GICC_BASE + 0x004u, 0x000000F0u);              /* 优先级掩码 */
    Xil_Out32(GICC_BASE + 0x000u, 0x00000001u);              /* CPU 接口使能 */
}

/* ==================== 波特率计算 ==================== */
static void uart_calc_baud(u32 refclk, u32 baud, u32* pgen, u32* pdiv)
{
    u32 best_err = 0xFFFFFFFFu, best_gen = 1u, best_div = 4u;
    u32 div, gen, actual, err;

    for (div = 4u; div <= 255u; div++) {
        u32 denom = baud * (div + 1u);
        if (denom == 0u) continue;
        gen = (refclk + denom / 2u) / denom;
        if (gen == 0u) continue;
        if (gen > 0xFFFFu) gen = 0xFFFFu;
        actual = refclk / ((div + 1u) * gen);
        err = (actual > baud) ? (actual - baud) : (baud - actual);
        /* 误差相同时优先更大的 BDIV: 过采样倍数高, 高波特率下采样更稳 */
        if (err < best_err || (err == best_err && div > best_div)) {
            best_err = err; best_gen = gen; best_div = div;
        }
    }
    *pgen = best_gen;
    *pdiv = best_div;
}

/* ==================== UART1 初始化 (只在这一路做完整配置) ==================== */
static void uart1_hw_init(u32 refclk, u32 baud)
{
    u32 gen = 0u, div = 0u;
    int i;

    /* 先确保 UART1 的 APB 时钟是开的, 否则写寄存器会被忽略甚至挂总线 */
    Xil_Out32(0xF8000008u, 0xDF0Du); /* SLCR unlock */
    /* UART1 参考时钟: 从 SD/BOOT.BIN 启动时, FSBL 里烧死的 ps7_init 可能还是旧版
       (UART_CLK_CTRL=0x1201, BIT1=0), 那样 UART1 会一直停在复位态: CR 的
       RXRST/TXRST 永远读回 1, 收不到任何字节, 屏幕只剩 HUD。
       这里自己写一次, 与当前 hw/ps7_init.tcl 的 0x1203 保持一致。 */
    Xil_Out32(SLCR_UART_CLK, (Xil_In32(SLCR_UART_CLK) & 0xFFFFC000u) | 0x00001203u);
    Xil_Out32(SLCR_APER_CLK, Xil_In32(SLCR_APER_CLK) | (1u << 21));
    Xil_Out32(0xF8000004u, 0x767Bu); /* SLCR lock */
    xil_printf("cam: uart1 clk 0xF8000154 = 0x%08x\r\n", (unsigned)Xil_In32(SLCR_UART_CLK));

    Xil_Out32(UART1_BASE + U_IDR, 0xFFFFFFFFu);
    Xil_Out32(UART1_BASE + U_CR, 0x28u);                   /* RXDIS | TXDIS */
    Xil_Out32(UART1_BASE + U_CR, U_CR_RXRST | U_CR_TXRST);  /* 复位 FIFO */
    for (i = 0; i < 100000; i++) {
        if ((Xil_In32(UART1_BASE + U_CR) & (U_CR_RXRST | U_CR_TXRST)) == 0u) break;
    }

    Xil_Out32(UART1_BASE + U_MR, U_MR_8N1);

    uart_calc_baud(refclk, baud, &gen, &div);
    Xil_Out32(UART1_BASE + U_BAUDGEN, gen);
    Xil_Out32(UART1_BASE + U_BAUDDIV, div);

    Xil_Out32(UART1_BASE + U_RXWM, 16u);      /* 每 16 字节触发一次中断 */
    Xil_Out32(UART1_BASE + U_RXTOUT, 32u);

    Xil_Out32(UART1_BASE + U_CR, U_CR_RXEN | U_CR_TXEN);
    Xil_Out32(UART1_BASE + U_CR, U_CR_RXEN | U_CR_TXEN | U_CR_RSTTO);

    xil_printf("cam: uart1 %d baud -> CD=%d BDIV=%d (actual %d)\r\n",
               (int)baud, (int)gen, (int)div,
               (int)(refclk / ((div + 1u) * gen)));
}

/* ==================== 对外: 初始化 ==================== */
int cam_link_init(void)
{
    s_head = s_tail = 0;
    s_frames = s_drops = s_ovf = 0;
    s_ready = 1;

#if (CAM_IS_UART0 == 0)
    uart1_hw_init(100000000u, CAM_LINK_BAUD);
#endif

    /* 清一下残留状态, 打开发送水线中断 */
    Xil_Out32(CAM_UART_BASE + U_ISR, 0xFFFFFFFFu);
    Xil_Out32(CAM_UART_BASE + U_IER, U_ISR_RTRIG);

#if (CAM_IS_UART0 == 0) && (CAM_LINK_USE_IRQ != 0)
    {
        XScuGic_Config* gic_cfg = XScuGic_LookupConfig(XPAR_SCUGIC_0_DEVICE_ID);
        if (gic_cfg != NULL) {
            XScuGic_CfgInitialize(&s_gic, gic_cfg, gic_cfg->CpuBaseAddress);
            XScuGic_SetPriorityTriggerType(&s_gic, CAM_UART_IRQ, 0xA0u, 0x3u);
            XScuGic_Connect(&s_gic, CAM_UART_IRQ,
                            (Xil_ExceptionHandler)cam_uart_isr, (void*)0);
            XScuGic_Enable(&s_gic, CAM_UART_IRQ);
            Xil_ExceptionInit();
            Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
                                         (Xil_ExceptionHandler)XScuGic_InterruptHandler,
                                         (void*)&s_gic);
            Xil_ExceptionEnable();
            xil_printf("cam: uart1 irq %d via XScuGic\r\n", (int)CAM_UART_IRQ);
        } else {
            xil_printf("cam: XScuGic_LookupConfig failed, polling only\r\n");
        }
    }
#else
    Xil_Out32(CAM_UART_BASE + U_IDR, 0xFFFFFFFFu);   /* 查询模式: 关掉中断 */
    xil_printf("cam: polling mode (%s)\r\n", (CAM_IS_UART0 ? "UART0 115200" : "UART1"));
#endif

    return 0;
}

/* ==================== 对外: 取一帧 ==================== */
int cam_link_get_frame(u8* dst, int cap, int* pw, int* ph, int* pfmt)
{
    if (!s_ready || dst == 0) return 0;

    for (;;) {
        u32 n = RB_COUNT();
        u32 len, w, h, i, fmt;
        u32 sum = 0u, chk;
        u32 scanned = 0u;

        if (n < P_HDR + P_OVER) return 0;

        /* 帧头 */
        if (RB_AT(0) != P_HEAD0 || RB_AT(1) != P_HEAD1) { RB_DROP(1); s_drops++; if (++scanned >= 512u) return 0; continue; }

        if (RB_AT(2) != P_VER) { RB_DROP(1); s_drops++; if (++scanned >= 512u) return 0; continue; }

        fmt = (u32)RB_AT(3);
        w   = (u32)RB_AT(4) | ((u32)RB_AT(5) << 8);
        h   = (u32)RB_AT(6) | ((u32)RB_AT(7) << 8);
        len = (u32)RB_AT(8)  | ((u32)RB_AT(9)  << 8) |
              ((u32)RB_AT(10) << 16) | ((u32)RB_AT(11) << 24);

        if (w == 0u || h == 0u || w > CAM_LINK_MAX_W || h > CAM_LINK_MAX_H ||
            fmt > CAM_FMT_RGB565 || len == 0u || len > CAM_MAX_PAYLOAD ||
            len != w * h * ((fmt == CAM_FMT_GRAY8) ? 1u : 2u)) {
            RB_DROP(1); s_drops++; if (++scanned >= 512u) return 0; continue;
        }

        if (n < P_HDR + len + P_OVER) return 0;      /* 还没收全, 等 */

        for (i = 0u; i < len; i++) {
            sum += (u32)RB_AT(P_HDR + i);
            if ((i & 0x3FFu) == 0x3FFu) cam_link_pump();   /* 边算边收, 防 FIFO 溢出 */
        }
        sum &= 0xFFFFu;
        chk  = (u32)RB_AT(P_HDR + len) | ((u32)RB_AT(P_HDR + len + 1u) << 8);

        if (sum != chk ||
            RB_AT(P_HDR + len + 2u) != P_TAIL0 ||
            RB_AT(P_HDR + len + 3u) != P_TAIL1) {
            s_bad++;
            s_lastlen = len;
            s_lastsum = sum;
            s_lastchk = chk;
            RB_DROP(1); s_drops++; if (++scanned >= 512u) return 0; continue;
        }

        if ((int)len > cap) {                        /* 缓冲不够, 整帧丢掉 */
            RB_DROP(P_HDR + len + P_OVER);
            s_drops++;
            xil_printf("cam: frame %dx%d len=%d > cap %d, skipped\r\n",
                       (int)w, (int)h, (int)len, cap);
            continue;
        }

        for (i = 0u; i < len; i++) {
            dst[i] = RB_AT(P_HDR + i);
            if ((i & 0x3FFu) == 0x3FFu) cam_link_pump();   /* 边拷边收, 防 FIFO 溢出 */
        }
        RB_DROP(P_HDR + len + P_OVER);

        *pw = (int)w;
        *ph = (int)h;
        *pfmt = (int)fmt;
        s_frames++;
        return 1;
    }
}

/* ==================== 对外: ACK ==================== */
int cam_link_ack(void)
{
#if (CAM_IS_UART0 == 0)
    int i;
    for (i = 0; i < 1000; i++) {
        if ((Xil_In32(CAM_UART_BASE + U_SR) & 0x08u) == 0u) break;  /* TX FIFO 未满 */
    }
    Xil_Out32(CAM_UART_BASE + U_FIFO, (u32)'K');
#endif
    return 0;
}

/* ==================== 对外: 诊断 ==================== */
void cam_link_diag(void)
{
    xil_printf("cam: base=0x%08x mode=%s irq=%d\r\n",
               (unsigned)CAM_UART_BASE,
               (CAM_IS_UART0 ? "UART0" : "UART1"),
               (int)CAM_UART_IRQ);
    xil_printf("cam: CR=0x%08x MR=0x%08x SR=0x%08x IMR=0x%08x ISR=0x%08x\r\n",
               (unsigned)Xil_In32(CAM_UART_BASE + U_CR),
               (unsigned)Xil_In32(CAM_UART_BASE + U_MR),
               (unsigned)Xil_In32(CAM_UART_BASE + U_SR),
               (unsigned)Xil_In32(CAM_UART_BASE + U_IMR),
               (unsigned)Xil_In32(CAM_UART_BASE + U_ISR));
    xil_printf("cam: BAUDGEN=%d BAUDDIV=%d RXWM=%d\r\n",
               (int)Xil_In32(CAM_UART_BASE + U_BAUDGEN),
               (int)Xil_In32(CAM_UART_BASE + U_BAUDDIV),
               (int)Xil_In32(CAM_UART_BASE + U_RXWM));
    xil_printf("cam: frames=%d drops=%d ovf=%d pending=%d\r\n",
               (int)s_frames, (int)s_drops,
               (int)s_ovf, (int)RB_COUNT());
    xil_printf("cam: ovr=%d bad=%d last(len=%d sum=%04x chk=%04x)\r\n",
               (int)s_ovr, (int)s_bad, (int)s_lastlen,
               (unsigned)s_lastsum, (unsigned)s_lastchk);
}

u32 cam_link_frames(void) { return s_frames; }
u32 cam_link_drops(void)  { return s_drops;  }
