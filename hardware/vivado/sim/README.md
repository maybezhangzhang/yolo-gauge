# 卷积加速器的 RTL 仿真

本目录是一套自包含的 Vivado Simulator 测试台，被测对象是打包好的加速器 IP
（`../hls/ip/conv640`，与位流里那份 Verilog 完全相同：`conv.v` 741,923 B，
MD5 `96CCAAAB97E91EBD6E1CEEC561253186`）。

之所以要有它：HLS 的协同仿真（`cosim_design`）驱动不了这套接口——原因见
`../../docs/verification.md` §5——所以改在 RTL 层直接验证算术和寄存器契约。

## 测试做什么

`tb_conv_pwconv.v` 例化 `conv`，并完全按 PS 驱动的时序去驱动它：

* 通过 `s_axi_CTRL` 窗口写六个标量寄存器（`ch_in`、`ch_out`、`fsize`、`stride`、`kernel`、
  `act`），再读回核对；
* 通过 `s_axi_CONTROL` 窗口写十三个 64 位指针（`in1..in4`、`w1..w4`、`b`、`out1..out4`），
  四个 in/w/out 指针都指向同一个基址，和 `basic_op.cpp` 的做法一致；
* 拉高 `ap_start`，轮询 `ap_done`，然后把每个输出字与测试台里算出的期望值逐一比较。

指针指向的存储器模型是 `axi4_slave_ram.v`——一个行为级 AXI4 从机，初值来自
`fm_init.hex`（特征图）与 `w1_init.hex`（权重）。

当前配置：**1×1 逐点卷积，ch_in 4 → ch_out 8，12×12，stride 1，kernel 1，bias 4.0**——
每个输出字都必须读回 `0x0800`。

## 期望结果

```
================================================================
 conv 1x1 pwconv testbench
   ch_in=4  ch_out=8  fsize=12  stride=1  kernel=1  act=0
   ...
  ap_done asserted after 822 polls, kernel time = 13.585 us
    checked 8 x 144 = 1152 elements
RESULT: PASS  (all outputs == 4.0 = 0x0800)
================================================================
```

`822` 是内核运行期间 CPU 发起的 AXI-Lite 读次数；按 100 MHz 折算就是上面报的 13.585 µs。
同一内核在板上约 1.5 ms，因为它跑在 DDR 上而不是零延迟的 RAM 模型——仿真证明的是正确性，
不是吞吐。

## 怎么跑

无界面（约一分钟，结果打印在 `sim.log` 里）：

```bat
run_sim.bat
```

带波形的 GUI 流程：

```tcl
# 在本目录下
vivado -source run_gui_sim.tcl      # elaborate + 打开仿真
vivado -source run_gui_wave.tcl     # 重新打开保存好的 tb_sim.wdb
```

`sim.log` 与 `sim_wave.log` 是保留下来的两次运行日志；`tb_sim.wdb` 是最后一次 GUI 运行的波形库。
`logwave.tcl` 是个小工具，用来把关心的信号（各指针、`ap_start`、`ap_done`、写通道）加进波形。

## 文件

| 文件 | 作用 |
| --- | --- |
| `tb_conv_pwconv.v` | 测试台：配寄存器、启动/轮询、比对输出 |
| `axi4_slave_ram.v` | 测试台用的行为级 AXI4 从机存储 |
| `fm_init.hex`、`w1_init.hex` | 特征图与权重的初值 |
| `filelist.f` | `xvlog` 的编译顺序 |
| `run_sim.bat` | 无界面流程：`xvlog` → `xelab` → `xsim -runall` |
| `run_gui_sim.tcl`、`run_gui_wave.tcl`、`logwave.tcl` | GUI 流程与信号导出 |
| `sim.log`、`sim_wave.log`、`events.log` | 最近的运行日志 |
| `tb_sim.wdb` | 波形库 |

生成物（`xsim.dir/`、`prj_gui/`、`.Xil/`、`*.pb`、`*_pid*.str`）已 gitignore，可随时由上面的脚本重建。
