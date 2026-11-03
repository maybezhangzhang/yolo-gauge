/*
 * sd_io.h
 */
#ifndef SD_IO_H
#define SD_IO_H
#include "ff.h"
#include "xparameters.h"
#include "xdevcfg.h"
#include<cstdlib>
#include<cstdio>
#include<string>
#include<cmath>
#include<iostream>

#define SCALE 512
typedef short data_t;
using namespace std;

static FATFS fatfs;

/* SD / weight-load diagnostics (see sd_io.cpp for the index map). */
extern unsigned g_sd_status[8];

int SD_Init();

int SD_Transfer_read(char *FileName,u32 DestinationAddress,u32 ByteLength);

int SD_Transfer_write(char *FileName,u32 SourceAddress,u32 ByteLength);

void write_data(string filename,data_t* x,int length);

void read_params(string filename,data_t* x,int length);

#endif
