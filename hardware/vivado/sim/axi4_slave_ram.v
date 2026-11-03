`timescale 1ns/1ps
// -----------------------------------------------------------------------------
// axi4_slave_ram.v
// 简易 AXI4 全功能从机（单通道、非流水）——仅用于行为级仿真，不可综合上板。
//   * 数据宽度 32bit，地址宽度 64bit
//   * 支持 INCR(01) / WRAP(10) 突发，支持 WSTRB 字节写使能
//   * 读数据在 AR 握手后一拍给出，RLAST 按 ARLEN 生成
// 内存按 32bit 字存储；conv IP 内部元素是 16bit(ap_fixed<16,7>)，
// 因此相邻两个元素打包在一个 32bit 字里：低 16bit=偶数元素，高 16bit=奇数元素。
// -----------------------------------------------------------------------------
module axi4_slave_ram #(
    parameter integer MEM_WORDS  = 4096,
    parameter integer ADDR_WIDTH = 64,
    parameter integer DATA_WIDTH = 32,
    parameter integer USE_INIT   = 0,
    parameter         INIT_FILE  = "none.hex"
)(
    input  wire                    aclk,
    input  wire                    aresetn,

    // AXI4 write address channel
    input  wire [ADDR_WIDTH-1:0]   s_awaddr,
    input  wire [7:0]              s_awlen,
    input  wire [2:0]              s_awsize,
    input  wire [1:0]              s_awburst,
    input  wire                    s_awvalid,
    output wire                    s_awready,

    // AXI4 write data channel
    input  wire [DATA_WIDTH-1:0]   s_wdata,
    input  wire [DATA_WIDTH/8-1:0] s_wstrb,
    input  wire                    s_wlast,
    input  wire                    s_wvalid,
    output wire                    s_wready,

    // AXI4 write response channel
    output wire                    s_bvalid,
    input  wire                    s_bready,
    output wire [1:0]              s_bresp,

    // AXI4 read address channel
    input  wire [ADDR_WIDTH-1:0]   s_araddr,
    input  wire [7:0]              s_arlen,
    input  wire [2:0]              s_arsize,
    input  wire [1:0]              s_arburst,
    input  wire                    s_arvalid,
    output wire                    s_arready,

    // AXI4 read data channel
    output wire [DATA_WIDTH-1:0]   s_rdata,
    output wire                    s_rlast,
    output wire                    s_rvalid,
    input  wire                    s_rready,
    output wire [1:0]              s_rresp
);

    // ---------------- memory ----------------
    reg [DATA_WIDTH-1:0] mem [0:MEM_WORDS-1];

    integer k;
    initial begin
        for (k = 0; k < MEM_WORDS; k = k + 1)
            mem[k] = {DATA_WIDTH{1'b0}};
        if (USE_INIT != 0)
            $readmemh(INIT_FILE, mem);
    end

    // 字节地址 -> 字地址
    function integer mem_index;
        input [ADDR_WIDTH-1:0] addr;
        begin
            mem_index = (addr >> 2) % MEM_WORDS;
        end
    endfunction

    // 下一拍地址：INCR 线性 +4；WRAP 在 (len+1)*4 字节窗口内回卷
    function [ADDR_WIDTH-1:0] next_beat_addr;
        input [ADDR_WIDTH-1:0] addr;
        input [7:0]            len;
        input [1:0]            burst;
        reg   [31:0]           bytes;
        begin
            bytes = ({1'b0, len} + 1'b1) * 4;
            if (burst == 2'b10)
                next_beat_addr = (addr & ~(bytes - 1'b1)) | ((addr + 4) & (bytes - 1'b1));
            else
                next_beat_addr = addr + 4;
        end
    endfunction

    // ---------------- write channel ----------------
    reg                    aw_have_addr;
    reg [ADDR_WIDTH-1:0]   aw_addr;
    reg [7:0]              aw_len;
    reg [1:0]              aw_burst;

    reg                    w_buf_valid;
    reg [DATA_WIDTH-1:0]   w_buf_data;
    reg [DATA_WIDTH/8-1:0] w_buf_strb;
    reg                    w_buf_last;

    reg                    bvalid_r;
    reg [8:0]              w_beat_cnt;

    assign s_awready = ~aw_have_addr;
    assign s_wready  = ~w_buf_valid;
    assign s_bvalid  = bvalid_r;
    assign s_bresp   = 2'b00;   // OKAY

    wire w_commit = w_buf_valid && aw_have_addr && !bvalid_r;

    always @(posedge aclk) begin
        if (!aresetn) begin
            aw_have_addr <= 1'b0;
            aw_addr      <= {ADDR_WIDTH{1'b0}};
            aw_len       <= 8'd0;
            aw_burst     <= 2'd0;
            w_buf_valid  <= 1'b0;
            w_buf_data   <= {DATA_WIDTH{1'b0}};
            w_buf_strb   <= {(DATA_WIDTH/8){1'b0}};
            w_buf_last   <= 1'b0;
            bvalid_r     <= 1'b0;
            w_beat_cnt   <= 9'd0;
        end
        else begin
            // ---- AW 通道：锁存地址即可 ---- //
            if (s_awvalid && s_awready) begin
                aw_addr      <= s_awaddr;
                aw_len       <= s_awlen;
                aw_burst     <= s_awburst;
                aw_have_addr <= 1'b1;
                w_beat_cnt   <= 9'd0;
            end

            // ---- W 通道：先收一拍进缓冲，再落内存 ---- //
            if (s_wvalid && s_wready) begin
                w_buf_valid <= 1'b1;
                w_buf_data  <= s_wdata;
                w_buf_strb  <= s_wstrb;
                w_buf_last  <= s_wlast;
            end
            else if (w_commit) begin
                w_buf_valid <= 1'b0;
            end

            // ---- 落内存（字节写使能） ---- //
            if (w_commit) begin
                if (w_buf_strb[0]) mem[mem_index(aw_addr)][7:0]   <= w_buf_data[7:0];
                if (w_buf_strb[1]) mem[mem_index(aw_addr)][15:8]  <= w_buf_data[15:8];
                if (w_buf_strb[2]) mem[mem_index(aw_addr)][23:16] <= w_buf_data[23:16];
                if (w_buf_strb[3]) mem[mem_index(aw_addr)][31:24] <= w_buf_data[31:24];

                aw_addr    <= next_beat_addr(aw_addr, aw_len, aw_burst);
                w_beat_cnt <= w_beat_cnt + 1'b1;

                if (w_buf_last || (w_beat_cnt == aw_len)) begin
                    bvalid_r     <= 1'b1;
                    aw_have_addr <= 1'b0;
                end
            end

            if (bvalid_r && s_bready) begin
                bvalid_r   <= 1'b0;
                w_beat_cnt <= 9'd0;
            end
        end
    end

    // ---------------- read channel ----------------
    reg                    r_busy;
    reg                    r_valid;
    reg [ADDR_WIDTH-1:0]   r_addr;
    reg [7:0]              r_len;
    reg [1:0]              r_burst;
    reg [8:0]              r_beat_cnt;
    reg [DATA_WIDTH-1:0]   r_data;
    reg                    r_last;

    assign s_arready = ~r_busy;
    assign s_rvalid  = r_valid;
    assign s_rdata   = r_data;
    assign s_rlast   = r_last;
    assign s_rresp   = 2'b00;   // OKAY

    wire [ADDR_WIDTH-1:0] r_next_addr = next_beat_addr(r_addr, r_len, r_burst);

    always @(posedge aclk) begin
        if (!aresetn) begin
            r_busy     <= 1'b0;
            r_valid    <= 1'b0;
            r_addr     <= {ADDR_WIDTH{1'b0}};
            r_len      <= 8'd0;
            r_burst    <= 2'd0;
            r_beat_cnt <= 9'd0;
            r_data     <= {DATA_WIDTH{1'b0}};
            r_last     <= 1'b0;
        end
        else if (r_valid) begin
            if (s_rready) begin
                if (r_beat_cnt == r_len) begin
                    r_valid    <= 1'b0;
                    r_busy     <= 1'b0;
                    r_last     <= 1'b0;
                    r_beat_cnt <= 9'd0;
                end
                else begin
                    r_beat_cnt <= r_beat_cnt + 1'b1;
                    r_addr     <= r_next_addr;
                    r_data     <= mem[mem_index(r_next_addr)];
                    r_last     <= ((r_beat_cnt + 1'b1) == r_len);
                end
            end
        end
        else if (s_arvalid) begin
            r_busy     <= 1'b1;
            r_valid    <= 1'b1;
            r_addr     <= s_araddr;
            r_len      <= s_arlen;
            r_burst    <= s_arburst;
            r_beat_cnt <= 9'd0;
            r_data     <= mem[mem_index(s_araddr)];
            r_last     <= (s_arlen == 8'd0);
        end
    end

endmodule
