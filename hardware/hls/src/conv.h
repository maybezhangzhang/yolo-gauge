/**
 * @file    conv.h
 * @brief   YOLOv4-Tiny 加速器顶层 IP 的头文件（IP 名字：conv_v0_0）
 *
 * 这里只做两件事：
 *   1) 通过包含 3 个子模块头文件，把子模块声明与公共数据类型 data_t 引进来；
 *   2) 声明顶层函数 conv()，供 testbench（test_tb.cpp）和综合使用。
 *
 * 头文件包含顺序是有依赖的：
 *   pwconv.h   定义 data_t = ap_fixed<16,7,AP_RND,AP_SAT>，以及 Tp/Tn/Tm/MAX_LEN
 *   std_conv.h 定义 Tr/Tc/Tn/Tm/K/P/TRin/TCin，同样定义 data_t 和 MAX_LEN
 *   maxpool.h -> upsample.h，再次定义 data_t
 * 三个头文件里 data_t 的 typedef 完全一致，所以重复包含不会冲突（同一类型重复定义是合法的），
 * 但这也意味着 data_t 的格式一旦要改，必须三个头文件同时改——这是这份代码的一个隐患。
 */
#include"pwconv.h"
#include"std_conv.h"
#include"maxpool.h"

/**
 * @brief  加速器顶层函数声明（实现见 conv.cpp）
 *
 * @param  in1~in4    输入特征图 DDR 基地址，4 个 AXI 通道（FM1~FM4）
 * @param  w1~w4      权重 DDR 基地址，4 个 AXI 通道（W1~W4）
 * @param  b          bias 基地址，复用 W1 通道，深度 128
 * @param  out1~out4  输出特征图 DDR 基地址，复用 FM1~FM4
 * @param  ch_in      输入通道数
 * @param  ch_out     输出通道数
 * @param  fsize      输入特征图边长（正方形）
 * @param  stride     步长，1 或 2
 * @param  kernel     算子选择：3=3x3卷积，1=1x1卷积，2=最大池化，0=上采样
 * @param  act        激活选择：1=LeakyReLU，其它=不激活
 *
 * @note   参数用 int 而不是 unsigned short，是为了和 AXI-Lite 的 32 位寄存器位宽对齐，
 *         避免 HLS 在端口上额外插入位宽转换逻辑；进入子模块后会被窄化成 unsigned short。
 */
void conv(data_t* in1,data_t* in2,data_t* in3,data_t* in4,data_t* w1,data_t* w2,data_t* w3,
data_t* w4,data_t* b,data_t* out1,data_t* out2,data_t* out3,data_t* out4,int ch_in,int ch_out,
int fsize,int stride,int kernel,int act);
