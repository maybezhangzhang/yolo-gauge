`timescale 1ns/1ps
// ---------------------------------------------------------------------------
// tb_conv_pwconv.v
//   DUT  : conv (Vitis HLS 2020.2 生成的 1x1/3x3/maxpool/upsample 统一加速器)
//   用例 : kernel=1 (1x1 pwconv)
//          ch_in=4, ch_out=8, fsize=12, stride=1, act=0
//          输入特征图全 1.0(0x0200)，权重全 1.0，bias 全 0
//          期望输出：每个通道每个像素 = 4.0 (0x0800)
//   激励 : 8 个 AXI4 从机 RAM 挂在 FM1~FM4 / W1~W4 上，
//          通过 AXI-Lite(CTRL) 配参数、启动，再轮询 AP_CTRL[1] ap_done。
//   说明 : data_t = ap_fixed<16,7>，1.0 = 0x0200，4.0 = 0x0800；
//          AXI 总线 32bit，2 个 16bit 元素打包在 1 个字里。
// ---------------------------------------------------------------------------
module tb_conv_pwconv;

    // ---------------- 测试配置 ----------------
    localparam integer CH_IN  = 4;
    localparam integer CH_OUT = 8;
    localparam integer FSIZE  = 12;
    localparam integer STRIDE = 1;
    localparam integer KERNEL = 1;
    localparam integer ACT    = 0;

    localparam integer PIX       = FSIZE * FSIZE;  // 144 元素/通道
    localparam integer PIX_WORDS = PIX / 2;        // 72 字/通道
    localparam integer OUT_WORD  = 1024;           // out1 = 0x1000 -> 字地址 1024

    localparam [63:0] IN_BASE  = 64'h0000_0000;
    localparam [63:0] W1_BASE  = 64'h0000_0000;
    localparam [63:0] B_BASE   = 64'h0000_0100;
    localparam [63:0] OUT_BASE = 64'h0000_1000;

    localparam [15:0] ONE_I16  = 16'h0200;
    localparam [15:0] FOUR_I16 = 16'h0800;

    // ---------------- 时钟 / 复位 ----------------
    reg ap_clk;
    reg ap_rst_n;

    initial begin
        ap_clk = 1'b0;
        forever #2.75 ap_clk = ~ap_clk;   // 5.5ns 周期，与 HLS 目标时钟一致
    end

    // 波形/事务记录见文件末尾的 events.log 逐拍采样（xsim 下比 VCD 更轻量）

    // ---------------- AXI-Lite: s_axi_CTRL (ap_ctrl + 标量) ----------------
    reg  [5:0]  s_axi_CTRL_AWADDR;
    reg         s_axi_CTRL_AWVALID;
    wire        s_axi_CTRL_AWREADY;
    reg  [31:0] s_axi_CTRL_WDATA;
    reg  [3:0]  s_axi_CTRL_WSTRB;
    reg         s_axi_CTRL_WVALID;
    wire        s_axi_CTRL_WREADY;
    wire        s_axi_CTRL_BVALID;
    reg         s_axi_CTRL_BREADY;
    wire [1:0]  s_axi_CTRL_BRESP;
    reg  [5:0]  s_axi_CTRL_ARADDR;
    reg         s_axi_CTRL_ARVALID;
    wire        s_axi_CTRL_ARREADY;
    wire        s_axi_CTRL_RVALID;
    reg         s_axi_CTRL_RREADY;
    wire [31:0] s_axi_CTRL_RDATA;
    wire [1:0]  s_axi_CTRL_RRESP;

    // ---------------- AXI-Lite: s_axi_control (13 个 64bit 指针) ----------------
    reg  [7:0]  s_axi_control_AWADDR;
    reg         s_axi_control_AWVALID;
    wire        s_axi_control_AWREADY;
    reg  [31:0] s_axi_control_WDATA;
    reg  [3:0]  s_axi_control_WSTRB;
    reg         s_axi_control_WVALID;
    wire        s_axi_control_WREADY;
    wire        s_axi_control_BVALID;
    reg         s_axi_control_BREADY;
    wire [1:0]  s_axi_control_BRESP;
    reg  [7:0]  s_axi_control_ARADDR;
    reg         s_axi_control_ARVALID;
    wire        s_axi_control_ARREADY;
    wire        s_axi_control_RVALID;
    reg         s_axi_control_RREADY;
    wire [31:0] s_axi_control_RDATA;
    wire [1:0]  s_axi_control_RRESP;

    // ---------------- DUT <-> 8 个 AXI4 从机 ----------------
    wire        w_FM1_AWVALID;
    wire        w_FM1_AWREADY;
    wire [63:0] w_FM1_AWADDR;
    wire [ 7:0] w_FM1_AWLEN;
    wire [ 2:0] w_FM1_AWSIZE;
    wire [ 1:0] w_FM1_AWBURST;
    wire        w_FM1_WVALID;
    wire        w_FM1_WREADY;
    wire [31:0] w_FM1_WDATA;
    wire [ 3:0] w_FM1_WSTRB;
    wire        w_FM1_WLAST;
    wire        w_FM1_BVALID;
    wire        w_FM1_BREADY;
    wire [ 1:0] w_FM1_BRESP;
    wire        w_FM1_ARVALID;
    wire        w_FM1_ARREADY;
    wire [63:0] w_FM1_ARADDR;
    wire [ 7:0] w_FM1_ARLEN;
    wire [ 2:0] w_FM1_ARSIZE;
    wire [ 1:0] w_FM1_ARBURST;
    wire        w_FM1_RVALID;
    wire        w_FM1_RREADY;
    wire [31:0] w_FM1_RDATA;
    wire        w_FM1_RLAST;
    wire [ 1:0] w_FM1_RRESP;

    wire        w_FM2_AWVALID;
    wire        w_FM2_AWREADY;
    wire [63:0] w_FM2_AWADDR;
    wire [ 7:0] w_FM2_AWLEN;
    wire [ 2:0] w_FM2_AWSIZE;
    wire [ 1:0] w_FM2_AWBURST;
    wire        w_FM2_WVALID;
    wire        w_FM2_WREADY;
    wire [31:0] w_FM2_WDATA;
    wire [ 3:0] w_FM2_WSTRB;
    wire        w_FM2_WLAST;
    wire        w_FM2_BVALID;
    wire        w_FM2_BREADY;
    wire [ 1:0] w_FM2_BRESP;
    wire        w_FM2_ARVALID;
    wire        w_FM2_ARREADY;
    wire [63:0] w_FM2_ARADDR;
    wire [ 7:0] w_FM2_ARLEN;
    wire [ 2:0] w_FM2_ARSIZE;
    wire [ 1:0] w_FM2_ARBURST;
    wire        w_FM2_RVALID;
    wire        w_FM2_RREADY;
    wire [31:0] w_FM2_RDATA;
    wire        w_FM2_RLAST;
    wire [ 1:0] w_FM2_RRESP;

    wire        w_FM3_AWVALID;
    wire        w_FM3_AWREADY;
    wire [63:0] w_FM3_AWADDR;
    wire [ 7:0] w_FM3_AWLEN;
    wire [ 2:0] w_FM3_AWSIZE;
    wire [ 1:0] w_FM3_AWBURST;
    wire        w_FM3_WVALID;
    wire        w_FM3_WREADY;
    wire [31:0] w_FM3_WDATA;
    wire [ 3:0] w_FM3_WSTRB;
    wire        w_FM3_WLAST;
    wire        w_FM3_BVALID;
    wire        w_FM3_BREADY;
    wire [ 1:0] w_FM3_BRESP;
    wire        w_FM3_ARVALID;
    wire        w_FM3_ARREADY;
    wire [63:0] w_FM3_ARADDR;
    wire [ 7:0] w_FM3_ARLEN;
    wire [ 2:0] w_FM3_ARSIZE;
    wire [ 1:0] w_FM3_ARBURST;
    wire        w_FM3_RVALID;
    wire        w_FM3_RREADY;
    wire [31:0] w_FM3_RDATA;
    wire        w_FM3_RLAST;
    wire [ 1:0] w_FM3_RRESP;

    wire        w_FM4_AWVALID;
    wire        w_FM4_AWREADY;
    wire [63:0] w_FM4_AWADDR;
    wire [ 7:0] w_FM4_AWLEN;
    wire [ 2:0] w_FM4_AWSIZE;
    wire [ 1:0] w_FM4_AWBURST;
    wire        w_FM4_WVALID;
    wire        w_FM4_WREADY;
    wire [31:0] w_FM4_WDATA;
    wire [ 3:0] w_FM4_WSTRB;
    wire        w_FM4_WLAST;
    wire        w_FM4_BVALID;
    wire        w_FM4_BREADY;
    wire [ 1:0] w_FM4_BRESP;
    wire        w_FM4_ARVALID;
    wire        w_FM4_ARREADY;
    wire [63:0] w_FM4_ARADDR;
    wire [ 7:0] w_FM4_ARLEN;
    wire [ 2:0] w_FM4_ARSIZE;
    wire [ 1:0] w_FM4_ARBURST;
    wire        w_FM4_RVALID;
    wire        w_FM4_RREADY;
    wire [31:0] w_FM4_RDATA;
    wire        w_FM4_RLAST;
    wire [ 1:0] w_FM4_RRESP;

    wire        w_W1_AWVALID;
    wire        w_W1_AWREADY;
    wire [63:0] w_W1_AWADDR;
    wire [ 7:0] w_W1_AWLEN;
    wire [ 2:0] w_W1_AWSIZE;
    wire [ 1:0] w_W1_AWBURST;
    wire        w_W1_WVALID;
    wire        w_W1_WREADY;
    wire [31:0] w_W1_WDATA;
    wire [ 3:0] w_W1_WSTRB;
    wire        w_W1_WLAST;
    wire        w_W1_BVALID;
    wire        w_W1_BREADY;
    wire [ 1:0] w_W1_BRESP;
    wire        w_W1_ARVALID;
    wire        w_W1_ARREADY;
    wire [63:0] w_W1_ARADDR;
    wire [ 7:0] w_W1_ARLEN;
    wire [ 2:0] w_W1_ARSIZE;
    wire [ 1:0] w_W1_ARBURST;
    wire        w_W1_RVALID;
    wire        w_W1_RREADY;
    wire [31:0] w_W1_RDATA;
    wire        w_W1_RLAST;
    wire [ 1:0] w_W1_RRESP;

    wire        w_W2_AWVALID;
    wire        w_W2_AWREADY;
    wire [63:0] w_W2_AWADDR;
    wire [ 7:0] w_W2_AWLEN;
    wire [ 2:0] w_W2_AWSIZE;
    wire [ 1:0] w_W2_AWBURST;
    wire        w_W2_WVALID;
    wire        w_W2_WREADY;
    wire [31:0] w_W2_WDATA;
    wire [ 3:0] w_W2_WSTRB;
    wire        w_W2_WLAST;
    wire        w_W2_BVALID;
    wire        w_W2_BREADY;
    wire [ 1:0] w_W2_BRESP;
    wire        w_W2_ARVALID;
    wire        w_W2_ARREADY;
    wire [63:0] w_W2_ARADDR;
    wire [ 7:0] w_W2_ARLEN;
    wire [ 2:0] w_W2_ARSIZE;
    wire [ 1:0] w_W2_ARBURST;
    wire        w_W2_RVALID;
    wire        w_W2_RREADY;
    wire [31:0] w_W2_RDATA;
    wire        w_W2_RLAST;
    wire [ 1:0] w_W2_RRESP;

    wire        w_W3_AWVALID;
    wire        w_W3_AWREADY;
    wire [63:0] w_W3_AWADDR;
    wire [ 7:0] w_W3_AWLEN;
    wire [ 2:0] w_W3_AWSIZE;
    wire [ 1:0] w_W3_AWBURST;
    wire        w_W3_WVALID;
    wire        w_W3_WREADY;
    wire [31:0] w_W3_WDATA;
    wire [ 3:0] w_W3_WSTRB;
    wire        w_W3_WLAST;
    wire        w_W3_BVALID;
    wire        w_W3_BREADY;
    wire [ 1:0] w_W3_BRESP;
    wire        w_W3_ARVALID;
    wire        w_W3_ARREADY;
    wire [63:0] w_W3_ARADDR;
    wire [ 7:0] w_W3_ARLEN;
    wire [ 2:0] w_W3_ARSIZE;
    wire [ 1:0] w_W3_ARBURST;
    wire        w_W3_RVALID;
    wire        w_W3_RREADY;
    wire [31:0] w_W3_RDATA;
    wire        w_W3_RLAST;
    wire [ 1:0] w_W3_RRESP;

    wire        w_W4_AWVALID;
    wire        w_W4_AWREADY;
    wire [63:0] w_W4_AWADDR;
    wire [ 7:0] w_W4_AWLEN;
    wire [ 2:0] w_W4_AWSIZE;
    wire [ 1:0] w_W4_AWBURST;
    wire        w_W4_WVALID;
    wire        w_W4_WREADY;
    wire [31:0] w_W4_WDATA;
    wire [ 3:0] w_W4_WSTRB;
    wire        w_W4_WLAST;
    wire        w_W4_BVALID;
    wire        w_W4_BREADY;
    wire [ 1:0] w_W4_BRESP;
    wire        w_W4_ARVALID;
    wire        w_W4_ARREADY;
    wire [63:0] w_W4_ARADDR;
    wire [ 7:0] w_W4_ARLEN;
    wire [ 2:0] w_W4_ARSIZE;
    wire [ 1:0] w_W4_ARBURST;
    wire        w_W4_RVALID;
    wire        w_W4_RREADY;
    wire [31:0] w_W4_RDATA;
    wire        w_W4_RLAST;
    wire [ 1:0] w_W4_RRESP;

    // ---------------- DUT ----------------
    conv dut (
        .ap_clk(ap_clk),
        .ap_rst_n(ap_rst_n),
        .m_axi_FM1_AWVALID(w_FM1_AWVALID),
        .m_axi_FM1_AWREADY(w_FM1_AWREADY),
        .m_axi_FM1_AWADDR(w_FM1_AWADDR),
        .m_axi_FM1_AWLEN(w_FM1_AWLEN),
        .m_axi_FM1_AWSIZE(w_FM1_AWSIZE),
        .m_axi_FM1_AWBURST(w_FM1_AWBURST),
        .m_axi_FM1_WVALID(w_FM1_WVALID),
        .m_axi_FM1_WREADY(w_FM1_WREADY),
        .m_axi_FM1_WDATA(w_FM1_WDATA),
        .m_axi_FM1_WSTRB(w_FM1_WSTRB),
        .m_axi_FM1_WLAST(w_FM1_WLAST),
        .m_axi_FM1_BVALID(w_FM1_BVALID),
        .m_axi_FM1_BREADY(w_FM1_BREADY),
        .m_axi_FM1_BRESP(w_FM1_BRESP),
        .m_axi_FM1_ARVALID(w_FM1_ARVALID),
        .m_axi_FM1_ARREADY(w_FM1_ARREADY),
        .m_axi_FM1_ARADDR(w_FM1_ARADDR),
        .m_axi_FM1_ARLEN(w_FM1_ARLEN),
        .m_axi_FM1_ARSIZE(w_FM1_ARSIZE),
        .m_axi_FM1_ARBURST(w_FM1_ARBURST),
        .m_axi_FM1_RVALID(w_FM1_RVALID),
        .m_axi_FM1_RREADY(w_FM1_RREADY),
        .m_axi_FM1_RDATA(w_FM1_RDATA),
        .m_axi_FM1_RLAST(w_FM1_RLAST),
        .m_axi_FM1_RRESP(w_FM1_RRESP),
        .m_axi_FM1_BID(1'b0),
        .m_axi_FM1_BUSER(1'b0),
        .m_axi_FM1_RID(1'b0),
        .m_axi_FM1_RUSER(1'b0),
        .m_axi_FM2_AWVALID(w_FM2_AWVALID),
        .m_axi_FM2_AWREADY(w_FM2_AWREADY),
        .m_axi_FM2_AWADDR(w_FM2_AWADDR),
        .m_axi_FM2_AWLEN(w_FM2_AWLEN),
        .m_axi_FM2_AWSIZE(w_FM2_AWSIZE),
        .m_axi_FM2_AWBURST(w_FM2_AWBURST),
        .m_axi_FM2_WVALID(w_FM2_WVALID),
        .m_axi_FM2_WREADY(w_FM2_WREADY),
        .m_axi_FM2_WDATA(w_FM2_WDATA),
        .m_axi_FM2_WSTRB(w_FM2_WSTRB),
        .m_axi_FM2_WLAST(w_FM2_WLAST),
        .m_axi_FM2_BVALID(w_FM2_BVALID),
        .m_axi_FM2_BREADY(w_FM2_BREADY),
        .m_axi_FM2_BRESP(w_FM2_BRESP),
        .m_axi_FM2_ARVALID(w_FM2_ARVALID),
        .m_axi_FM2_ARREADY(w_FM2_ARREADY),
        .m_axi_FM2_ARADDR(w_FM2_ARADDR),
        .m_axi_FM2_ARLEN(w_FM2_ARLEN),
        .m_axi_FM2_ARSIZE(w_FM2_ARSIZE),
        .m_axi_FM2_ARBURST(w_FM2_ARBURST),
        .m_axi_FM2_RVALID(w_FM2_RVALID),
        .m_axi_FM2_RREADY(w_FM2_RREADY),
        .m_axi_FM2_RDATA(w_FM2_RDATA),
        .m_axi_FM2_RLAST(w_FM2_RLAST),
        .m_axi_FM2_RRESP(w_FM2_RRESP),
        .m_axi_FM2_BID(1'b0),
        .m_axi_FM2_BUSER(1'b0),
        .m_axi_FM2_RID(1'b0),
        .m_axi_FM2_RUSER(1'b0),
        .m_axi_FM3_AWVALID(w_FM3_AWVALID),
        .m_axi_FM3_AWREADY(w_FM3_AWREADY),
        .m_axi_FM3_AWADDR(w_FM3_AWADDR),
        .m_axi_FM3_AWLEN(w_FM3_AWLEN),
        .m_axi_FM3_AWSIZE(w_FM3_AWSIZE),
        .m_axi_FM3_AWBURST(w_FM3_AWBURST),
        .m_axi_FM3_WVALID(w_FM3_WVALID),
        .m_axi_FM3_WREADY(w_FM3_WREADY),
        .m_axi_FM3_WDATA(w_FM3_WDATA),
        .m_axi_FM3_WSTRB(w_FM3_WSTRB),
        .m_axi_FM3_WLAST(w_FM3_WLAST),
        .m_axi_FM3_BVALID(w_FM3_BVALID),
        .m_axi_FM3_BREADY(w_FM3_BREADY),
        .m_axi_FM3_BRESP(w_FM3_BRESP),
        .m_axi_FM3_ARVALID(w_FM3_ARVALID),
        .m_axi_FM3_ARREADY(w_FM3_ARREADY),
        .m_axi_FM3_ARADDR(w_FM3_ARADDR),
        .m_axi_FM3_ARLEN(w_FM3_ARLEN),
        .m_axi_FM3_ARSIZE(w_FM3_ARSIZE),
        .m_axi_FM3_ARBURST(w_FM3_ARBURST),
        .m_axi_FM3_RVALID(w_FM3_RVALID),
        .m_axi_FM3_RREADY(w_FM3_RREADY),
        .m_axi_FM3_RDATA(w_FM3_RDATA),
        .m_axi_FM3_RLAST(w_FM3_RLAST),
        .m_axi_FM3_RRESP(w_FM3_RRESP),
        .m_axi_FM3_BID(1'b0),
        .m_axi_FM3_BUSER(1'b0),
        .m_axi_FM3_RID(1'b0),
        .m_axi_FM3_RUSER(1'b0),
        .m_axi_FM4_AWVALID(w_FM4_AWVALID),
        .m_axi_FM4_AWREADY(w_FM4_AWREADY),
        .m_axi_FM4_AWADDR(w_FM4_AWADDR),
        .m_axi_FM4_AWLEN(w_FM4_AWLEN),
        .m_axi_FM4_AWSIZE(w_FM4_AWSIZE),
        .m_axi_FM4_AWBURST(w_FM4_AWBURST),
        .m_axi_FM4_WVALID(w_FM4_WVALID),
        .m_axi_FM4_WREADY(w_FM4_WREADY),
        .m_axi_FM4_WDATA(w_FM4_WDATA),
        .m_axi_FM4_WSTRB(w_FM4_WSTRB),
        .m_axi_FM4_WLAST(w_FM4_WLAST),
        .m_axi_FM4_BVALID(w_FM4_BVALID),
        .m_axi_FM4_BREADY(w_FM4_BREADY),
        .m_axi_FM4_BRESP(w_FM4_BRESP),
        .m_axi_FM4_ARVALID(w_FM4_ARVALID),
        .m_axi_FM4_ARREADY(w_FM4_ARREADY),
        .m_axi_FM4_ARADDR(w_FM4_ARADDR),
        .m_axi_FM4_ARLEN(w_FM4_ARLEN),
        .m_axi_FM4_ARSIZE(w_FM4_ARSIZE),
        .m_axi_FM4_ARBURST(w_FM4_ARBURST),
        .m_axi_FM4_RVALID(w_FM4_RVALID),
        .m_axi_FM4_RREADY(w_FM4_RREADY),
        .m_axi_FM4_RDATA(w_FM4_RDATA),
        .m_axi_FM4_RLAST(w_FM4_RLAST),
        .m_axi_FM4_RRESP(w_FM4_RRESP),
        .m_axi_FM4_BID(1'b0),
        .m_axi_FM4_BUSER(1'b0),
        .m_axi_FM4_RID(1'b0),
        .m_axi_FM4_RUSER(1'b0),
        .m_axi_W1_AWVALID(w_W1_AWVALID),
        .m_axi_W1_AWREADY(w_W1_AWREADY),
        .m_axi_W1_AWADDR(w_W1_AWADDR),
        .m_axi_W1_AWLEN(w_W1_AWLEN),
        .m_axi_W1_AWSIZE(w_W1_AWSIZE),
        .m_axi_W1_AWBURST(w_W1_AWBURST),
        .m_axi_W1_WVALID(w_W1_WVALID),
        .m_axi_W1_WREADY(w_W1_WREADY),
        .m_axi_W1_WDATA(w_W1_WDATA),
        .m_axi_W1_WSTRB(w_W1_WSTRB),
        .m_axi_W1_WLAST(w_W1_WLAST),
        .m_axi_W1_BVALID(w_W1_BVALID),
        .m_axi_W1_BREADY(w_W1_BREADY),
        .m_axi_W1_BRESP(w_W1_BRESP),
        .m_axi_W1_ARVALID(w_W1_ARVALID),
        .m_axi_W1_ARREADY(w_W1_ARREADY),
        .m_axi_W1_ARADDR(w_W1_ARADDR),
        .m_axi_W1_ARLEN(w_W1_ARLEN),
        .m_axi_W1_ARSIZE(w_W1_ARSIZE),
        .m_axi_W1_ARBURST(w_W1_ARBURST),
        .m_axi_W1_RVALID(w_W1_RVALID),
        .m_axi_W1_RREADY(w_W1_RREADY),
        .m_axi_W1_RDATA(w_W1_RDATA),
        .m_axi_W1_RLAST(w_W1_RLAST),
        .m_axi_W1_RRESP(w_W1_RRESP),
        .m_axi_W1_BID(1'b0),
        .m_axi_W1_BUSER(1'b0),
        .m_axi_W1_RID(1'b0),
        .m_axi_W1_RUSER(1'b0),
        .m_axi_W2_AWVALID(w_W2_AWVALID),
        .m_axi_W2_AWREADY(w_W2_AWREADY),
        .m_axi_W2_AWADDR(w_W2_AWADDR),
        .m_axi_W2_AWLEN(w_W2_AWLEN),
        .m_axi_W2_AWSIZE(w_W2_AWSIZE),
        .m_axi_W2_AWBURST(w_W2_AWBURST),
        .m_axi_W2_WVALID(w_W2_WVALID),
        .m_axi_W2_WREADY(w_W2_WREADY),
        .m_axi_W2_WDATA(w_W2_WDATA),
        .m_axi_W2_WSTRB(w_W2_WSTRB),
        .m_axi_W2_WLAST(w_W2_WLAST),
        .m_axi_W2_BVALID(w_W2_BVALID),
        .m_axi_W2_BREADY(w_W2_BREADY),
        .m_axi_W2_BRESP(w_W2_BRESP),
        .m_axi_W2_ARVALID(w_W2_ARVALID),
        .m_axi_W2_ARREADY(w_W2_ARREADY),
        .m_axi_W2_ARADDR(w_W2_ARADDR),
        .m_axi_W2_ARLEN(w_W2_ARLEN),
        .m_axi_W2_ARSIZE(w_W2_ARSIZE),
        .m_axi_W2_ARBURST(w_W2_ARBURST),
        .m_axi_W2_RVALID(w_W2_RVALID),
        .m_axi_W2_RREADY(w_W2_RREADY),
        .m_axi_W2_RDATA(w_W2_RDATA),
        .m_axi_W2_RLAST(w_W2_RLAST),
        .m_axi_W2_RRESP(w_W2_RRESP),
        .m_axi_W2_BID(1'b0),
        .m_axi_W2_BUSER(1'b0),
        .m_axi_W2_RID(1'b0),
        .m_axi_W2_RUSER(1'b0),
        .m_axi_W3_AWVALID(w_W3_AWVALID),
        .m_axi_W3_AWREADY(w_W3_AWREADY),
        .m_axi_W3_AWADDR(w_W3_AWADDR),
        .m_axi_W3_AWLEN(w_W3_AWLEN),
        .m_axi_W3_AWSIZE(w_W3_AWSIZE),
        .m_axi_W3_AWBURST(w_W3_AWBURST),
        .m_axi_W3_WVALID(w_W3_WVALID),
        .m_axi_W3_WREADY(w_W3_WREADY),
        .m_axi_W3_WDATA(w_W3_WDATA),
        .m_axi_W3_WSTRB(w_W3_WSTRB),
        .m_axi_W3_WLAST(w_W3_WLAST),
        .m_axi_W3_BVALID(w_W3_BVALID),
        .m_axi_W3_BREADY(w_W3_BREADY),
        .m_axi_W3_BRESP(w_W3_BRESP),
        .m_axi_W3_ARVALID(w_W3_ARVALID),
        .m_axi_W3_ARREADY(w_W3_ARREADY),
        .m_axi_W3_ARADDR(w_W3_ARADDR),
        .m_axi_W3_ARLEN(w_W3_ARLEN),
        .m_axi_W3_ARSIZE(w_W3_ARSIZE),
        .m_axi_W3_ARBURST(w_W3_ARBURST),
        .m_axi_W3_RVALID(w_W3_RVALID),
        .m_axi_W3_RREADY(w_W3_RREADY),
        .m_axi_W3_RDATA(w_W3_RDATA),
        .m_axi_W3_RLAST(w_W3_RLAST),
        .m_axi_W3_RRESP(w_W3_RRESP),
        .m_axi_W3_BID(1'b0),
        .m_axi_W3_BUSER(1'b0),
        .m_axi_W3_RID(1'b0),
        .m_axi_W3_RUSER(1'b0),
        .m_axi_W4_AWVALID(w_W4_AWVALID),
        .m_axi_W4_AWREADY(w_W4_AWREADY),
        .m_axi_W4_AWADDR(w_W4_AWADDR),
        .m_axi_W4_AWLEN(w_W4_AWLEN),
        .m_axi_W4_AWSIZE(w_W4_AWSIZE),
        .m_axi_W4_AWBURST(w_W4_AWBURST),
        .m_axi_W4_WVALID(w_W4_WVALID),
        .m_axi_W4_WREADY(w_W4_WREADY),
        .m_axi_W4_WDATA(w_W4_WDATA),
        .m_axi_W4_WSTRB(w_W4_WSTRB),
        .m_axi_W4_WLAST(w_W4_WLAST),
        .m_axi_W4_BVALID(w_W4_BVALID),
        .m_axi_W4_BREADY(w_W4_BREADY),
        .m_axi_W4_BRESP(w_W4_BRESP),
        .m_axi_W4_ARVALID(w_W4_ARVALID),
        .m_axi_W4_ARREADY(w_W4_ARREADY),
        .m_axi_W4_ARADDR(w_W4_ARADDR),
        .m_axi_W4_ARLEN(w_W4_ARLEN),
        .m_axi_W4_ARSIZE(w_W4_ARSIZE),
        .m_axi_W4_ARBURST(w_W4_ARBURST),
        .m_axi_W4_RVALID(w_W4_RVALID),
        .m_axi_W4_RREADY(w_W4_RREADY),
        .m_axi_W4_RDATA(w_W4_RDATA),
        .m_axi_W4_RLAST(w_W4_RLAST),
        .m_axi_W4_RRESP(w_W4_RRESP),
        .m_axi_W4_BID(1'b0),
        .m_axi_W4_BUSER(1'b0),
        .m_axi_W4_RID(1'b0),
        .m_axi_W4_RUSER(1'b0),
        .s_axi_CTRL_AWVALID(s_axi_CTRL_AWVALID),
        .s_axi_CTRL_AWREADY(s_axi_CTRL_AWREADY),
        .s_axi_CTRL_AWADDR(s_axi_CTRL_AWADDR),
        .s_axi_CTRL_WVALID(s_axi_CTRL_WVALID),
        .s_axi_CTRL_WREADY(s_axi_CTRL_WREADY),
        .s_axi_CTRL_WDATA(s_axi_CTRL_WDATA),
        .s_axi_CTRL_WSTRB(s_axi_CTRL_WSTRB),
        .s_axi_CTRL_ARVALID(s_axi_CTRL_ARVALID),
        .s_axi_CTRL_ARREADY(s_axi_CTRL_ARREADY),
        .s_axi_CTRL_ARADDR(s_axi_CTRL_ARADDR),
        .s_axi_CTRL_RVALID(s_axi_CTRL_RVALID),
        .s_axi_CTRL_RREADY(s_axi_CTRL_RREADY),
        .s_axi_CTRL_RDATA(s_axi_CTRL_RDATA),
        .s_axi_CTRL_RRESP(s_axi_CTRL_RRESP),
        .s_axi_CTRL_BVALID(s_axi_CTRL_BVALID),
        .s_axi_CTRL_BREADY(s_axi_CTRL_BREADY),
        .s_axi_CTRL_BRESP(s_axi_CTRL_BRESP),
        .s_axi_control_AWVALID(s_axi_control_AWVALID),
        .s_axi_control_AWREADY(s_axi_control_AWREADY),
        .s_axi_control_AWADDR(s_axi_control_AWADDR),
        .s_axi_control_WVALID(s_axi_control_WVALID),
        .s_axi_control_WREADY(s_axi_control_WREADY),
        .s_axi_control_WDATA(s_axi_control_WDATA),
        .s_axi_control_WSTRB(s_axi_control_WSTRB),
        .s_axi_control_ARVALID(s_axi_control_ARVALID),
        .s_axi_control_ARREADY(s_axi_control_ARREADY),
        .s_axi_control_ARADDR(s_axi_control_ARADDR),
        .s_axi_control_RVALID(s_axi_control_RVALID),
        .s_axi_control_RREADY(s_axi_control_RREADY),
        .s_axi_control_RDATA(s_axi_control_RDATA),
        .s_axi_control_RRESP(s_axi_control_RRESP),
        .s_axi_control_BVALID(s_axi_control_BVALID),
        .s_axi_control_BREADY(s_axi_control_BREADY),
        .s_axi_control_BRESP(s_axi_control_BRESP)
    );

    // ---------------- 8 个 AXI4 从机 RAM ----------------
    axi4_slave_ram #(
        .MEM_WORDS(4096),
        .USE_INIT(1),
        .INIT_FILE("fm_init.hex")
    ) u_fm1 (
        .aclk(ap_clk),
        .aresetn(ap_rst_n),
        .s_awvalid(w_FM1_AWVALID),
        .s_awready(w_FM1_AWREADY),
        .s_awaddr(w_FM1_AWADDR),
        .s_awlen(w_FM1_AWLEN),
        .s_awsize(w_FM1_AWSIZE),
        .s_awburst(w_FM1_AWBURST),
        .s_wvalid(w_FM1_WVALID),
        .s_wready(w_FM1_WREADY),
        .s_wdata(w_FM1_WDATA),
        .s_wstrb(w_FM1_WSTRB),
        .s_wlast(w_FM1_WLAST),
        .s_bvalid(w_FM1_BVALID),
        .s_bready(w_FM1_BREADY),
        .s_bresp(w_FM1_BRESP),
        .s_arvalid(w_FM1_ARVALID),
        .s_arready(w_FM1_ARREADY),
        .s_araddr(w_FM1_ARADDR),
        .s_arlen(w_FM1_ARLEN),
        .s_arsize(w_FM1_ARSIZE),
        .s_arburst(w_FM1_ARBURST),
        .s_rvalid(w_FM1_RVALID),
        .s_rready(w_FM1_RREADY),
        .s_rdata(w_FM1_RDATA),
        .s_rlast(w_FM1_RLAST),
        .s_rresp(w_FM1_RRESP)
    );

    axi4_slave_ram #(
        .MEM_WORDS(4096),
        .USE_INIT(1),
        .INIT_FILE("fm_init.hex")
    ) u_fm2 (
        .aclk(ap_clk),
        .aresetn(ap_rst_n),
        .s_awvalid(w_FM2_AWVALID),
        .s_awready(w_FM2_AWREADY),
        .s_awaddr(w_FM2_AWADDR),
        .s_awlen(w_FM2_AWLEN),
        .s_awsize(w_FM2_AWSIZE),
        .s_awburst(w_FM2_AWBURST),
        .s_wvalid(w_FM2_WVALID),
        .s_wready(w_FM2_WREADY),
        .s_wdata(w_FM2_WDATA),
        .s_wstrb(w_FM2_WSTRB),
        .s_wlast(w_FM2_WLAST),
        .s_bvalid(w_FM2_BVALID),
        .s_bready(w_FM2_BREADY),
        .s_bresp(w_FM2_BRESP),
        .s_arvalid(w_FM2_ARVALID),
        .s_arready(w_FM2_ARREADY),
        .s_araddr(w_FM2_ARADDR),
        .s_arlen(w_FM2_ARLEN),
        .s_arsize(w_FM2_ARSIZE),
        .s_arburst(w_FM2_ARBURST),
        .s_rvalid(w_FM2_RVALID),
        .s_rready(w_FM2_RREADY),
        .s_rdata(w_FM2_RDATA),
        .s_rlast(w_FM2_RLAST),
        .s_rresp(w_FM2_RRESP)
    );

    axi4_slave_ram #(
        .MEM_WORDS(4096),
        .USE_INIT(1),
        .INIT_FILE("fm_init.hex")
    ) u_fm3 (
        .aclk(ap_clk),
        .aresetn(ap_rst_n),
        .s_awvalid(w_FM3_AWVALID),
        .s_awready(w_FM3_AWREADY),
        .s_awaddr(w_FM3_AWADDR),
        .s_awlen(w_FM3_AWLEN),
        .s_awsize(w_FM3_AWSIZE),
        .s_awburst(w_FM3_AWBURST),
        .s_wvalid(w_FM3_WVALID),
        .s_wready(w_FM3_WREADY),
        .s_wdata(w_FM3_WDATA),
        .s_wstrb(w_FM3_WSTRB),
        .s_wlast(w_FM3_WLAST),
        .s_bvalid(w_FM3_BVALID),
        .s_bready(w_FM3_BREADY),
        .s_bresp(w_FM3_BRESP),
        .s_arvalid(w_FM3_ARVALID),
        .s_arready(w_FM3_ARREADY),
        .s_araddr(w_FM3_ARADDR),
        .s_arlen(w_FM3_ARLEN),
        .s_arsize(w_FM3_ARSIZE),
        .s_arburst(w_FM3_ARBURST),
        .s_rvalid(w_FM3_RVALID),
        .s_rready(w_FM3_RREADY),
        .s_rdata(w_FM3_RDATA),
        .s_rlast(w_FM3_RLAST),
        .s_rresp(w_FM3_RRESP)
    );

    axi4_slave_ram #(
        .MEM_WORDS(4096),
        .USE_INIT(1),
        .INIT_FILE("fm_init.hex")
    ) u_fm4 (
        .aclk(ap_clk),
        .aresetn(ap_rst_n),
        .s_awvalid(w_FM4_AWVALID),
        .s_awready(w_FM4_AWREADY),
        .s_awaddr(w_FM4_AWADDR),
        .s_awlen(w_FM4_AWLEN),
        .s_awsize(w_FM4_AWSIZE),
        .s_awburst(w_FM4_AWBURST),
        .s_wvalid(w_FM4_WVALID),
        .s_wready(w_FM4_WREADY),
        .s_wdata(w_FM4_WDATA),
        .s_wstrb(w_FM4_WSTRB),
        .s_wlast(w_FM4_WLAST),
        .s_bvalid(w_FM4_BVALID),
        .s_bready(w_FM4_BREADY),
        .s_bresp(w_FM4_BRESP),
        .s_arvalid(w_FM4_ARVALID),
        .s_arready(w_FM4_ARREADY),
        .s_araddr(w_FM4_ARADDR),
        .s_arlen(w_FM4_ARLEN),
        .s_arsize(w_FM4_ARSIZE),
        .s_arburst(w_FM4_ARBURST),
        .s_rvalid(w_FM4_RVALID),
        .s_rready(w_FM4_RREADY),
        .s_rdata(w_FM4_RDATA),
        .s_rlast(w_FM4_RLAST),
        .s_rresp(w_FM4_RRESP)
    );

    axi4_slave_ram #(
        .MEM_WORDS(4096),
        .USE_INIT(1),
        .INIT_FILE("w1_init.hex")
    ) u_w1 (
        .aclk(ap_clk),
        .aresetn(ap_rst_n),
        .s_awvalid(w_W1_AWVALID),
        .s_awready(w_W1_AWREADY),
        .s_awaddr(w_W1_AWADDR),
        .s_awlen(w_W1_AWLEN),
        .s_awsize(w_W1_AWSIZE),
        .s_awburst(w_W1_AWBURST),
        .s_wvalid(w_W1_WVALID),
        .s_wready(w_W1_WREADY),
        .s_wdata(w_W1_WDATA),
        .s_wstrb(w_W1_WSTRB),
        .s_wlast(w_W1_WLAST),
        .s_bvalid(w_W1_BVALID),
        .s_bready(w_W1_BREADY),
        .s_bresp(w_W1_BRESP),
        .s_arvalid(w_W1_ARVALID),
        .s_arready(w_W1_ARREADY),
        .s_araddr(w_W1_ARADDR),
        .s_arlen(w_W1_ARLEN),
        .s_arsize(w_W1_ARSIZE),
        .s_arburst(w_W1_ARBURST),
        .s_rvalid(w_W1_RVALID),
        .s_rready(w_W1_RREADY),
        .s_rdata(w_W1_RDATA),
        .s_rlast(w_W1_RLAST),
        .s_rresp(w_W1_RRESP)
    );

    axi4_slave_ram #(
        .MEM_WORDS(4096),
        .USE_INIT(0),
        .INIT_FILE("none.hex")
    ) u_w2 (
        .aclk(ap_clk),
        .aresetn(ap_rst_n),
        .s_awvalid(w_W2_AWVALID),
        .s_awready(w_W2_AWREADY),
        .s_awaddr(w_W2_AWADDR),
        .s_awlen(w_W2_AWLEN),
        .s_awsize(w_W2_AWSIZE),
        .s_awburst(w_W2_AWBURST),
        .s_wvalid(w_W2_WVALID),
        .s_wready(w_W2_WREADY),
        .s_wdata(w_W2_WDATA),
        .s_wstrb(w_W2_WSTRB),
        .s_wlast(w_W2_WLAST),
        .s_bvalid(w_W2_BVALID),
        .s_bready(w_W2_BREADY),
        .s_bresp(w_W2_BRESP),
        .s_arvalid(w_W2_ARVALID),
        .s_arready(w_W2_ARREADY),
        .s_araddr(w_W2_ARADDR),
        .s_arlen(w_W2_ARLEN),
        .s_arsize(w_W2_ARSIZE),
        .s_arburst(w_W2_ARBURST),
        .s_rvalid(w_W2_RVALID),
        .s_rready(w_W2_RREADY),
        .s_rdata(w_W2_RDATA),
        .s_rlast(w_W2_RLAST),
        .s_rresp(w_W2_RRESP)
    );

    axi4_slave_ram #(
        .MEM_WORDS(4096),
        .USE_INIT(0),
        .INIT_FILE("none.hex")
    ) u_w3 (
        .aclk(ap_clk),
        .aresetn(ap_rst_n),
        .s_awvalid(w_W3_AWVALID),
        .s_awready(w_W3_AWREADY),
        .s_awaddr(w_W3_AWADDR),
        .s_awlen(w_W3_AWLEN),
        .s_awsize(w_W3_AWSIZE),
        .s_awburst(w_W3_AWBURST),
        .s_wvalid(w_W3_WVALID),
        .s_wready(w_W3_WREADY),
        .s_wdata(w_W3_WDATA),
        .s_wstrb(w_W3_WSTRB),
        .s_wlast(w_W3_WLAST),
        .s_bvalid(w_W3_BVALID),
        .s_bready(w_W3_BREADY),
        .s_bresp(w_W3_BRESP),
        .s_arvalid(w_W3_ARVALID),
        .s_arready(w_W3_ARREADY),
        .s_araddr(w_W3_ARADDR),
        .s_arlen(w_W3_ARLEN),
        .s_arsize(w_W3_ARSIZE),
        .s_arburst(w_W3_ARBURST),
        .s_rvalid(w_W3_RVALID),
        .s_rready(w_W3_RREADY),
        .s_rdata(w_W3_RDATA),
        .s_rlast(w_W3_RLAST),
        .s_rresp(w_W3_RRESP)
    );

    axi4_slave_ram #(
        .MEM_WORDS(4096),
        .USE_INIT(0),
        .INIT_FILE("none.hex")
    ) u_w4 (
        .aclk(ap_clk),
        .aresetn(ap_rst_n),
        .s_awvalid(w_W4_AWVALID),
        .s_awready(w_W4_AWREADY),
        .s_awaddr(w_W4_AWADDR),
        .s_awlen(w_W4_AWLEN),
        .s_awsize(w_W4_AWSIZE),
        .s_awburst(w_W4_AWBURST),
        .s_wvalid(w_W4_WVALID),
        .s_wready(w_W4_WREADY),
        .s_wdata(w_W4_WDATA),
        .s_wstrb(w_W4_WSTRB),
        .s_wlast(w_W4_WLAST),
        .s_bvalid(w_W4_BVALID),
        .s_bready(w_W4_BREADY),
        .s_bresp(w_W4_BRESP),
        .s_arvalid(w_W4_ARVALID),
        .s_arready(w_W4_ARREADY),
        .s_araddr(w_W4_ARADDR),
        .s_arlen(w_W4_ARLEN),
        .s_arsize(w_W4_ARSIZE),
        .s_arburst(w_W4_ARBURST),
        .s_rvalid(w_W4_RVALID),
        .s_rready(w_W4_RREADY),
        .s_rdata(w_W4_RDATA),
        .s_rlast(w_W4_RLAST),
        .s_rresp(w_W4_RRESP)
    );

    // ---------------- AXI-Lite 写任务 ----------------
    task ctrl_write(input [5:0] addr, input [31:0] data);
        begin
            s_axi_CTRL_AWADDR  = addr;
            s_axi_CTRL_AWVALID = 1'b1;
            s_axi_CTRL_WDATA   = data;
            s_axi_CTRL_WSTRB   = 4'hF;
            s_axi_CTRL_WVALID  = 1'b1;
            while (!s_axi_CTRL_AWREADY) @(posedge ap_clk);
            @(posedge ap_clk);                 // AW 握手
            s_axi_CTRL_AWVALID = 1'b0;
            while (!s_axi_CTRL_WREADY) @(posedge ap_clk);
            @(posedge ap_clk);                 // W 握手
            s_axi_CTRL_WVALID  = 1'b0;
            while (!s_axi_CTRL_BVALID) @(posedge ap_clk);
            s_axi_CTRL_BREADY  = 1'b1;
            @(posedge ap_clk);                 // B 握手
            s_axi_CTRL_BREADY  = 1'b0;
        end
    endtask

    task ctrl_read(input [5:0] addr, output [31:0] data);
        begin
            s_axi_CTRL_ARADDR  = addr;
            s_axi_CTRL_ARVALID = 1'b1;
            while (!s_axi_CTRL_ARREADY) @(posedge ap_clk);
            @(posedge ap_clk);                 // AR 握手
            s_axi_CTRL_ARVALID = 1'b0;
            while (!s_axi_CTRL_RVALID) @(posedge ap_clk);
            data               = s_axi_CTRL_RDATA;
            s_axi_CTRL_RREADY  = 1'b1;
            @(posedge ap_clk);                 // R 握手
            s_axi_CTRL_RREADY  = 1'b0;
        end
    endtask

    task ptr_write(input [7:0] addr, input [31:0] data);
        begin
            s_axi_control_AWADDR  = addr;
            s_axi_control_AWVALID = 1'b1;
            s_axi_control_WDATA   = data;
            s_axi_control_WSTRB   = 4'hF;
            s_axi_control_WVALID  = 1'b1;
            while (!s_axi_control_AWREADY) @(posedge ap_clk);
            @(posedge ap_clk);
            s_axi_control_AWVALID = 1'b0;
            while (!s_axi_control_WREADY) @(posedge ap_clk);
            @(posedge ap_clk);
            s_axi_control_WVALID  = 1'b0;
            while (!s_axi_control_BVALID) @(posedge ap_clk);
            s_axi_control_BREADY  = 1'b1;
            @(posedge ap_clk);
            s_axi_control_BREADY  = 1'b0;
        end
    endtask

    task ptr_read(input [7:0] addr, output [31:0] data);
        begin
            s_axi_control_ARADDR  = addr;
            s_axi_control_ARVALID = 1'b1;
            while (!s_axi_control_ARREADY) @(posedge ap_clk);
            @(posedge ap_clk);
            s_axi_control_ARVALID = 1'b0;
            while (!s_axi_control_RVALID) @(posedge ap_clk);
            data                  = s_axi_control_RDATA;
            s_axi_control_RREADY  = 1'b1;
            @(posedge ap_clk);
            s_axi_control_RREADY  = 1'b0;
        end
    endtask

    // ---------------- 结果比对 ----------------
    task check_output;
        integer ch, p, errors;
        reg [31:0] word32;
        reg [15:0] got;
        begin
            errors = 0;
            for (ch = 0; ch < CH_OUT; ch = ch + 1) begin
                for (p = 0; p < PIX; p = p + 1) begin
                    word32 = u_fm1.mem[OUT_WORD + ch * PIX_WORDS + (p / 2)];
                    got    = (p % 2) ? word32[31:16] : word32[15:0];
                    if (got !== FOUR_I16) begin
                        errors = errors + 1;
                        if (errors <= 10)
                            $display("    MISMATCH ch=%0d pix=%0d got=0x%04h exp=0x%04h",
                                     ch, p, got, FOUR_I16);
                    end
                end
            end
            $display("    checked %0d x %0d = %0d elements", CH_OUT, PIX, CH_OUT * PIX);
            if (errors == 0)
                $display("RESULT: PASS  (all outputs == 4.0 = 0x0800)");
            else
                $display("RESULT: FAIL  (%0d / %0d mismatches)", errors, CH_OUT * PIX);
        end
    endtask

    // ---------------- 主流程 ----------------
    reg [31:0] rb;
    reg        done;
    integer    polls;
    real       t_start, t_end;

    initial begin
        // 所有驱动信号先清零
        s_axi_CTRL_AWADDR = 6'd0;   s_axi_CTRL_AWVALID = 1'b0;
        s_axi_CTRL_WDATA  = 32'd0;  s_axi_CTRL_WSTRB   = 4'd0;  s_axi_CTRL_WVALID = 1'b0;
        s_axi_CTRL_BREADY = 1'b0;
        s_axi_CTRL_ARADDR = 6'd0;   s_axi_CTRL_ARVALID = 1'b0;  s_axi_CTRL_RREADY = 1'b0;
        s_axi_control_AWADDR = 8'd0; s_axi_control_AWVALID = 1'b0;
        s_axi_control_WDATA  = 32'd0; s_axi_control_WSTRB = 4'd0; s_axi_control_WVALID = 1'b0;
        s_axi_control_BREADY = 1'b0;
        s_axi_control_ARADDR = 8'd0; s_axi_control_ARVALID = 1'b0; s_axi_control_RREADY = 1'b0;

        ap_rst_n = 1'b0;
        #300;
        ap_rst_n = 1'b1;
        #100;

        $display("");
        $display("================================================================");
        $display(" conv 1x1 pwconv testbench");
        $display("   ch_in=%0d  ch_out=%0d  fsize=%0d  stride=%0d  kernel=%0d  act=%0d",
                 CH_IN, CH_OUT, FSIZE, STRIDE, KERNEL, ACT);
        $display("   in1..in4 base = 0x%016h", IN_BASE);
        $display("   w1 base       = 0x%016h   b base = 0x%016h", W1_BASE, B_BASE);
        $display("   out1 base     = 0x%016h", OUT_BASE);
        $display("   expect        = 0x%04h (4.0) for all outputs", FOUR_I16);
        $display("================================================================");

        // ---- 1. 配置 13 个 64bit 指针（高 32bit 复位即为 0，此处只写低 32bit）----
        ptr_write(8'h10, IN_BASE[31:0]);    // in1
        ptr_write(8'h1c, IN_BASE[31:0]);    // in2
        ptr_write(8'h28, IN_BASE[31:0]);    // in3
        ptr_write(8'h34, IN_BASE[31:0]);    // in4
        ptr_write(8'h40, W1_BASE[31:0]);    // w1
        ptr_write(8'h4c, W1_BASE[31:0]);    // w2
        ptr_write(8'h58, W1_BASE[31:0]);    // w3
        ptr_write(8'h64, W1_BASE[31:0]);    // w4
        ptr_write(8'h70, B_BASE[31:0]);     // b
        ptr_write(8'h7c, OUT_BASE[31:0]);   // out1
        ptr_write(8'h88, OUT_BASE[31:0]);   // out2
        ptr_write(8'h94, OUT_BASE[31:0]);   // out3
        ptr_write(8'ha0, OUT_BASE[31:0]);   // out4

        // ---- 2. 配置标量参数 ----
        ctrl_write(6'h10, CH_IN);      // ch_in
        ctrl_write(6'h18, CH_OUT);     // ch_out
        ctrl_write(6'h20, FSIZE);      // fsize
        ctrl_write(6'h28, STRIDE);     // stride
        ctrl_write(6'h30, KERNEL);     // kernel = 1 -> pwconv
        ctrl_write(6'h38, ACT);        // act

        // ---- 3. 回读确认 ----
        ctrl_read(6'h10, rb); $display("  readback ch_in  = %0d", rb);
        ctrl_read(6'h18, rb); $display("  readback ch_out = %0d", rb);
        ctrl_read(6'h20, rb); $display("  readback fsize  = %0d", rb);
        ctrl_read(6'h28, rb); $display("  readback stride = %0d", rb);
        ctrl_read(6'h30, rb); $display("  readback kernel = %0d", rb);
        ctrl_read(6'h38, rb); $display("  readback act    = %0d", rb);
        ptr_read(8'h7c, rb);  $display("  readback out1_lo= 0x%08h", rb);

        // ---- 4. 启动 ----
        t_start = $realtime;
        ctrl_write(6'h00, 32'h0000_0001);   // ap_start = 1

        // ---- 5. 轮询 ap_done (AP_CTRL bit1) ----
        polls = 0;
        done  = 1'b0;
        while (!done && polls < 100000) begin
            ctrl_read(6'h00, rb);
            done  = rb[1];
            polls = polls + 1;
        end
        t_end = $realtime;

        if (!done) begin
            $display("");
            $display("RESULT: TIMEOUT  (ap_done never asserted after %0d polls, %.1f us)",
                     polls, (t_end - t_start) / 1000.0);
        end
        else begin
            $display("");
            $display("  ap_done asserted after %0d polls, kernel time = %.3f us",
                     polls, (t_end - t_start) / 1000.0);
            check_output;
        end

        $display("================================================================");
        $display("");
        $finish;
    end

    // ---------------- 逐拍记录关键握手信号（供波形渲染/结果核对） ----------------
    integer ev_fd;

    initial begin
        ev_fd = $fopen("events.log", "w");
        if (ev_fd == 0)
            $display("WARNING: cannot open events.log");
    end

    always @(negedge ap_clk) begin
        $fwrite(ev_fd, "%0.3f %b%b%b%b%b%b %b%b%b%b %b%b%b%b%b%b %b%b %h %h %h %h\n",
                $realtime,
                s_axi_CTRL_AWVALID, s_axi_CTRL_AWREADY, s_axi_CTRL_WVALID,
                s_axi_CTRL_WREADY, s_axi_CTRL_BVALID, s_axi_CTRL_BREADY,
                w_FM1_ARVALID, w_FM1_ARREADY, w_FM1_RVALID, w_FM1_RREADY,
                w_FM1_AWVALID, w_FM1_AWREADY, w_FM1_WVALID,
                w_FM1_WREADY, w_FM1_BVALID, w_FM1_BREADY,
                dut.ap_start, dut.ap_done,
                w_FM1_ARADDR, w_FM1_RDATA, w_FM1_AWADDR, w_FM1_WDATA);
    end

endmodule
