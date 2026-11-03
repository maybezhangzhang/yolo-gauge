#include"basic_op.h"
#include"xil_printf.h"
#include<string.h>

void conv_leakyrelu(int ch_in,int ch_out,int pad,int stride,int k,int h,int w,data_t* in,data_t *weight,data_t *bias,data_t *out,int act){
	int oh = h, ow = w;
	if(stride == 2){ oh = h/2; ow = w/2; }
	else if(stride == 0){
		if(k == 2){ oh = h/2; ow = w/2; }
		else if(k == 0){ oh = h*2; ow = w*2; }
	}
	Xil_DCacheFlushRange((u32)in, sizeof(data_t)*ch_in*h*w);
	Xil_DCacheFlushRange((u32)weight, sizeof(data_t)*ch_out*ch_in*k*k);
	Xil_DCacheFlushRange((u32)bias, sizeof(data_t)*ch_out);
	Xil_DCacheInvalidateRange((u32)out, sizeof(data_t)*ch_out*oh*ow);

	XConv_Set_in1(&conv_inst, (u32)in);
	XConv_Set_in2(&conv_inst, (u32)in);
	XConv_Set_in3(&conv_inst, (u32)in);
	XConv_Set_in4(&conv_inst, (u32)in);

	XConv_Set_w1_r(&conv_inst, (u32)weight);
	XConv_Set_w2_r(&conv_inst, (u32)weight);
	XConv_Set_w3_r(&conv_inst, (u32)weight);
	XConv_Set_w4_r(&conv_inst, (u32)weight);

	XConv_Set_b(&conv_inst, (u32)bias);
	XConv_Set_out1(&conv_inst, (u32)out);
	XConv_Set_out2(&conv_inst, (u32)out);
	XConv_Set_out3(&conv_inst, (u32)out);
	XConv_Set_out4(&conv_inst, (u32)out);

	XConv_Set_ch_in(&conv_inst,ch_in);
	XConv_Set_ch_out(&conv_inst,ch_out);
	XConv_Set_fsize(&conv_inst,h);
	XConv_Set_stride(&conv_inst,stride);
	XConv_Set_kernel(&conv_inst,k);
	XConv_Set_act(&conv_inst,act);

	/* Programming is repeated on retry: a start that the block ignored is
	 * usually accompanied by a lost write, so the config is re-latched. */
	auto program = [&]() {
		XConv_Set_in1(&conv_inst, (u32)in);
		XConv_Set_in2(&conv_inst, (u32)in);
		XConv_Set_in3(&conv_inst, (u32)in);
		XConv_Set_in4(&conv_inst, (u32)in);

		XConv_Set_w1_r(&conv_inst, (u32)weight);
		XConv_Set_w2_r(&conv_inst, (u32)weight);
		XConv_Set_w3_r(&conv_inst, (u32)weight);
		XConv_Set_w4_r(&conv_inst, (u32)weight);

		XConv_Set_b(&conv_inst, (u32)bias);
		XConv_Set_out1(&conv_inst, (u32)out);
		XConv_Set_out2(&conv_inst, (u32)out);
		XConv_Set_out3(&conv_inst, (u32)out);
		XConv_Set_out4(&conv_inst, (u32)out);

		XConv_Set_ch_in(&conv_inst,ch_in);
		XConv_Set_ch_out(&conv_inst,ch_out);
		XConv_Set_fsize(&conv_inst,h);
		XConv_Set_stride(&conv_inst,stride);
		XConv_Set_kernel(&conv_inst,k);
		XConv_Set_act(&conv_inst,act);
	};

	program();
	XConv_Start(&conv_inst);
	{
		/* The block only accepts a new ap_start once it is back in the idle
		 * state, and ap_done is raised one cycle before ap_idle.  Starting the
		 * next convolution straight after ap_done therefore races: the start
		 * is silently dropped and this loop would wait for a done that never
		 * comes.  Wait for ap_idle, and re-program if the block is idle but
		 * never left the starting blocks. */
		volatile u32 to = 0;
		u32 restarts = 0;
		for (;;) {
			u32 ctrl = XConv_ReadReg(conv_inst.Ctrl_BaseAddress,
			                         XCONV_CTRL_ADDR_AP_CTRL);
			if (ctrl & 0x02u) {                      /* ap_done */
				while ((XConv_ReadReg(conv_inst.Ctrl_BaseAddress,
				                      XCONV_CTRL_ADDR_AP_CTRL) & 0x04u) == 0u) {
					if (++to > 100000u) break;       /* idle never came, go on */
				}
				break;
			}
			if ((ctrl & 0x04u) && restarts < 8u) {   /* idle, start was lost */
				++restarts;
				program();
				XConv_Start(&conv_inst);
				to = 0;
				continue;
			}
			if (++to > 10000000u) {
				xil_printf("CONV TIMEOUT: ch_in=%d ch_out=%d k=%d h=%d stride=%d act=%d\r\n",
				           ch_in,ch_out,k,h,stride,act);
				break;
			}
		}
	}
	Xil_DCacheInvalidateRange((u32)out, sizeof(data_t)*ch_out*oh*ow);

	if (k == 3 && ow > 10) {
		data_t tmp[10];
		for (int c = 0; c < ch_out; c++) {
			data_t* plane = out + (u32)c * oh * ow;
			for (int r = 0; r < oh; r++) {
				data_t* row = plane + (u32)r * ow;
				memcpy(tmp, row, sizeof(tmp));
				memmove(row, row + 10, sizeof(data_t) * (ow - 10));
				memcpy(row + ow - 10, tmp, sizeof(tmp));
			}
		}
		Xil_DCacheFlushRange((u32)out, sizeof(data_t)*ch_out*oh*ow);
	}
}

void conv_init(){
	XConv_Initialize(&conv_inst, 0);
}

int conv_probe(u32 v){
	XConv_Set_fsize(&conv_inst,v);
	return (int)XConv_Get_fsize(&conv_inst);
}

void sampling(data_t* in,data_t* out,int ch,int fsize,int mode){
	if(mode==1)
	    conv_leakyrelu(ch,ch,0,0,2,fsize,fsize,in,in,in,out,0);
	else
		conv_leakyrelu(ch,ch,0,0,0,fsize,fsize,in,in,in,out,0);
}

