// ==============================================================
// Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2020.2 (64-bit)
// Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
// ==============================================================
`timescale 1ns/1ps
module conv_control_s_axi
#(parameter
    C_S_AXI_ADDR_WIDTH = 8,
    C_S_AXI_DATA_WIDTH = 32
)(
    input  wire                          ACLK,
    input  wire                          ARESET,
    input  wire                          ACLK_EN,
    input  wire [C_S_AXI_ADDR_WIDTH-1:0] AWADDR,
    input  wire                          AWVALID,
    output wire                          AWREADY,
    input  wire [C_S_AXI_DATA_WIDTH-1:0] WDATA,
    input  wire [C_S_AXI_DATA_WIDTH/8-1:0] WSTRB,
    input  wire                          WVALID,
    output wire                          WREADY,
    output wire [1:0]                    BRESP,
    output wire                          BVALID,
    input  wire                          BREADY,
    input  wire [C_S_AXI_ADDR_WIDTH-1:0] ARADDR,
    input  wire                          ARVALID,
    output wire                          ARREADY,
    output wire [C_S_AXI_DATA_WIDTH-1:0] RDATA,
    output wire [1:0]                    RRESP,
    output wire                          RVALID,
    input  wire                          RREADY,
    output wire [63:0]                   in1,
    output wire [63:0]                   in2,
    output wire [63:0]                   in3,
    output wire [63:0]                   in4,
    output wire [63:0]                   w1_r,
    output wire [63:0]                   w2_r,
    output wire [63:0]                   w3_r,
    output wire [63:0]                   w4_r,
    output wire [63:0]                   b,
    output wire [63:0]                   out1,
    output wire [63:0]                   out2,
    output wire [63:0]                   out3,
    output wire [63:0]                   out4
);
//------------------------Address Info-------------------
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

//------------------------Parameter----------------------
localparam
    ADDR_IN1_DATA_0  = 8'h10,
    ADDR_IN1_DATA_1  = 8'h14,
    ADDR_IN1_CTRL    = 8'h18,
    ADDR_IN2_DATA_0  = 8'h1c,
    ADDR_IN2_DATA_1  = 8'h20,
    ADDR_IN2_CTRL    = 8'h24,
    ADDR_IN3_DATA_0  = 8'h28,
    ADDR_IN3_DATA_1  = 8'h2c,
    ADDR_IN3_CTRL    = 8'h30,
    ADDR_IN4_DATA_0  = 8'h34,
    ADDR_IN4_DATA_1  = 8'h38,
    ADDR_IN4_CTRL    = 8'h3c,
    ADDR_W1_R_DATA_0 = 8'h40,
    ADDR_W1_R_DATA_1 = 8'h44,
    ADDR_W1_R_CTRL   = 8'h48,
    ADDR_W2_R_DATA_0 = 8'h4c,
    ADDR_W2_R_DATA_1 = 8'h50,
    ADDR_W2_R_CTRL   = 8'h54,
    ADDR_W3_R_DATA_0 = 8'h58,
    ADDR_W3_R_DATA_1 = 8'h5c,
    ADDR_W3_R_CTRL   = 8'h60,
    ADDR_W4_R_DATA_0 = 8'h64,
    ADDR_W4_R_DATA_1 = 8'h68,
    ADDR_W4_R_CTRL   = 8'h6c,
    ADDR_B_DATA_0    = 8'h70,
    ADDR_B_DATA_1    = 8'h74,
    ADDR_B_CTRL      = 8'h78,
    ADDR_OUT1_DATA_0 = 8'h7c,
    ADDR_OUT1_DATA_1 = 8'h80,
    ADDR_OUT1_CTRL   = 8'h84,
    ADDR_OUT2_DATA_0 = 8'h88,
    ADDR_OUT2_DATA_1 = 8'h8c,
    ADDR_OUT2_CTRL   = 8'h90,
    ADDR_OUT3_DATA_0 = 8'h94,
    ADDR_OUT3_DATA_1 = 8'h98,
    ADDR_OUT3_CTRL   = 8'h9c,
    ADDR_OUT4_DATA_0 = 8'ha0,
    ADDR_OUT4_DATA_1 = 8'ha4,
    ADDR_OUT4_CTRL   = 8'ha8,
    WRIDLE           = 2'd0,
    WRDATA           = 2'd1,
    WRRESP           = 2'd2,
    WRRESET          = 2'd3,
    RDIDLE           = 2'd0,
    RDDATA           = 2'd1,
    RDRESET          = 2'd2,
    ADDR_BITS                = 8;

//------------------------Local signal-------------------
    reg  [1:0]                    wstate = WRRESET;
    reg  [1:0]                    wnext;
    reg  [ADDR_BITS-1:0]          waddr;
    wire [C_S_AXI_DATA_WIDTH-1:0] wmask;
    wire                          aw_hs;
    wire                          w_hs;
    reg  [1:0]                    rstate = RDRESET;
    reg  [1:0]                    rnext;
    reg  [C_S_AXI_DATA_WIDTH-1:0] rdata;
    wire                          ar_hs;
    wire [ADDR_BITS-1:0]          raddr;
    // internal registers
    reg  [63:0]                   int_in1 = 'b0;
    reg  [63:0]                   int_in2 = 'b0;
    reg  [63:0]                   int_in3 = 'b0;
    reg  [63:0]                   int_in4 = 'b0;
    reg  [63:0]                   int_w1_r = 'b0;
    reg  [63:0]                   int_w2_r = 'b0;
    reg  [63:0]                   int_w3_r = 'b0;
    reg  [63:0]                   int_w4_r = 'b0;
    reg  [63:0]                   int_b = 'b0;
    reg  [63:0]                   int_out1 = 'b0;
    reg  [63:0]                   int_out2 = 'b0;
    reg  [63:0]                   int_out3 = 'b0;
    reg  [63:0]                   int_out4 = 'b0;

//------------------------Instantiation------------------


//------------------------AXI write fsm------------------
assign AWREADY = (wstate == WRIDLE);
assign WREADY  = (wstate == WRDATA);
assign BRESP   = 2'b00;  // OKAY
assign BVALID  = (wstate == WRRESP);
assign wmask   = { {8{WSTRB[3]}}, {8{WSTRB[2]}}, {8{WSTRB[1]}}, {8{WSTRB[0]}} };
assign aw_hs   = AWVALID & AWREADY;
assign w_hs    = WVALID & WREADY;

// wstate
always @(posedge ACLK) begin
    if (ARESET)
        wstate <= WRRESET;
    else if (ACLK_EN)
        wstate <= wnext;
end

// wnext
always @(*) begin
    case (wstate)
        WRIDLE:
            if (AWVALID)
                wnext = WRDATA;
            else
                wnext = WRIDLE;
        WRDATA:
            if (WVALID)
                wnext = WRRESP;
            else
                wnext = WRDATA;
        WRRESP:
            if (BREADY)
                wnext = WRIDLE;
            else
                wnext = WRRESP;
        default:
            wnext = WRIDLE;
    endcase
end

// waddr
always @(posedge ACLK) begin
    if (ACLK_EN) begin
        if (aw_hs)
            waddr <= AWADDR[ADDR_BITS-1:0];
    end
end

//------------------------AXI read fsm-------------------
assign ARREADY = (rstate == RDIDLE);
assign RDATA   = rdata;
assign RRESP   = 2'b00;  // OKAY
assign RVALID  = (rstate == RDDATA);
assign ar_hs   = ARVALID & ARREADY;
assign raddr   = ARADDR[ADDR_BITS-1:0];

// rstate
always @(posedge ACLK) begin
    if (ARESET)
        rstate <= RDRESET;
    else if (ACLK_EN)
        rstate <= rnext;
end

// rnext
always @(*) begin
    case (rstate)
        RDIDLE:
            if (ARVALID)
                rnext = RDDATA;
            else
                rnext = RDIDLE;
        RDDATA:
            if (RREADY & RVALID)
                rnext = RDIDLE;
            else
                rnext = RDDATA;
        default:
            rnext = RDIDLE;
    endcase
end

// rdata
always @(posedge ACLK) begin
    if (ACLK_EN) begin
        if (ar_hs) begin
            rdata <= 'b0;
            case (raddr)
                ADDR_IN1_DATA_0: begin
                    rdata <= int_in1[31:0];
                end
                ADDR_IN1_DATA_1: begin
                    rdata <= int_in1[63:32];
                end
                ADDR_IN2_DATA_0: begin
                    rdata <= int_in2[31:0];
                end
                ADDR_IN2_DATA_1: begin
                    rdata <= int_in2[63:32];
                end
                ADDR_IN3_DATA_0: begin
                    rdata <= int_in3[31:0];
                end
                ADDR_IN3_DATA_1: begin
                    rdata <= int_in3[63:32];
                end
                ADDR_IN4_DATA_0: begin
                    rdata <= int_in4[31:0];
                end
                ADDR_IN4_DATA_1: begin
                    rdata <= int_in4[63:32];
                end
                ADDR_W1_R_DATA_0: begin
                    rdata <= int_w1_r[31:0];
                end
                ADDR_W1_R_DATA_1: begin
                    rdata <= int_w1_r[63:32];
                end
                ADDR_W2_R_DATA_0: begin
                    rdata <= int_w2_r[31:0];
                end
                ADDR_W2_R_DATA_1: begin
                    rdata <= int_w2_r[63:32];
                end
                ADDR_W3_R_DATA_0: begin
                    rdata <= int_w3_r[31:0];
                end
                ADDR_W3_R_DATA_1: begin
                    rdata <= int_w3_r[63:32];
                end
                ADDR_W4_R_DATA_0: begin
                    rdata <= int_w4_r[31:0];
                end
                ADDR_W4_R_DATA_1: begin
                    rdata <= int_w4_r[63:32];
                end
                ADDR_B_DATA_0: begin
                    rdata <= int_b[31:0];
                end
                ADDR_B_DATA_1: begin
                    rdata <= int_b[63:32];
                end
                ADDR_OUT1_DATA_0: begin
                    rdata <= int_out1[31:0];
                end
                ADDR_OUT1_DATA_1: begin
                    rdata <= int_out1[63:32];
                end
                ADDR_OUT2_DATA_0: begin
                    rdata <= int_out2[31:0];
                end
                ADDR_OUT2_DATA_1: begin
                    rdata <= int_out2[63:32];
                end
                ADDR_OUT3_DATA_0: begin
                    rdata <= int_out3[31:0];
                end
                ADDR_OUT3_DATA_1: begin
                    rdata <= int_out3[63:32];
                end
                ADDR_OUT4_DATA_0: begin
                    rdata <= int_out4[31:0];
                end
                ADDR_OUT4_DATA_1: begin
                    rdata <= int_out4[63:32];
                end
            endcase
        end
    end
end


//------------------------Register logic-----------------
assign in1  = int_in1;
assign in2  = int_in2;
assign in3  = int_in3;
assign in4  = int_in4;
assign w1_r = int_w1_r;
assign w2_r = int_w2_r;
assign w3_r = int_w3_r;
assign w4_r = int_w4_r;
assign b    = int_b;
assign out1 = int_out1;
assign out2 = int_out2;
assign out3 = int_out3;
assign out4 = int_out4;
// int_in1[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_in1[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_IN1_DATA_0)
            int_in1[31:0] <= (WDATA[31:0] & wmask) | (int_in1[31:0] & ~wmask);
    end
end

// int_in1[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_in1[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_IN1_DATA_1)
            int_in1[63:32] <= (WDATA[31:0] & wmask) | (int_in1[63:32] & ~wmask);
    end
end

// int_in2[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_in2[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_IN2_DATA_0)
            int_in2[31:0] <= (WDATA[31:0] & wmask) | (int_in2[31:0] & ~wmask);
    end
end

// int_in2[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_in2[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_IN2_DATA_1)
            int_in2[63:32] <= (WDATA[31:0] & wmask) | (int_in2[63:32] & ~wmask);
    end
end

// int_in3[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_in3[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_IN3_DATA_0)
            int_in3[31:0] <= (WDATA[31:0] & wmask) | (int_in3[31:0] & ~wmask);
    end
end

// int_in3[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_in3[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_IN3_DATA_1)
            int_in3[63:32] <= (WDATA[31:0] & wmask) | (int_in3[63:32] & ~wmask);
    end
end

// int_in4[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_in4[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_IN4_DATA_0)
            int_in4[31:0] <= (WDATA[31:0] & wmask) | (int_in4[31:0] & ~wmask);
    end
end

// int_in4[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_in4[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_IN4_DATA_1)
            int_in4[63:32] <= (WDATA[31:0] & wmask) | (int_in4[63:32] & ~wmask);
    end
end

// int_w1_r[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_w1_r[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_W1_R_DATA_0)
            int_w1_r[31:0] <= (WDATA[31:0] & wmask) | (int_w1_r[31:0] & ~wmask);
    end
end

// int_w1_r[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_w1_r[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_W1_R_DATA_1)
            int_w1_r[63:32] <= (WDATA[31:0] & wmask) | (int_w1_r[63:32] & ~wmask);
    end
end

// int_w2_r[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_w2_r[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_W2_R_DATA_0)
            int_w2_r[31:0] <= (WDATA[31:0] & wmask) | (int_w2_r[31:0] & ~wmask);
    end
end

// int_w2_r[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_w2_r[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_W2_R_DATA_1)
            int_w2_r[63:32] <= (WDATA[31:0] & wmask) | (int_w2_r[63:32] & ~wmask);
    end
end

// int_w3_r[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_w3_r[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_W3_R_DATA_0)
            int_w3_r[31:0] <= (WDATA[31:0] & wmask) | (int_w3_r[31:0] & ~wmask);
    end
end

// int_w3_r[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_w3_r[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_W3_R_DATA_1)
            int_w3_r[63:32] <= (WDATA[31:0] & wmask) | (int_w3_r[63:32] & ~wmask);
    end
end

// int_w4_r[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_w4_r[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_W4_R_DATA_0)
            int_w4_r[31:0] <= (WDATA[31:0] & wmask) | (int_w4_r[31:0] & ~wmask);
    end
end

// int_w4_r[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_w4_r[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_W4_R_DATA_1)
            int_w4_r[63:32] <= (WDATA[31:0] & wmask) | (int_w4_r[63:32] & ~wmask);
    end
end

// int_b[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_b[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_B_DATA_0)
            int_b[31:0] <= (WDATA[31:0] & wmask) | (int_b[31:0] & ~wmask);
    end
end

// int_b[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_b[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_B_DATA_1)
            int_b[63:32] <= (WDATA[31:0] & wmask) | (int_b[63:32] & ~wmask);
    end
end

// int_out1[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_out1[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_OUT1_DATA_0)
            int_out1[31:0] <= (WDATA[31:0] & wmask) | (int_out1[31:0] & ~wmask);
    end
end

// int_out1[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_out1[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_OUT1_DATA_1)
            int_out1[63:32] <= (WDATA[31:0] & wmask) | (int_out1[63:32] & ~wmask);
    end
end

// int_out2[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_out2[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_OUT2_DATA_0)
            int_out2[31:0] <= (WDATA[31:0] & wmask) | (int_out2[31:0] & ~wmask);
    end
end

// int_out2[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_out2[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_OUT2_DATA_1)
            int_out2[63:32] <= (WDATA[31:0] & wmask) | (int_out2[63:32] & ~wmask);
    end
end

// int_out3[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_out3[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_OUT3_DATA_0)
            int_out3[31:0] <= (WDATA[31:0] & wmask) | (int_out3[31:0] & ~wmask);
    end
end

// int_out3[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_out3[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_OUT3_DATA_1)
            int_out3[63:32] <= (WDATA[31:0] & wmask) | (int_out3[63:32] & ~wmask);
    end
end

// int_out4[31:0]
always @(posedge ACLK) begin
    if (ARESET)
        int_out4[31:0] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_OUT4_DATA_0)
            int_out4[31:0] <= (WDATA[31:0] & wmask) | (int_out4[31:0] & ~wmask);
    end
end

// int_out4[63:32]
always @(posedge ACLK) begin
    if (ARESET)
        int_out4[63:32] <= 0;
    else if (ACLK_EN) begin
        if (w_hs && waddr == ADDR_OUT4_DATA_1)
            int_out4[63:32] <= (WDATA[31:0] & wmask) | (int_out4[63:32] & ~wmask);
    end
end


//------------------------Memory logic-------------------

endmodule
