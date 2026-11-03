// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2020.2 (64-bit)
// Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
// ==============================================================
// control
// 0x00 : reserved
// 0x04 : reserved
// 0x08 : reserved
// 0x0c : reserved
// 0x10 : Data signal of in1
//        bit 31~0 - in1[31:0] (Read/Write)
// 0x14 : Data signal of in1
//        bit 31~0 - in1[63:32] (Read/Write)
// 0x18 : reserved
// 0x1c : Data signal of in2
//        bit 31~0 - in2[31:0] (Read/Write)
// 0x20 : Data signal of in2
//        bit 31~0 - in2[63:32] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of in3
//        bit 31~0 - in3[31:0] (Read/Write)
// 0x2c : Data signal of in3
//        bit 31~0 - in3[63:32] (Read/Write)
// 0x30 : reserved
// 0x34 : Data signal of in4
//        bit 31~0 - in4[31:0] (Read/Write)
// 0x38 : Data signal of in4
//        bit 31~0 - in4[63:32] (Read/Write)
// 0x3c : reserved
// 0x40 : Data signal of w1_r
//        bit 31~0 - w1_r[31:0] (Read/Write)
// 0x44 : Data signal of w1_r
//        bit 31~0 - w1_r[63:32] (Read/Write)
// 0x48 : reserved
// 0x4c : Data signal of w2_r
//        bit 31~0 - w2_r[31:0] (Read/Write)
// 0x50 : Data signal of w2_r
//        bit 31~0 - w2_r[63:32] (Read/Write)
// 0x54 : reserved
// 0x58 : Data signal of w3_r
//        bit 31~0 - w3_r[31:0] (Read/Write)
// 0x5c : Data signal of w3_r
//        bit 31~0 - w3_r[63:32] (Read/Write)
// 0x60 : reserved
// 0x64 : Data signal of w4_r
//        bit 31~0 - w4_r[31:0] (Read/Write)
// 0x68 : Data signal of w4_r
//        bit 31~0 - w4_r[63:32] (Read/Write)
// 0x6c : reserved
// 0x70 : Data signal of b
//        bit 31~0 - b[31:0] (Read/Write)
// 0x74 : Data signal of b
//        bit 31~0 - b[63:32] (Read/Write)
// 0x78 : reserved
// 0x7c : Data signal of out1
//        bit 31~0 - out1[31:0] (Read/Write)
// 0x80 : Data signal of out1
//        bit 31~0 - out1[63:32] (Read/Write)
// 0x84 : reserved
// 0x88 : Data signal of out2
//        bit 31~0 - out2[31:0] (Read/Write)
// 0x8c : Data signal of out2
//        bit 31~0 - out2[63:32] (Read/Write)
// 0x90 : reserved
// 0x94 : Data signal of out3
//        bit 31~0 - out3[31:0] (Read/Write)
// 0x98 : Data signal of out3
//        bit 31~0 - out3[63:32] (Read/Write)
// 0x9c : reserved
// 0xa0 : Data signal of out4
//        bit 31~0 - out4[31:0] (Read/Write)
// 0xa4 : Data signal of out4
//        bit 31~0 - out4[63:32] (Read/Write)
// 0xa8 : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XCONV_CONTROL_ADDR_IN1_DATA  0x10
#define XCONV_CONTROL_BITS_IN1_DATA  64
#define XCONV_CONTROL_ADDR_IN2_DATA  0x1c
#define XCONV_CONTROL_BITS_IN2_DATA  64
#define XCONV_CONTROL_ADDR_IN3_DATA  0x28
#define XCONV_CONTROL_BITS_IN3_DATA  64
#define XCONV_CONTROL_ADDR_IN4_DATA  0x34
#define XCONV_CONTROL_BITS_IN4_DATA  64
#define XCONV_CONTROL_ADDR_W1_R_DATA 0x40
#define XCONV_CONTROL_BITS_W1_R_DATA 64
#define XCONV_CONTROL_ADDR_W2_R_DATA 0x4c
#define XCONV_CONTROL_BITS_W2_R_DATA 64
#define XCONV_CONTROL_ADDR_W3_R_DATA 0x58
#define XCONV_CONTROL_BITS_W3_R_DATA 64
#define XCONV_CONTROL_ADDR_W4_R_DATA 0x64
#define XCONV_CONTROL_BITS_W4_R_DATA 64
#define XCONV_CONTROL_ADDR_B_DATA    0x70
#define XCONV_CONTROL_BITS_B_DATA    64
#define XCONV_CONTROL_ADDR_OUT1_DATA 0x7c
#define XCONV_CONTROL_BITS_OUT1_DATA 64
#define XCONV_CONTROL_ADDR_OUT2_DATA 0x88
#define XCONV_CONTROL_BITS_OUT2_DATA 64
#define XCONV_CONTROL_ADDR_OUT3_DATA 0x94
#define XCONV_CONTROL_BITS_OUT3_DATA 64
#define XCONV_CONTROL_ADDR_OUT4_DATA 0xa0
#define XCONV_CONTROL_BITS_OUT4_DATA 64

// CTRL
// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read/COR)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read)
//        bit 7  - auto_restart (Read/Write)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0  - enable ap_done interrupt (Read/Write)
//        bit 1  - enable ap_ready interrupt (Read/Write)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/TOW)
//        bit 0  - ap_done (COR/TOW)
//        bit 1  - ap_ready (COR/TOW)
//        others - reserved
// 0x10 : Data signal of ch_in
//        bit 31~0 - ch_in[31:0] (Read/Write)
// 0x14 : reserved
// 0x18 : Data signal of ch_out
//        bit 31~0 - ch_out[31:0] (Read/Write)
// 0x1c : reserved
// 0x20 : Data signal of fsize
//        bit 31~0 - fsize[31:0] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of stride
//        bit 31~0 - stride[31:0] (Read/Write)
// 0x2c : reserved
// 0x30 : Data signal of kernel
//        bit 31~0 - kernel[31:0] (Read/Write)
// 0x34 : reserved
// 0x38 : Data signal of act
//        bit 31~0 - act[31:0] (Read/Write)
// 0x3c : reserved
// (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

#define XCONV_CTRL_ADDR_AP_CTRL     0x00
#define XCONV_CTRL_ADDR_GIE         0x04
#define XCONV_CTRL_ADDR_IER         0x08
#define XCONV_CTRL_ADDR_ISR         0x0c
#define XCONV_CTRL_ADDR_CH_IN_DATA  0x10
#define XCONV_CTRL_BITS_CH_IN_DATA  32
#define XCONV_CTRL_ADDR_CH_OUT_DATA 0x18
#define XCONV_CTRL_BITS_CH_OUT_DATA 32
#define XCONV_CTRL_ADDR_FSIZE_DATA  0x20
#define XCONV_CTRL_BITS_FSIZE_DATA  32
#define XCONV_CTRL_ADDR_STRIDE_DATA 0x28
#define XCONV_CTRL_BITS_STRIDE_DATA 32
#define XCONV_CTRL_ADDR_KERNEL_DATA 0x30
#define XCONV_CTRL_BITS_KERNEL_DATA 32
#define XCONV_CTRL_ADDR_ACT_DATA    0x38
#define XCONV_CTRL_BITS_ACT_DATA    32

