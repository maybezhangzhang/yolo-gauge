/**
 * @file    conv.cpp
 * @brief   YOLOv4-Tiny 卷积加速器顶层 IP —— 算子统一调度入口（IP 名字：conv_v0_0）
 *
 * 本文件是整个硬件加速器的“入口函数”。它本身不实现任何算术运算，只做一件事：
 * 根据软件侧下发的 kernel 参数，把这次调用分派到对应的硬件子模块：
 *     kernel == 3  ->  std_conv()  3x3 标准卷积（含 1x1 之外的全部卷积）
 *     kernel == 1  ->  pwconv()    1x1 逐点卷积（point-wise conv）
 *     kernel == 2  ->  maxpool()   2x2 最大池化
 *     kernel == 0  ->  upsample()  2 倍最近邻上采样
 *
 * 硬件接口分为两类：
 *   1) CTRL  —— s_axilite 寄存器组，CPU 通过 AXI-Lite 写这些标量参数并启动 IP；
 *   2) m_axi —— 高位宽 (64bit) 大带宽数据通道，直接读写 DDR 里的特征图/权重。
 *
 * 数据通路按“4 个 AXI 通道并行”设计（in1~in4 / w1~w4 / out1~out4），
 * 每个通道一次搬 4 个 data_t（4 x ap_fixed<16,7> = 64bit），
 * 所以 DSP 阵列的并行度是 Tm(32) x Tn(4) 之类的量级：
 *     in1..in4   负责通道方向 4 个连续通道的输入特征图
 *     w1..w4     负责 m 方向 4 个连续输出通道的权重
 *     out1..out4 负责 m 方向 4 个连续输出通道的结果写回
 */

#include"conv.h"


/**
 * @brief  加速器顶层函数：按 kernel 类型把一次算子调用分派到具体硬件模块
 *
 * @param  in1,in2,in3,in4   输入特征图在 DDR 中的基地址（4 个 AXI 通道并行，每通道 1 份）
 * @param  w1,w2,w3,w4       权重在 DDR 中的基地址（4 个 AXI 通道并行，按输出通道 m 切分）
 * @param  b                 bias 基地址（复用 W1 通道，深度只需 128，即最多 128 个输出通道）
 * @param  out1,out2,out3,out4 输出特征图基地址（与输入共用同一组 FM 通道，读写复用）
 * @param  ch_in             输入通道数 N
 * @param  ch_out            输出通道数 M
 * @param  fsize             输入特征图边长（正方形，单位：像素/元素）
 * @param  stride            卷积步长，1 或 2（stride=2 时输出尺寸减半）
 * @param  kernel            算子选择码：3=3x3卷积，1=1x1卷积，2=最大池化，0=上采样
 * @param  act               激活选择：1 = 打开 LeakyReLU(alpha=0.1)，其它 = 直通不激活
 *
 * @note   该函数没有任何 return 值语义，return 端口只用于 AXI-Lite 的“启动/完成”握手。
 * @note   所有 m_axi 端口都带 offset=slave，意味着地址在运行时由 CPU 通过 AXI-Lite 写入，
 *         而不是硬编码在 RTL 里——这也是同一套硬件能支持 YOLOv4-Tiny 上百层的根本原因。
 */
void conv(data_t* in1,data_t* in2,data_t* in3,data_t* in4,data_t* w1,data_t* w2,data_t* w3,
data_t* w4,data_t* b,data_t* out1,data_t* out2,data_t* out3,data_t* out4,
int ch_in,int ch_out,int fsize,int stride,int kernel,int act){
//PRAGMA

// ------------------------- CTRL：AXI-Lite 标量寄存器组 -------------------------
// 这些端口会被打包进同一个 CTRL bundle，在 Vivado 里映射成一块 AXI-Lite 从设备寄存器区，
// CPU 依次写 fsize/ch_out/act/stride/ch_in/kernel，再读 return 寄存器判断运算是否结束。
#pragma HLS INTERFACE s_axilite port=return bundle=CTRL
#pragma HLS INTERFACE s_axilite port=fsize bundle=CTRL
#pragma HLS INTERFACE s_axilite port=ch_out bundle=CTRL
#pragma HLS INTERFACE s_axilite port=act bundle=CTRL
#pragma HLS INTERFACE s_axilite port=stride bundle=CTRL
#pragma HLS INTERFACE s_axilite port=ch_in bundle=CTRL
#pragma HLS INTERFACE s_axilite port=kernel bundle=CTRL

// ------------------------- FM：特征图数据通道（输入侧） -------------------------
// depth=450000 刚好覆盖 416x416=173056 的最大特征图，同时避免 LUT 爆增
#pragma HLS INTERFACE m_axi depth=450000 port=in1 offset=slave bundle=FM1 max_read_burst_length=256 max_write_burst_length=256
#pragma HLS INTERFACE m_axi depth=450000 port=in2 offset=slave bundle=FM2 max_read_burst_length=256 max_write_burst_length=256
#pragma HLS INTERFACE m_axi depth=450000 port=in3 offset=slave bundle=FM3 max_read_burst_length=256 max_write_burst_length=256
#pragma HLS INTERFACE m_axi depth=450000 port=in4 offset=slave bundle=FM4 max_read_burst_length=256 max_write_burst_length=256

// ------------------------- W：权重数据通道 -------------------------
// depth 够用就行，大 depth 会增加 LUT
#pragma HLS INTERFACE m_axi depth=65536 port=w1 offset=slave bundle=W1 max_read_burst_length=256
#pragma HLS INTERFACE m_axi depth=65536 port=w2 offset=slave bundle=W2 max_read_burst_length=256
#pragma HLS INTERFACE m_axi depth=65536 port=w3 offset=slave bundle=W3 max_read_burst_length=256
#pragma HLS INTERFACE m_axi depth=65536 port=w4 offset=slave bundle=W4 max_read_burst_length=256

// bias 只有 ch_out 个（<=128），所以复用 W1 通道即可。
#pragma HLS INTERFACE m_axi depth=128 port=b offset=slave bundle=W1 max_read_burst_length=256

// ------------------------- FM：特征图数据通道（输出侧） -------------------------
// 输出写回复用 FM1~FM4，depth 同输入
#pragma HLS INTERFACE m_axi depth=450000 port=out1 offset=slave bundle=FM1
#pragma HLS INTERFACE m_axi depth=450000 port=out2 offset=slave bundle=FM2
#pragma HLS INTERFACE m_axi depth=450000 port=out3 offset=slave bundle=FM3
#pragma HLS INTERFACE m_axi depth=450000 port=out4 offset=slave bundle=FM4
//content

	// ============================ 算子分派 ============================
	// kernel 是一个“多路复用选择码”，把 4 种算子塞进同一块硬件，
	// 用寄存器换面积：YOLOv4-Tiny 中 3x3/1x1/maxpool/upsample 的比例约为 1:2:1:1，
	// 分时复用足以跑满，没必要给每种算子单独出一套 IP。
	if(kernel==3){
		// 3x3 标准卷积。原本还有一条 Winograd 快路径（3x3 用 Winograd 可省 2.25x 乘法），
		// 但这里被注释掉了，说明当前版本走的是直接卷积：原因通常是 Winograd 的变换矩阵
		// 定点误差太大（data_t 只有 9 位小数），量化后掉点明显，于是退回 std_conv。
		//WinogradConv(in1,in2,in3,in4,w1,w2,w3,w4,b,out1,out2,ch_out,ch_in,fsize,stride,act);
		std_conv(in1,in2,in3,in4,w1,w2,w3,w4,b,out1,out2,out3,out4,ch_in,ch_out,fsize,stride,act);
	}
	else if(kernel==1){
		// 1x1 逐点卷积：没有空间滑窗，只做通道方向的矩阵乘（M x N 的 GEMM），
		// 所以不需要 w2/w3/w4 之外的多余输入端口，也不需要 out2~out4 的独立处理逻辑；
		// 这里只传 in1 / out1，由 pwconv 内部自己按 4 通道并行展开。
		pwconv(in1,in2,in3,in4,w1,b,out1,ch_in,ch_out,fsize,act);
	}
	else if(kernel==2){     //kernel==2 maxpool; kernel==0 upsample
		// 2x2 最大池化：stride 固定为 2、kernel 固定为 2，因此不需要额外的 stride 参数；
		// 池化不改变通道数，所以 ch_in 直接当通道数用，输出尺寸由 maxpool 内部按 2 倍缩小处理。
		maxpool(in1,out1,ch_in,fsize,fsize);
	}
	else{
		// 其余情况（约定 kernel==0）一律走上采样：YOLOv4-Tiny 的 FPN 支路需要 2 倍最近邻上采样，
		// 逻辑最简单（每个像素复制成 2x2），所以放在最后作为默认分支，
		// 这样即使软件侧下发了未定义的 kernel 值，硬件也不会挂死，而是退化成一个确定行为。
		upsample(in1,out1,fsize);
	}
}
