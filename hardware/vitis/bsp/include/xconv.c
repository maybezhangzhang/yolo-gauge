// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2020.2 (64-bit)
// Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
// ==============================================================
/***************************** Include Files *********************************/
#include "xconv.h"

/************************** Function Implementation *************************/
#ifndef __linux__
int XConv_CfgInitialize(XConv *InstancePtr, XConv_Config *ConfigPtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(ConfigPtr != NULL);

    InstancePtr->Control_BaseAddress = ConfigPtr->Control_BaseAddress;
    InstancePtr->Ctrl_BaseAddress = ConfigPtr->Ctrl_BaseAddress;
    InstancePtr->IsReady = XIL_COMPONENT_IS_READY;

    return XST_SUCCESS;
}
#endif

void XConv_Start(XConv *InstancePtr) {
    u32 Data;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_AP_CTRL) & 0x80;
    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_AP_CTRL, Data | 0x01);
}

u32 XConv_IsDone(XConv *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_AP_CTRL);
    return (Data >> 1) & 0x1;
}

u32 XConv_IsIdle(XConv *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_AP_CTRL);
    return (Data >> 2) & 0x1;
}

u32 XConv_IsReady(XConv *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_AP_CTRL);
    // check ap_start to see if the pcore is ready for next input
    return !(Data & 0x1);
}

void XConv_EnableAutoRestart(XConv *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_AP_CTRL, 0x80);
}

void XConv_DisableAutoRestart(XConv *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_AP_CTRL, 0);
}

void XConv_Set_in1(XConv *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN1_DATA, (u32)(Data));
    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN1_DATA + 4, (u32)(Data >> 32));
}

u64 XConv_Get_in1(XConv *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN1_DATA);
    Data += (u64)XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN1_DATA + 4) << 32;
    return Data;
}

void XConv_Set_in2(XConv *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN2_DATA, (u32)(Data));
    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN2_DATA + 4, (u32)(Data >> 32));
}

u64 XConv_Get_in2(XConv *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN2_DATA);
    Data += (u64)XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN2_DATA + 4) << 32;
    return Data;
}

void XConv_Set_in3(XConv *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN3_DATA, (u32)(Data));
    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN3_DATA + 4, (u32)(Data >> 32));
}

u64 XConv_Get_in3(XConv *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN3_DATA);
    Data += (u64)XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN3_DATA + 4) << 32;
    return Data;
}

void XConv_Set_in4(XConv *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN4_DATA, (u32)(Data));
    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN4_DATA + 4, (u32)(Data >> 32));
}

u64 XConv_Get_in4(XConv *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN4_DATA);
    Data += (u64)XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_IN4_DATA + 4) << 32;
    return Data;
}

void XConv_Set_w1_r(XConv *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W1_R_DATA, (u32)(Data));
    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W1_R_DATA + 4, (u32)(Data >> 32));
}

u64 XConv_Get_w1_r(XConv *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W1_R_DATA);
    Data += (u64)XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W1_R_DATA + 4) << 32;
    return Data;
}

void XConv_Set_w2_r(XConv *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W2_R_DATA, (u32)(Data));
    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W2_R_DATA + 4, (u32)(Data >> 32));
}

u64 XConv_Get_w2_r(XConv *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W2_R_DATA);
    Data += (u64)XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W2_R_DATA + 4) << 32;
    return Data;
}

void XConv_Set_w3_r(XConv *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W3_R_DATA, (u32)(Data));
    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W3_R_DATA + 4, (u32)(Data >> 32));
}

u64 XConv_Get_w3_r(XConv *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W3_R_DATA);
    Data += (u64)XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W3_R_DATA + 4) << 32;
    return Data;
}

void XConv_Set_w4_r(XConv *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W4_R_DATA, (u32)(Data));
    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W4_R_DATA + 4, (u32)(Data >> 32));
}

u64 XConv_Get_w4_r(XConv *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W4_R_DATA);
    Data += (u64)XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_W4_R_DATA + 4) << 32;
    return Data;
}

void XConv_Set_b(XConv *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_B_DATA, (u32)(Data));
    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_B_DATA + 4, (u32)(Data >> 32));
}

u64 XConv_Get_b(XConv *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_B_DATA);
    Data += (u64)XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_B_DATA + 4) << 32;
    return Data;
}

void XConv_Set_out1(XConv *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT1_DATA, (u32)(Data));
    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT1_DATA + 4, (u32)(Data >> 32));
}

u64 XConv_Get_out1(XConv *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT1_DATA);
    Data += (u64)XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT1_DATA + 4) << 32;
    return Data;
}

void XConv_Set_out2(XConv *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT2_DATA, (u32)(Data));
    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT2_DATA + 4, (u32)(Data >> 32));
}

u64 XConv_Get_out2(XConv *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT2_DATA);
    Data += (u64)XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT2_DATA + 4) << 32;
    return Data;
}

void XConv_Set_out3(XConv *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT3_DATA, (u32)(Data));
    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT3_DATA + 4, (u32)(Data >> 32));
}

u64 XConv_Get_out3(XConv *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT3_DATA);
    Data += (u64)XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT3_DATA + 4) << 32;
    return Data;
}

void XConv_Set_out4(XConv *InstancePtr, u64 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT4_DATA, (u32)(Data));
    XConv_WriteReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT4_DATA + 4, (u32)(Data >> 32));
}

u64 XConv_Get_out4(XConv *InstancePtr) {
    u64 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT4_DATA);
    Data += (u64)XConv_ReadReg(InstancePtr->Control_BaseAddress, XCONV_CONTROL_ADDR_OUT4_DATA + 4) << 32;
    return Data;
}

void XConv_Set_ch_in(XConv *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_CH_IN_DATA, Data);
}

u32 XConv_Get_ch_in(XConv *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_CH_IN_DATA);
    return Data;
}

void XConv_Set_ch_out(XConv *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_CH_OUT_DATA, Data);
}

u32 XConv_Get_ch_out(XConv *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_CH_OUT_DATA);
    return Data;
}

void XConv_Set_fsize(XConv *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_FSIZE_DATA, Data);
}

u32 XConv_Get_fsize(XConv *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_FSIZE_DATA);
    return Data;
}

void XConv_Set_stride(XConv *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_STRIDE_DATA, Data);
}

u32 XConv_Get_stride(XConv *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_STRIDE_DATA);
    return Data;
}

void XConv_Set_kernel(XConv *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_KERNEL_DATA, Data);
}

u32 XConv_Get_kernel(XConv *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_KERNEL_DATA);
    return Data;
}

void XConv_Set_act(XConv *InstancePtr, u32 Data) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_ACT_DATA, Data);
}

u32 XConv_Get_act(XConv *InstancePtr) {
    u32 Data;

    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Data = XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_ACT_DATA);
    return Data;
}

void XConv_InterruptGlobalEnable(XConv *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_GIE, 1);
}

void XConv_InterruptGlobalDisable(XConv *InstancePtr) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_GIE, 0);
}

void XConv_InterruptEnable(XConv *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_IER);
    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_IER, Register | Mask);
}

void XConv_InterruptDisable(XConv *InstancePtr, u32 Mask) {
    u32 Register;

    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    Register =  XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_IER);
    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_IER, Register & (~Mask));
}

void XConv_InterruptClear(XConv *InstancePtr, u32 Mask) {
    Xil_AssertVoid(InstancePtr != NULL);
    Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    XConv_WriteReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_ISR, Mask);
}

u32 XConv_InterruptGetEnabled(XConv *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_IER);
}

u32 XConv_InterruptGetStatus(XConv *InstancePtr) {
    Xil_AssertNonvoid(InstancePtr != NULL);
    Xil_AssertNonvoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

    return XConv_ReadReg(InstancePtr->Ctrl_BaseAddress, XCONV_CTRL_ADDR_ISR);
}

