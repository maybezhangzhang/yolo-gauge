-- ==============================================================
-- Vitis HLS - High-Level Synthesis from C, C++ and OpenCL v2020.2 (64-bit)
-- Copyright 1986-2020 Xilinx, Inc. All Rights Reserved.
-- ==============================================================
library IEEE;
use IEEE.STD_LOGIC_1164.all;
use IEEE.NUMERIC_STD.all;

entity conv_control_s_axi is
generic (
    C_S_AXI_ADDR_WIDTH    : INTEGER := 8;
    C_S_AXI_DATA_WIDTH    : INTEGER := 32);
port (
    ACLK                  :in   STD_LOGIC;
    ARESET                :in   STD_LOGIC;
    ACLK_EN               :in   STD_LOGIC;
    AWADDR                :in   STD_LOGIC_VECTOR(C_S_AXI_ADDR_WIDTH-1 downto 0);
    AWVALID               :in   STD_LOGIC;
    AWREADY               :out  STD_LOGIC;
    WDATA                 :in   STD_LOGIC_VECTOR(C_S_AXI_DATA_WIDTH-1 downto 0);
    WSTRB                 :in   STD_LOGIC_VECTOR(C_S_AXI_DATA_WIDTH/8-1 downto 0);
    WVALID                :in   STD_LOGIC;
    WREADY                :out  STD_LOGIC;
    BRESP                 :out  STD_LOGIC_VECTOR(1 downto 0);
    BVALID                :out  STD_LOGIC;
    BREADY                :in   STD_LOGIC;
    ARADDR                :in   STD_LOGIC_VECTOR(C_S_AXI_ADDR_WIDTH-1 downto 0);
    ARVALID               :in   STD_LOGIC;
    ARREADY               :out  STD_LOGIC;
    RDATA                 :out  STD_LOGIC_VECTOR(C_S_AXI_DATA_WIDTH-1 downto 0);
    RRESP                 :out  STD_LOGIC_VECTOR(1 downto 0);
    RVALID                :out  STD_LOGIC;
    RREADY                :in   STD_LOGIC;
    in1                   :out  STD_LOGIC_VECTOR(63 downto 0);
    in2                   :out  STD_LOGIC_VECTOR(63 downto 0);
    in3                   :out  STD_LOGIC_VECTOR(63 downto 0);
    in4                   :out  STD_LOGIC_VECTOR(63 downto 0);
    w1_r                  :out  STD_LOGIC_VECTOR(63 downto 0);
    w2_r                  :out  STD_LOGIC_VECTOR(63 downto 0);
    w3_r                  :out  STD_LOGIC_VECTOR(63 downto 0);
    w4_r                  :out  STD_LOGIC_VECTOR(63 downto 0);
    b                     :out  STD_LOGIC_VECTOR(63 downto 0);
    out1                  :out  STD_LOGIC_VECTOR(63 downto 0);
    out2                  :out  STD_LOGIC_VECTOR(63 downto 0);
    out3                  :out  STD_LOGIC_VECTOR(63 downto 0);
    out4                  :out  STD_LOGIC_VECTOR(63 downto 0)
);
end entity conv_control_s_axi;

-- ------------------------Address Info-------------------
-- 0x00 : reserved
-- 0x04 : reserved
-- 0x08 : reserved
-- 0x0c : reserved
-- 0x10 : Data signal of in1
--        bit 31~0 - in1[31:0] (Read/Write)
-- 0x14 : Data signal of in1
--        bit 31~0 - in1[63:32] (Read/Write)
-- 0x18 : reserved
-- 0x1c : Data signal of in2
--        bit 31~0 - in2[31:0] (Read/Write)
-- 0x20 : Data signal of in2
--        bit 31~0 - in2[63:32] (Read/Write)
-- 0x24 : reserved
-- 0x28 : Data signal of in3
--        bit 31~0 - in3[31:0] (Read/Write)
-- 0x2c : Data signal of in3
--        bit 31~0 - in3[63:32] (Read/Write)
-- 0x30 : reserved
-- 0x34 : Data signal of in4
--        bit 31~0 - in4[31:0] (Read/Write)
-- 0x38 : Data signal of in4
--        bit 31~0 - in4[63:32] (Read/Write)
-- 0x3c : reserved
-- 0x40 : Data signal of w1_r
--        bit 31~0 - w1_r[31:0] (Read/Write)
-- 0x44 : Data signal of w1_r
--        bit 31~0 - w1_r[63:32] (Read/Write)
-- 0x48 : reserved
-- 0x4c : Data signal of w2_r
--        bit 31~0 - w2_r[31:0] (Read/Write)
-- 0x50 : Data signal of w2_r
--        bit 31~0 - w2_r[63:32] (Read/Write)
-- 0x54 : reserved
-- 0x58 : Data signal of w3_r
--        bit 31~0 - w3_r[31:0] (Read/Write)
-- 0x5c : Data signal of w3_r
--        bit 31~0 - w3_r[63:32] (Read/Write)
-- 0x60 : reserved
-- 0x64 : Data signal of w4_r
--        bit 31~0 - w4_r[31:0] (Read/Write)
-- 0x68 : Data signal of w4_r
--        bit 31~0 - w4_r[63:32] (Read/Write)
-- 0x6c : reserved
-- 0x70 : Data signal of b
--        bit 31~0 - b[31:0] (Read/Write)
-- 0x74 : Data signal of b
--        bit 31~0 - b[63:32] (Read/Write)
-- 0x78 : reserved
-- 0x7c : Data signal of out1
--        bit 31~0 - out1[31:0] (Read/Write)
-- 0x80 : Data signal of out1
--        bit 31~0 - out1[63:32] (Read/Write)
-- 0x84 : reserved
-- 0x88 : Data signal of out2
--        bit 31~0 - out2[31:0] (Read/Write)
-- 0x8c : Data signal of out2
--        bit 31~0 - out2[63:32] (Read/Write)
-- 0x90 : reserved
-- 0x94 : Data signal of out3
--        bit 31~0 - out3[31:0] (Read/Write)
-- 0x98 : Data signal of out3
--        bit 31~0 - out3[63:32] (Read/Write)
-- 0x9c : reserved
-- 0xa0 : Data signal of out4
--        bit 31~0 - out4[31:0] (Read/Write)
-- 0xa4 : Data signal of out4
--        bit 31~0 - out4[63:32] (Read/Write)
-- 0xa8 : reserved
-- (SC = Self Clear, COR = Clear on Read, TOW = Toggle on Write, COH = Clear on Handshake)

architecture behave of conv_control_s_axi is
    type states is (wridle, wrdata, wrresp, wrreset, rdidle, rddata, rdreset);  -- read and write fsm states
    signal wstate  : states := wrreset;
    signal rstate  : states := rdreset;
    signal wnext, rnext: states;
    constant ADDR_IN1_DATA_0  : INTEGER := 16#10#;
    constant ADDR_IN1_DATA_1  : INTEGER := 16#14#;
    constant ADDR_IN1_CTRL    : INTEGER := 16#18#;
    constant ADDR_IN2_DATA_0  : INTEGER := 16#1c#;
    constant ADDR_IN2_DATA_1  : INTEGER := 16#20#;
    constant ADDR_IN2_CTRL    : INTEGER := 16#24#;
    constant ADDR_IN3_DATA_0  : INTEGER := 16#28#;
    constant ADDR_IN3_DATA_1  : INTEGER := 16#2c#;
    constant ADDR_IN3_CTRL    : INTEGER := 16#30#;
    constant ADDR_IN4_DATA_0  : INTEGER := 16#34#;
    constant ADDR_IN4_DATA_1  : INTEGER := 16#38#;
    constant ADDR_IN4_CTRL    : INTEGER := 16#3c#;
    constant ADDR_W1_R_DATA_0 : INTEGER := 16#40#;
    constant ADDR_W1_R_DATA_1 : INTEGER := 16#44#;
    constant ADDR_W1_R_CTRL   : INTEGER := 16#48#;
    constant ADDR_W2_R_DATA_0 : INTEGER := 16#4c#;
    constant ADDR_W2_R_DATA_1 : INTEGER := 16#50#;
    constant ADDR_W2_R_CTRL   : INTEGER := 16#54#;
    constant ADDR_W3_R_DATA_0 : INTEGER := 16#58#;
    constant ADDR_W3_R_DATA_1 : INTEGER := 16#5c#;
    constant ADDR_W3_R_CTRL   : INTEGER := 16#60#;
    constant ADDR_W4_R_DATA_0 : INTEGER := 16#64#;
    constant ADDR_W4_R_DATA_1 : INTEGER := 16#68#;
    constant ADDR_W4_R_CTRL   : INTEGER := 16#6c#;
    constant ADDR_B_DATA_0    : INTEGER := 16#70#;
    constant ADDR_B_DATA_1    : INTEGER := 16#74#;
    constant ADDR_B_CTRL      : INTEGER := 16#78#;
    constant ADDR_OUT1_DATA_0 : INTEGER := 16#7c#;
    constant ADDR_OUT1_DATA_1 : INTEGER := 16#80#;
    constant ADDR_OUT1_CTRL   : INTEGER := 16#84#;
    constant ADDR_OUT2_DATA_0 : INTEGER := 16#88#;
    constant ADDR_OUT2_DATA_1 : INTEGER := 16#8c#;
    constant ADDR_OUT2_CTRL   : INTEGER := 16#90#;
    constant ADDR_OUT3_DATA_0 : INTEGER := 16#94#;
    constant ADDR_OUT3_DATA_1 : INTEGER := 16#98#;
    constant ADDR_OUT3_CTRL   : INTEGER := 16#9c#;
    constant ADDR_OUT4_DATA_0 : INTEGER := 16#a0#;
    constant ADDR_OUT4_DATA_1 : INTEGER := 16#a4#;
    constant ADDR_OUT4_CTRL   : INTEGER := 16#a8#;
    constant ADDR_BITS         : INTEGER := 8;

    signal waddr               : UNSIGNED(ADDR_BITS-1 downto 0);
    signal wmask               : UNSIGNED(C_S_AXI_DATA_WIDTH-1 downto 0);
    signal aw_hs               : STD_LOGIC;
    signal w_hs                : STD_LOGIC;
    signal rdata_data          : UNSIGNED(C_S_AXI_DATA_WIDTH-1 downto 0);
    signal ar_hs               : STD_LOGIC;
    signal raddr               : UNSIGNED(ADDR_BITS-1 downto 0);
    signal AWREADY_t           : STD_LOGIC;
    signal WREADY_t            : STD_LOGIC;
    signal ARREADY_t           : STD_LOGIC;
    signal RVALID_t            : STD_LOGIC;
    -- internal registers
    signal int_in1             : UNSIGNED(63 downto 0) := (others => '0');
    signal int_in2             : UNSIGNED(63 downto 0) := (others => '0');
    signal int_in3             : UNSIGNED(63 downto 0) := (others => '0');
    signal int_in4             : UNSIGNED(63 downto 0) := (others => '0');
    signal int_w1_r            : UNSIGNED(63 downto 0) := (others => '0');
    signal int_w2_r            : UNSIGNED(63 downto 0) := (others => '0');
    signal int_w3_r            : UNSIGNED(63 downto 0) := (others => '0');
    signal int_w4_r            : UNSIGNED(63 downto 0) := (others => '0');
    signal int_b               : UNSIGNED(63 downto 0) := (others => '0');
    signal int_out1            : UNSIGNED(63 downto 0) := (others => '0');
    signal int_out2            : UNSIGNED(63 downto 0) := (others => '0');
    signal int_out3            : UNSIGNED(63 downto 0) := (others => '0');
    signal int_out4            : UNSIGNED(63 downto 0) := (others => '0');


begin
-- ----------------------- Instantiation------------------


-- ----------------------- AXI WRITE ---------------------
    AWREADY_t <=  '1' when wstate = wridle else '0';
    AWREADY   <=  AWREADY_t;
    WREADY_t  <=  '1' when wstate = wrdata else '0';
    WREADY    <=  WREADY_t;
    BRESP     <=  "00";  -- OKAY
    BVALID    <=  '1' when wstate = wrresp else '0';
    wmask     <=  (31 downto 24 => WSTRB(3), 23 downto 16 => WSTRB(2), 15 downto 8 => WSTRB(1), 7 downto 0 => WSTRB(0));
    aw_hs     <=  AWVALID and AWREADY_t;
    w_hs      <=  WVALID and WREADY_t;

    -- write FSM
    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ARESET = '1') then
                wstate <= wrreset;
            elsif (ACLK_EN = '1') then
                wstate <= wnext;
            end if;
        end if;
    end process;

    process (wstate, AWVALID, WVALID, BREADY)
    begin
        case (wstate) is
        when wridle =>
            if (AWVALID = '1') then
                wnext <= wrdata;
            else
                wnext <= wridle;
            end if;
        when wrdata =>
            if (WVALID = '1') then
                wnext <= wrresp;
            else
                wnext <= wrdata;
            end if;
        when wrresp =>
            if (BREADY = '1') then
                wnext <= wridle;
            else
                wnext <= wrresp;
            end if;
        when others =>
            wnext <= wridle;
        end case;
    end process;

    waddr_proc : process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (aw_hs = '1') then
                    waddr <= UNSIGNED(AWADDR(ADDR_BITS-1 downto 0));
                end if;
            end if;
        end if;
    end process;

-- ----------------------- AXI READ ----------------------
    ARREADY_t <= '1' when (rstate = rdidle) else '0';
    ARREADY <= ARREADY_t;
    RDATA   <= STD_LOGIC_VECTOR(rdata_data);
    RRESP   <= "00";  -- OKAY
    RVALID_t  <= '1' when (rstate = rddata) else '0';
    RVALID    <= RVALID_t;
    ar_hs   <= ARVALID and ARREADY_t;
    raddr   <= UNSIGNED(ARADDR(ADDR_BITS-1 downto 0));

    -- read FSM
    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ARESET = '1') then
                rstate <= rdreset;
            elsif (ACLK_EN = '1') then
                rstate <= rnext;
            end if;
        end if;
    end process;

    process (rstate, ARVALID, RREADY, RVALID_t)
    begin
        case (rstate) is
        when rdidle =>
            if (ARVALID = '1') then
                rnext <= rddata;
            else
                rnext <= rdidle;
            end if;
        when rddata =>
            if (RREADY = '1' and RVALID_t = '1') then
                rnext <= rdidle;
            else
                rnext <= rddata;
            end if;
        when others =>
            rnext <= rdidle;
        end case;
    end process;

    rdata_proc : process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (ar_hs = '1') then
                    rdata_data <= (others => '0');
                    case (TO_INTEGER(raddr)) is
                    when ADDR_IN1_DATA_0 =>
                        rdata_data <= RESIZE(int_in1(31 downto 0), 32);
                    when ADDR_IN1_DATA_1 =>
                        rdata_data <= RESIZE(int_in1(63 downto 32), 32);
                    when ADDR_IN2_DATA_0 =>
                        rdata_data <= RESIZE(int_in2(31 downto 0), 32);
                    when ADDR_IN2_DATA_1 =>
                        rdata_data <= RESIZE(int_in2(63 downto 32), 32);
                    when ADDR_IN3_DATA_0 =>
                        rdata_data <= RESIZE(int_in3(31 downto 0), 32);
                    when ADDR_IN3_DATA_1 =>
                        rdata_data <= RESIZE(int_in3(63 downto 32), 32);
                    when ADDR_IN4_DATA_0 =>
                        rdata_data <= RESIZE(int_in4(31 downto 0), 32);
                    when ADDR_IN4_DATA_1 =>
                        rdata_data <= RESIZE(int_in4(63 downto 32), 32);
                    when ADDR_W1_R_DATA_0 =>
                        rdata_data <= RESIZE(int_w1_r(31 downto 0), 32);
                    when ADDR_W1_R_DATA_1 =>
                        rdata_data <= RESIZE(int_w1_r(63 downto 32), 32);
                    when ADDR_W2_R_DATA_0 =>
                        rdata_data <= RESIZE(int_w2_r(31 downto 0), 32);
                    when ADDR_W2_R_DATA_1 =>
                        rdata_data <= RESIZE(int_w2_r(63 downto 32), 32);
                    when ADDR_W3_R_DATA_0 =>
                        rdata_data <= RESIZE(int_w3_r(31 downto 0), 32);
                    when ADDR_W3_R_DATA_1 =>
                        rdata_data <= RESIZE(int_w3_r(63 downto 32), 32);
                    when ADDR_W4_R_DATA_0 =>
                        rdata_data <= RESIZE(int_w4_r(31 downto 0), 32);
                    when ADDR_W4_R_DATA_1 =>
                        rdata_data <= RESIZE(int_w4_r(63 downto 32), 32);
                    when ADDR_B_DATA_0 =>
                        rdata_data <= RESIZE(int_b(31 downto 0), 32);
                    when ADDR_B_DATA_1 =>
                        rdata_data <= RESIZE(int_b(63 downto 32), 32);
                    when ADDR_OUT1_DATA_0 =>
                        rdata_data <= RESIZE(int_out1(31 downto 0), 32);
                    when ADDR_OUT1_DATA_1 =>
                        rdata_data <= RESIZE(int_out1(63 downto 32), 32);
                    when ADDR_OUT2_DATA_0 =>
                        rdata_data <= RESIZE(int_out2(31 downto 0), 32);
                    when ADDR_OUT2_DATA_1 =>
                        rdata_data <= RESIZE(int_out2(63 downto 32), 32);
                    when ADDR_OUT3_DATA_0 =>
                        rdata_data <= RESIZE(int_out3(31 downto 0), 32);
                    when ADDR_OUT3_DATA_1 =>
                        rdata_data <= RESIZE(int_out3(63 downto 32), 32);
                    when ADDR_OUT4_DATA_0 =>
                        rdata_data <= RESIZE(int_out4(31 downto 0), 32);
                    when ADDR_OUT4_DATA_1 =>
                        rdata_data <= RESIZE(int_out4(63 downto 32), 32);
                    when others =>
                        NULL;
                    end case;
                end if;
            end if;
        end if;
    end process;

-- ----------------------- Register logic ----------------
    in1                  <= STD_LOGIC_VECTOR(int_in1);
    in2                  <= STD_LOGIC_VECTOR(int_in2);
    in3                  <= STD_LOGIC_VECTOR(int_in3);
    in4                  <= STD_LOGIC_VECTOR(int_in4);
    w1_r                 <= STD_LOGIC_VECTOR(int_w1_r);
    w2_r                 <= STD_LOGIC_VECTOR(int_w2_r);
    w3_r                 <= STD_LOGIC_VECTOR(int_w3_r);
    w4_r                 <= STD_LOGIC_VECTOR(int_w4_r);
    b                    <= STD_LOGIC_VECTOR(int_b);
    out1                 <= STD_LOGIC_VECTOR(int_out1);
    out2                 <= STD_LOGIC_VECTOR(int_out2);
    out3                 <= STD_LOGIC_VECTOR(int_out3);
    out4                 <= STD_LOGIC_VECTOR(int_out4);

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_IN1_DATA_0) then
                    int_in1(31 downto 0) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_in1(31 downto 0));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_IN1_DATA_1) then
                    int_in1(63 downto 32) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_in1(63 downto 32));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_IN2_DATA_0) then
                    int_in2(31 downto 0) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_in2(31 downto 0));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_IN2_DATA_1) then
                    int_in2(63 downto 32) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_in2(63 downto 32));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_IN3_DATA_0) then
                    int_in3(31 downto 0) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_in3(31 downto 0));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_IN3_DATA_1) then
                    int_in3(63 downto 32) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_in3(63 downto 32));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_IN4_DATA_0) then
                    int_in4(31 downto 0) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_in4(31 downto 0));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_IN4_DATA_1) then
                    int_in4(63 downto 32) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_in4(63 downto 32));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_W1_R_DATA_0) then
                    int_w1_r(31 downto 0) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_w1_r(31 downto 0));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_W1_R_DATA_1) then
                    int_w1_r(63 downto 32) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_w1_r(63 downto 32));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_W2_R_DATA_0) then
                    int_w2_r(31 downto 0) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_w2_r(31 downto 0));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_W2_R_DATA_1) then
                    int_w2_r(63 downto 32) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_w2_r(63 downto 32));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_W3_R_DATA_0) then
                    int_w3_r(31 downto 0) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_w3_r(31 downto 0));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_W3_R_DATA_1) then
                    int_w3_r(63 downto 32) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_w3_r(63 downto 32));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_W4_R_DATA_0) then
                    int_w4_r(31 downto 0) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_w4_r(31 downto 0));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_W4_R_DATA_1) then
                    int_w4_r(63 downto 32) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_w4_r(63 downto 32));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_B_DATA_0) then
                    int_b(31 downto 0) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_b(31 downto 0));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_B_DATA_1) then
                    int_b(63 downto 32) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_b(63 downto 32));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_OUT1_DATA_0) then
                    int_out1(31 downto 0) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_out1(31 downto 0));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_OUT1_DATA_1) then
                    int_out1(63 downto 32) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_out1(63 downto 32));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_OUT2_DATA_0) then
                    int_out2(31 downto 0) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_out2(31 downto 0));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_OUT2_DATA_1) then
                    int_out2(63 downto 32) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_out2(63 downto 32));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_OUT3_DATA_0) then
                    int_out3(31 downto 0) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_out3(31 downto 0));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_OUT3_DATA_1) then
                    int_out3(63 downto 32) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_out3(63 downto 32));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_OUT4_DATA_0) then
                    int_out4(31 downto 0) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_out4(31 downto 0));
                end if;
            end if;
        end if;
    end process;

    process (ACLK)
    begin
        if (ACLK'event and ACLK = '1') then
            if (ACLK_EN = '1') then
                if (w_hs = '1' and waddr = ADDR_OUT4_DATA_1) then
                    int_out4(63 downto 32) <= (UNSIGNED(WDATA(31 downto 0)) and wmask(31 downto 0)) or ((not wmask(31 downto 0)) and int_out4(63 downto 32));
                end if;
            end if;
        end if;
    end process;


-- ----------------------- Memory logic ------------------

end architecture behave;
