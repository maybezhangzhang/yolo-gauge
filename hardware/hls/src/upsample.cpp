#include"upsample.h"

void upsample(data_t* in,data_t* out,int size){
//#pragma HLS INTERFACE m_axi depth=100 port=out offset=slave bundle=OUT num_write_outstanding=4
    // 128*size*size -> 128*(2*size)*(2*size)
	data_t tmp;
	for(unsigned short n=0;n<128;n++)
		for(unsigned short i=0;i<size;i++)
			for(unsigned short j=0;j<size;j++){
#pragma HLS PIPELINE
				int out_size=size*2;
				int wr_base_addr=n*out_size*out_size+i*2*out_size+2*j;
//#pragma HLS RESOURCE variable=wr_base_addr core=Mul_LUT
				int rd_base_addr=n*size*size+i*size;
//#pragma HLS RESOURCE variable=rd_base_addr core=Mul_LUT
				tmp=*(in+rd_base_addr+j);
				*(out+wr_base_addr)=tmp;
				*(out+wr_base_addr+1)=tmp;
				*(out+wr_base_addr+out_size)=tmp;
				*(out+wr_base_addr+out_size+1)=tmp;
			}
}
