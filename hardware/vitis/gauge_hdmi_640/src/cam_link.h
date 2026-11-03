/*
 * cam_link.h ---- 串口摄像头图像链路 (OpenMV / 任意串口相机 -> Zynq)
 *
 * ================== 这个模块干什么 ==================
 * 从一路 PS UART 上收"一帧一帧的灰度图", 解析成裸像素交给 main.cpp,
 * 后面的 letterbox -> conv -> yolo_decode -> 画框 全部复用现有代码。
 *
 * 数据格式 (小端):
 *   A5 5A | ver(1) | fmt(1) | w(2) | h(2) | len(4) | payload[len] | sum(2) | 5A A5
 *   - ver  : 固定 0x02 (v1 的 2 字节 len 装不下 QVGA 灰度的 76800 字节, 故扩到 4 字节)
 *   - fmt  : 0 = GRAY8 (每像素 1 字节), 1 = RGB565 (每像素 2 字节)
 *   - w/h  : 图像宽高
 *   - len  : payload 字节数 = w*h*每像素字节数
 *   - sum  : payload 所有字节的算术和, 取低 16 位
 *
 * ================== 两种工作模式 ==================
 * CAM_LINK_UART_SEL = 0  用 UART0 (板载 CH340 调试口)
 *        - 用途: 改 Vivado 之前, 先用电脑当"假摄像头"把整条通路跑通
 *        - 波特率沿用 ps7_init 的设置 (115200), 不重配, 不动 CR/MR
 *        - 只用查询方式接收 (不收中断), 因为这时 CPU 本来就闲着
 * CAM_LINK_UART_SEL = 1  用 UART1 (EMIO 引到 PL 引脚, 接 OpenMV)
 *        - 波特率 CAM_LINK_BAUD, 默认 921600 (100MHz 参考时钟算分频, 实际约 925925)
 *        - 默认开中断接收, 这样推理(~1s)期间到达的字节也不会丢
 *
 * ================== 流控 ==================
 * 一帧收完 -> main 处理 -> 处理完调用 cam_link_ack() 回一个 'K'
 * OpenMV 端每发一帧都等这个 'K', 等不到就重发, 所以推理期间不会堆帧。
 */
#ifndef CAM_LINK_H_INCLUDED
#define CAM_LINK_H_INCLUDED

#include "xil_types.h"

/* ---------------- 配置: 改这里切换模式 ---------------- */
#define CAM_LINK_UART_SEL   1          /* 0 = UART0 调试口(电脑模拟), 1 = UART1 EMIO(接 OpenMV) */
#define CAM_LINK_BAUD       921600u     /* 与 OpenMV 端 BAUD 一致; 实测误码高则退回 921600u */
#define CAM_LINK_USE_IRQ    1          /* 查询模式 + 解析循环内续泵, 避免 FIFO 溢出 */

#define CAM_LINK_MAX_W      640
#define CAM_LINK_MAX_H      480

#define CAM_FMT_GRAY8       0
#define CAM_FMT_RGB565      1

/* 初始化 (可重复调用, 返回 0 成功) */
int  cam_link_init(void);

/* 把 RX FIFO 里的字节搬进内部缓冲; 主循环里每次都要调 (查询模式的唯一驱动) */
void cam_link_pump(void);

/*
 * 取一帧完整图像 (非阻塞)。
 *   dst/cap : 输出缓冲与容量 (cap 至少 w*h)
 *   pw/ph   : 输出宽高
 *   pfmt    : 输出格式 (CAM_FMT_GRAY8 / CAM_FMT_RGB565)
 * 返回 1 = 取到一帧; 0 = 还没有; -1 = 有待处理错误(仅记录, 不阻塞)
 */
int  cam_link_get_frame(u8* dst, int cap, int* pw, int* ph, int* pfmt);

/* 处理完一帧后回 ACK, 让 OpenMV 发下一帧 */
int  cam_link_ack(void);

/* 打印寄存器/统计, 用于排查接线 */
void cam_link_diag(void);

u32  cam_link_frames(void);
u32  cam_link_drops(void);

#endif /* CAM_LINK_H_INCLUDED */
