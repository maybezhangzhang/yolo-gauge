/*
 * sd_io.c
 *
 *  Created on: 2021-8-18
 *      Author: DELL
 */
#include "sd_io.h"

/* Boot/SD diagnostics readable over JTAG (no UART needed).
 * [0] f_mount rc        [1] last f_open rc     [2] 1 = size mismatch
 * [3] failed file count [4] last f_size value  [5] last expected size
 * [6] read_params calls [7] 1 = mount ok
 */
unsigned g_sd_status[8] = {0,0,0,0,0,0,0,0};

int SD_Init()
{
    FRESULT rc;

    rc = f_mount(&fatfs,"",0);
    g_sd_status[0] = (unsigned)rc;
    g_sd_status[7] = rc ? 0u : 1u;
    if(rc)
    {
        xil_printf("ERROR : f_mount returned %d\r\n",rc);
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

int SD_Transfer_read(char *FileName,u32 DestinationAddress,u32 ByteLength)
{
    FIL fil;
    FRESULT rc;
    UINT br;

    rc = f_open(&fil,FileName,FA_READ);
    if(rc)
    {
        g_sd_status[1] = (unsigned)rc;
        xil_printf("ERROR : f_open returned %d\r\n",rc);
        return XST_FAILURE;
    }
    if (f_size(&fil) != ByteLength) {
        g_sd_status[2] = 1u;
        g_sd_status[4] = (unsigned)f_size(&fil);
        g_sd_status[5] = (unsigned)ByteLength;
        f_close(&fil);
        return XST_FAILURE;
    }
    rc = f_lseek(&fil, 0);
    if(rc)
    {
        xil_printf("ERROR : f_lseek returned %d\r\n",rc);
        return XST_FAILURE;
    }
    rc = f_read(&fil, (void*)DestinationAddress,ByteLength,&br);
    if(rc || br != ByteLength)
    {
        f_close(&fil);
        xil_printf("ERROR : f_read returned %d\r\n",rc);
        return XST_FAILURE;
    }
    rc = f_close(&fil);
    if(rc)
    {
        xil_printf(" ERROR : f_close returned %d\r\n", rc);
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}

int SD_Transfer_write(char *FileName,u32 SourceAddress,u32 ByteLength)
{
    FIL fil;
    FRESULT rc;
    UINT bw;

    rc = f_open(&fil,FileName,FA_CREATE_ALWAYS | FA_WRITE);
    if(rc)
    {
        xil_printf("ERROR : f_open returned %d\r\n",rc);
        return XST_FAILURE;
    }
    rc = f_lseek(&fil, 0);
    if(rc)
    {
        xil_printf("ERROR : f_lseek returned %d\r\n",rc);
        return XST_FAILURE;
    }
    rc = f_write(&fil,(void*) SourceAddress,ByteLength,&bw);
    if(rc)
    {
        xil_printf("ERROR : f_write returned %d\r\n", rc);
        return XST_FAILURE;
    }
    rc = f_close(&fil);
    if(rc){
        xil_printf("ERROR : f_close returned %d\r\n",rc);
        return XST_FAILURE;
    }
    return XST_SUCCESS;
}


void read_params(string filename,data_t* x,int length){
    g_sd_status[6]++;
    float* tmp=new float[length];
    if(SD_Transfer_read((char*)(filename.c_str()),(u32)tmp,(u32)(sizeof(float)*length))!=XST_SUCCESS){
        /* Do not hang here: record the failure, zero the buffer and let the
         * caller decide. A halted core is impossible to diagnose remotely. */
        g_sd_status[3]++;
        xil_printf("FATAL: weight file not readable: %s\r\n", filename.c_str());
        xil_printf("check: 1) SD card dir name == code path  2) xilffs use_lfn=true.\r\n");
        for(int i=0;i<length;i++) x[i]=(data_t)0;
        delete [] tmp;
        return;
    }
    for(int i=0;i<length;i++){
        double q = round((double)tmp[i]*SCALE);
        if (!isfinite(q) || q < -32768 || q > 32767) {
            g_sd_status[3]++;
            xil_printf("FATAL: invalid fixed-point weight: %s\r\n", filename.c_str());
            for(int j=0;j<length;j++) x[j]=(data_t)0;
            delete [] tmp;
            return;
        }
        x[i]=(data_t)q;
    }
    delete [] tmp;
}

void write_data(string filename,data_t* x,int length){
	cout<<"writing "<<filename<<endl;
	float* tmp=new float[length];
	for(int i=0;i<length;i++){
		tmp[i]=(float)x[i]/(float)SCALE;
	}
	SD_Transfer_write((char*)filename.c_str(),(u32)tmp,(u32)(sizeof(float)*length));
	delete [] tmp;
}
