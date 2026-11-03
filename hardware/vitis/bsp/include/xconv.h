// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2020.2 (64-bit)
// Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
// ==============================================================
#ifndef XCONV_H
#define XCONV_H

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#ifndef __linux__
#include "xil_types.h"
#include "xil_assert.h"
#include "xstatus.h"
#include "xil_io.h"
#else
#include <stdint.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stddef.h>
#endif
#include "xconv_hw.h"

/**************************** Type Definitions ******************************/
#ifdef __linux__
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
#else
typedef struct {
    u16 DeviceId;
    u32 Control_BaseAddress;
    u32 Ctrl_BaseAddress;
} XConv_Config;
#endif

typedef struct {
    u64 Control_BaseAddress;
    u64 Ctrl_BaseAddress;
    u32 IsReady;
} XConv;

typedef u32 word_type;

/***************** Macros (Inline Functions) Definitions *********************/
#ifndef __linux__
#define XConv_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))
#define XConv_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))
#else
#define XConv_WriteReg(BaseAddress, RegOffset, Data) \
    *(volatile u32*)((BaseAddress) + (RegOffset)) = (u32)(Data)
#define XConv_ReadReg(BaseAddress, RegOffset) \
    *(volatile u32*)((BaseAddress) + (RegOffset))

#define Xil_AssertVoid(expr)    assert(expr)
#define Xil_AssertNonvoid(expr) assert(expr)

#define XST_SUCCESS             0
#define XST_DEVICE_NOT_FOUND    2
#define XST_OPEN_DEVICE_FAILED  3
#define XIL_COMPONENT_IS_READY  1
#endif

/************************** Function Prototypes *****************************/
#ifndef __linux__
int XConv_Initialize(XConv *InstancePtr, u16 DeviceId);
XConv_Config* XConv_LookupConfig(u16 DeviceId);
int XConv_CfgInitialize(XConv *InstancePtr, XConv_Config *ConfigPtr);
#else
int XConv_Initialize(XConv *InstancePtr, const char* InstanceName);
int XConv_Release(XConv *InstancePtr);
#endif

void XConv_Start(XConv *InstancePtr);
u32 XConv_IsDone(XConv *InstancePtr);
u32 XConv_IsIdle(XConv *InstancePtr);
u32 XConv_IsReady(XConv *InstancePtr);
void XConv_EnableAutoRestart(XConv *InstancePtr);
void XConv_DisableAutoRestart(XConv *InstancePtr);

void XConv_Set_in1(XConv *InstancePtr, u64 Data);
u64 XConv_Get_in1(XConv *InstancePtr);
void XConv_Set_in2(XConv *InstancePtr, u64 Data);
u64 XConv_Get_in2(XConv *InstancePtr);
void XConv_Set_in3(XConv *InstancePtr, u64 Data);
u64 XConv_Get_in3(XConv *InstancePtr);
void XConv_Set_in4(XConv *InstancePtr, u64 Data);
u64 XConv_Get_in4(XConv *InstancePtr);
void XConv_Set_w1_r(XConv *InstancePtr, u64 Data);
u64 XConv_Get_w1_r(XConv *InstancePtr);
void XConv_Set_w2_r(XConv *InstancePtr, u64 Data);
u64 XConv_Get_w2_r(XConv *InstancePtr);
void XConv_Set_w3_r(XConv *InstancePtr, u64 Data);
u64 XConv_Get_w3_r(XConv *InstancePtr);
void XConv_Set_w4_r(XConv *InstancePtr, u64 Data);
u64 XConv_Get_w4_r(XConv *InstancePtr);
void XConv_Set_b(XConv *InstancePtr, u64 Data);
u64 XConv_Get_b(XConv *InstancePtr);
void XConv_Set_out1(XConv *InstancePtr, u64 Data);
u64 XConv_Get_out1(XConv *InstancePtr);
void XConv_Set_out2(XConv *InstancePtr, u64 Data);
u64 XConv_Get_out2(XConv *InstancePtr);
void XConv_Set_out3(XConv *InstancePtr, u64 Data);
u64 XConv_Get_out3(XConv *InstancePtr);
void XConv_Set_out4(XConv *InstancePtr, u64 Data);
u64 XConv_Get_out4(XConv *InstancePtr);
void XConv_Set_ch_in(XConv *InstancePtr, u32 Data);
u32 XConv_Get_ch_in(XConv *InstancePtr);
void XConv_Set_ch_out(XConv *InstancePtr, u32 Data);
u32 XConv_Get_ch_out(XConv *InstancePtr);
void XConv_Set_fsize(XConv *InstancePtr, u32 Data);
u32 XConv_Get_fsize(XConv *InstancePtr);
void XConv_Set_stride(XConv *InstancePtr, u32 Data);
u32 XConv_Get_stride(XConv *InstancePtr);
void XConv_Set_kernel(XConv *InstancePtr, u32 Data);
u32 XConv_Get_kernel(XConv *InstancePtr);
void XConv_Set_act(XConv *InstancePtr, u32 Data);
u32 XConv_Get_act(XConv *InstancePtr);

void XConv_InterruptGlobalEnable(XConv *InstancePtr);
void XConv_InterruptGlobalDisable(XConv *InstancePtr);
void XConv_InterruptEnable(XConv *InstancePtr, u32 Mask);
void XConv_InterruptDisable(XConv *InstancePtr, u32 Mask);
void XConv_InterruptClear(XConv *InstancePtr, u32 Mask);
u32 XConv_InterruptGetEnabled(XConv *InstancePtr);
u32 XConv_InterruptGetStatus(XConv *InstancePtr);

#ifdef __cplusplus
}
#endif

#endif
