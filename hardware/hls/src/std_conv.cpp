#include"conv.h"

data_t leaky_relu(data_t x){
	const data_t alpha=(data_t)0.1;
	if(x>=(data_t)0){
		return x;
	}
	else{
		return x*alpha;
	}
}

void load_input(data_t fm_in_buff[Tn][TRin][TCin],data_t* in1,data_t* in2,data_t* in3,data_t* in4,
unsigned short n,unsigned short fm_row,unsigned short fm_col,unsigned short fm_size,unsigned short stride){
// Row-hoisted addressing (v9): keep the row base constant inside a row so the load
// stream stays linear and burst friendly, and clamp only the column at the edges.
// Out-of-range pixels still read a safe in-buffer address and are then muxed to 0,
// because HLS hoists loads above the predicate and a negative offset would be
// zero-extended into a huge AXI address (the original clamp-to-0-at-tile-origin bug).
    unsigned short rr,cc;
    unsigned int size=(unsigned int)fm_size*(unsigned int)fm_size;
    unsigned short rows=(stride==1)?(unsigned short)(Tr+K-1):(unsigned short)TRin;
    unsigned short cols=(stride==1)?(unsigned short)(Tc+K-1):(unsigned short)TCin;
    int step=(stride==1)?1:2;
    int vc0=(int)fm_col*step-(int)P;
    for(rr=0;rr<rows;rr++){
        int vr=(int)fm_row*step-(int)P+(int)rr;
        bool row_in=(vr>=0)&&(vr<(int)fm_size);
        int row_off=row_in?(vr*(int)fm_size):0;
        int base=(int)n*(int)size+row_off;
        for(cc=0;cc<cols;cc++){
#pragma HLS PIPELINE II=1
            int vc=vc0+(int)cc;
            bool col_in=row_in&&(vc>=0)&&(vc<(int)fm_size);
            int off=base+(col_in?vc:0);
            data_t v1=*(in1+off);
            data_t v2=*(in2+off+(int)size);
            data_t v3=*(in3+off+2*(int)size);
            data_t v4=*(in4+off+3*(int)size);
            fm_in_buff[0][rr][cc]=col_in?v1:(data_t)0;
            fm_in_buff[1][rr][cc]=col_in?v2:(data_t)0;
            fm_in_buff[2][rr][cc]=col_in?v3:(data_t)0;
            fm_in_buff[3][rr][cc]=col_in?v4:(data_t)0;
        }
    }
}
void load_weight(data_t wt_buff[Tm][Tn][K][K],data_t* w1,data_t* w2,data_t* w3,data_t* w4,
                 unsigned short n,unsigned short m,unsigned short ch_in,unsigned short ch_out){
//load weight tile W[m:m+Tm][n:n+Tn][:][:]
    unsigned short mm,k;
    data_t tmp1,tmp2,tmp3,tmp4;
    int base_addr=m*ch_in*9+n*9;
    int ch_in_kk=ch_in*9;
    #pragma HLS RESOURCE variable=ch_in_kk core=Mul_LUT
    for(mm=0;mm<Tm;mm+=4){
        for(k=0;k<Tn*K*K;k++)             //k=nn*9+i*3+j
        {
#pragma HLS PIPELINE II=1
        	unsigned short nn=k/9;
        	unsigned short i=(k%9)/3;
        	unsigned short j=k%3;
        	//unsigned short offset=mm*ch_in*9;
        	tmp1=*(w1+base_addr+(mm+0)*ch_in_kk+k);
        	tmp2=*(w2+base_addr+(mm+1)*ch_in_kk+k);
        	tmp3=*(w3+base_addr+(mm+2)*ch_in_kk+k);
        	tmp4=*(w4+base_addr+(mm+3)*ch_in_kk+k);
            if((n+nn)<ch_in){
            	wt_buff[mm][nn][i][j]=tmp1;
            	wt_buff[mm+1][nn][i][j]=tmp2;
            	wt_buff[mm+2][nn][i][j]=tmp3;
            	wt_buff[mm+3][nn][i][j]=tmp4;
            }
            else{
            	wt_buff[mm][nn][i][j]=(data_t)0;
				wt_buff[mm+1][nn][i][j]=(data_t)0;
				wt_buff[mm+2][nn][i][j]=(data_t)0;
				wt_buff[mm+3][nn][i][j]=(data_t)0;
            }
        }
    }
}


void compute(data_t fm_in_buff[Tn][TRin][TCin],data_t fm_out_buff[Tm][Tr][Tc],data_t wt_buff[Tm][Tn][K][K],unsigned short stride){
#pragma HLS ARRAY_PARTITION variable=wt_buff complete dim=1
#pragma HLS ARRAY_PARTITION variable=wt_buff complete dim=2
#pragma HLS ARRAY_PARTITION variable=fm_out_buff complete dim=1
#pragma HLS ARRAY_PARTITION variable=fm_in_buff complete dim=1
    unsigned short kx,ky,rr,cc,nn,mm;
    for(kx=0;kx<K;kx++)
    	for(ky=0;ky<K;ky++)
    		for(rr=0;rr<Tr;rr++)
    			for(cc=0;cc<Tc;cc++){
				#pragma HLS PIPELINE II=1    
    				for(mm=0;mm<Tm;mm++){
    					for(nn=0;nn<Tn;nn++){
    						data_t mult=fm_in_buff[nn][rr*stride+kx][cc*stride+ky]*wt_buff[mm][nn][kx][ky];
    						data_t psum=mult+fm_out_buff[mm][rr][cc];
                            fm_out_buff[mm][rr][cc]=psum;
    					}
    				}
    			}
}

void load_bias(data_t fm_out_buff[Tm][Tr][Tc],data_t bias_buff[MAX_LEN/Tm][Tm],unsigned short m){
    int rr,cc,mm;
    for(rr=0;rr<Tr;rr++)
    	for(cc=0;cc<Tc;cc++){
			#pragma HLS PIPELINE II=1
    		for(mm=0;mm<Tm;mm++){
		 	    #pragma HLS UNROLL
                fm_out_buff[mm][rr][cc]=bias_buff[m/Tm][mm];
			}
		}
}

void store_output(data_t fm_out_buff[Tm][Tr][Tc],data_t* out1,data_t* out2,data_t* out3,data_t* out4,
unsigned short fm_row,unsigned short fm_col,unsigned short m,unsigned short fm_size,unsigned short ch_out,
unsigned short stride,unsigned short act){
//store fm_out_buff to Out[m:m+Tm][fm_row:fm_row+Tr][fm_col:fm_col+Tc]
unsigned short mm,rr,cc;
// Use int to avoid unsigned short overflow when fm_size*fm_size > 65535
int o_fm_size=(stride==1)?(int)fm_size:(int)(fm_size/2);
int o_size=o_fm_size*o_fm_size;
data_t tmp1,tmp2,tmp3,tmp4;
//
    for(mm=0;mm<Tm;mm+=4)
    	for(rr=0;rr<Tr;rr++)
    		for(cc=0;cc<Tc;cc++)
                {
#pragma HLS PIPELINE
    			    if(act==1){
    			    	tmp1=leaky_relu(fm_out_buff[mm][rr][cc]);
    			    	tmp2=leaky_relu(fm_out_buff[mm+1][rr][cc]);
    			    	tmp3=leaky_relu(fm_out_buff[mm+2][rr][cc]);
    			    	tmp4=leaky_relu(fm_out_buff[mm+3][rr][cc]);
    			    }
    			    else{
    			    	tmp1=fm_out_buff[mm][rr][cc];
    			    	tmp2=fm_out_buff[mm+1][rr][cc];
    			    	tmp3=fm_out_buff[mm+2][rr][cc];
    			    	tmp4=fm_out_buff[mm+3][rr][cc];
    			    }
    			    //o_fm_size%Tr==0
    			    int base_addr=((int)m+(int)mm)*o_size+((int)fm_row+rr)*o_fm_size+fm_col;
                    *(out1+base_addr+cc)=tmp1;
                    *(out2+base_addr+o_size+cc)=tmp2;
                    *(out3+base_addr+2*o_size+cc)=tmp3;
                    *(out4+base_addr+3*o_size+cc)=tmp4;
                }
}

void compute_output(data_t fm_out_buff[Tm][Tr][Tc],data_t bias_buff[MAX_LEN/Tm][Tm],data_t* in1,data_t* in2,
data_t* in3,data_t* in4,data_t* w1,data_t* w2,data_t* w3,data_t* w4,unsigned short m,unsigned short fm_size,
unsigned short ch_in,unsigned short ch_out,unsigned short fm_row,unsigned short fm_col,unsigned short stride){
	//pingpong buffer 
	data_t fm_in_buff1[Tn][TRin][TCin];
#pragma HLS ARRAY_PARTITION variable=fm_in_buff1 complete dim=1
	data_t fm_in_buff2[Tn][TRin][TCin];
#pragma HLS ARRAY_PARTITION variable=fm_in_buff2 complete dim=1
	data_t wt_buff1[Tm][Tn][K][K];
#pragma HLS ARRAY_PARTITION variable=wt_buff1 complete dim=2
#pragma HLS ARRAY_PARTITION variable=wt_buff1 complete dim=1
	data_t wt_buff2[Tm][Tn][K][K];
#pragma HLS ARRAY_PARTITION variable=wt_buff2 complete dim=2
#pragma HLS ARRAY_PARTITION variable=wt_buff2 complete dim=1
	unsigned short ti=0;
//compute Out[m:m+Tm][fm_row:fm_row+Tr][fm_col:fm_col+Tc]
	load_bias(fm_out_buff,bias_buff,m);
	load_input(fm_in_buff1,in1,in2,in3,in4,ti,fm_row,fm_col,fm_size,stride);   //ti=0
	load_weight(wt_buff1,w1,w2,w3,w4,ti,m,ch_in,ch_out);
	bool pingpong=true;
	for(ti=Tn;ti<ch_in;ti+=Tn){
		if(pingpong){
			load_input(fm_in_buff2,in1,in2,in3,in4,ti,fm_row,fm_col,fm_size,stride);
			load_weight(wt_buff2,w1,w2,w3,w4,ti,m,ch_in,ch_out);
			compute(fm_in_buff1,fm_out_buff,wt_buff1,stride);
			pingpong=false;
		}
		else{
			load_input(fm_in_buff1,in1,in2,in3,in4,ti,fm_row,fm_col,fm_size,stride);
			load_weight(wt_buff1,w1,w2,w3,w4,ti,m,ch_in,ch_out);
			compute(fm_in_buff2,fm_out_buff,wt_buff2,stride);
			pingpong=true;
		}
	}
	if(pingpong){
		compute(fm_in_buff1,fm_out_buff,wt_buff1,stride);
	}
	else{
		compute(fm_in_buff2,fm_out_buff,wt_buff2,stride);
	}
}

void next_block(unsigned short r,unsigned short c,unsigned short m,unsigned short &next_r,
unsigned short &next_c,unsigned short &next_m,unsigned short fm_size,unsigned short ch_out){
    if(c+Tc>=fm_size){
    	if(r+Tr>=fm_size){
    		next_m=m+Tm;
    		next_r=0;
    		next_c=0;
    	}
    	else{
    		next_m=m;
    		next_r=r+Tr;
    		next_c=0;
    	}
    }
    else{
    	next_m=m;
    	next_r=r;
    	next_c=c+Tc;
    }
}

void std_conv(data_t* in1,data_t* in2,data_t* in3,data_t* in4,data_t* w1,data_t* w2,data_t* w3,
		  data_t* w4,data_t* bias,data_t* out1,data_t* out2,data_t* out3,data_t* out4,unsigned short ch_in,
		  unsigned short ch_out,unsigned short fm_size,unsigned short stride,unsigned short act){
	//
//#pragma HLS INTERFACE m_axi depth=1000 port=in1 offset=slave bundle=IN1 num_read_outstanding=256
//#pragma HLS INTERFACE m_axi depth=1000 port=in2 offset=slave bundle=IN2 num_read_outstanding=256
//#pragma HLS INTERFACE m_axi depth=1000 port=in3 offset=slave bundle=IN3 num_read_outstanding=256
//#pragma HLS INTERFACE m_axi depth=1000 port=in4 offset=slave bundle=IN4 num_read_outstanding=256
//#pragma HLS INTERFACE m_axi depth=1000 port=w1 offset=slave bundle=W1 num_read_outstanding=256
//#pragma HLS INTERFACE m_axi depth=1000 port=w2 offset=slave bundle=W2 num_read_outstanding=256
//#pragma HLS INTERFACE m_axi depth=1000 port=w3 offset=slave bundle=W3 num_read_outstanding=256
//#pragma HLS INTERFACE m_axi depth=1000 port=w4 offset=slave bundle=W4 num_read_outstanding=256
//#pragma HLS INTERFACE m_axi depth=1000 port=bias offset=slave bundle=W1 num_read_outstanding=256
//#pragma HLS INTERFACE m_axi depth=1000 port=out1 offset=slave bundle=OUT1 num_write_outstanding=256
//#pragma HLS INTERFACE m_axi depth=1000 port=out2 offset=slave bundle=OUT2 num_write_outstanding=256
//#pragma HLS INTERFACE m_axi depth=1000 port=out3 offset=slave bundle=OUT3 num_write_outstanding=256
//#pragma HLS INTERFACE m_axi depth=1000 port=out4 offset=slave bundle=OUT4 num_write_outstanding=256
//#pragma HLS INTERFACE s_axilite port=return bundle=CTRL
//#pragma HLS INTERFACE s_axilite port=ch_in      bundle=CTRL
//#pragma HLS INTERFACE s_axilite port=ch_out      bundle=CTRL
//#pragma HLS INTERFACE s_axilite port=fm_size   bundle=CTRL
//#pragma HLS INTERFACE s_axilite port=stride   bundle=CTRL
//#pragma HLS INTERFACE s_axilite port=act   bundle=CTRL
//IN,W,B,OUT
    data_t bias_buff[MAX_LEN/Tm][Tm];
#pragma HLS ARRAY_PARTITION variable=bias_buff complete dim=2
    data_t fm_out1[Tm][Tr][Tc];
#pragma HLS ARRAY_PARTITION variable=fm_out1 complete dim=1
    data_t fm_out2[Tm][Tr][Tc];
#pragma HLS ARRAY_PARTITION variable=fm_out2 complete dim=1
    //pingpong buffer sign
    bool pingpong;
    unsigned short r,c,m;
    unsigned short next_r,next_c,next_m;
	unsigned short o_fm_size=(stride==1)?fm_size:fm_size/2;
    r=0;
    c=0;
    m=0;
    pingpong=true;
    memcpy((data_t*)bias_buff,(const data_t*)bias,sizeof(data_t)*ch_out);
    compute_output(fm_out1,bias_buff,in1,in2,in3,in4,w1,w2,w3,w4,m,fm_size,ch_in,ch_out,r,c,stride);
    next_block(r,c,m,next_r,next_c,next_m,o_fm_size,ch_out);
    while(true){
    	if(pingpong){   //store fm_out1 to DDR,compute next_block in fm_out2
    		compute_output(fm_out2,bias_buff,in1,in2,in3,in4,w1,w2,w3,w4,next_m,fm_size,ch_in,ch_out,next_r,next_c,stride);
    		store_output(fm_out1,out1,out2,out3,out4,r,c,m,fm_size,ch_out,stride,act);
    		pingpong=false;
    	}
    	else{           //store fm_out2 to DDR,compute next block in fm_out1
    		compute_output(fm_out1,bias_buff,in1,in2,in3,in4,w1,w2,w3,w4,next_m,fm_size,ch_in,ch_out,next_r,next_c,stride);
			store_output(fm_out2,out1,out2,out3,out4,r,c,m,fm_size,ch_out,stride,act);
			pingpong=true;
    	}
    	m=next_m;
    	r=next_r;
    	c=next_c;
    	next_block(r,c,m,next_r,next_c,next_m,o_fm_size,ch_out);
		if(next_m>=ch_out)
		    break;
    }
    //store last block to DDR
    if(pingpong){
    	store_output(fm_out1,out1,out2,out3,out4,r,c,m,fm_size,ch_out,stride,act);
    }
    else{
    	store_output(fm_out2,out1,out2,out3,out4,r,c,m,fm_size,ch_out,stride,act);
    }
}


