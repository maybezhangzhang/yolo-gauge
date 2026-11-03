# hardware/vivado

Zynq-7020 仪表读数系统的 Vivado 工程。工程本身是**由脚本从本目录的源码生成**的，
所以这里除了 IP 仓库不保存任何二进制产物。

```
create_project.tcl     从 srcs/ + 两个 IP 仓库生成 prj/yolo_gauge.xpr
build_bitstream.tcl    综合 -> 实现 -> write_bitstream -> 输出到 ../sd
srcs/                  框图（system.bd 及其 IP 实例）与约束
ip_repo/DVI_TX/        DVI 发送 IP（自建 IP，不在 Vivado 自带库里）
reports/               布线后的资源 / 时序 / 功耗报告
outputs/               生成的 .bit / .xsa 的落地目录（已 gitignore）
```

## 重建

```
cd hardware/vivado
vivado -mode batch -source create_project.tcl
vivado -mode batch -source build_bitstream.tcl
```

`create_project.tcl` 在打开设计之前先注册两个 IP 仓库：

* `../hls/ip`   —— 打包好的 HLS 加速器（`xilinx.com:hls:conv:0.0`）；
* `./ip_repo`   —— `DVI_TX`，显示通路使用。

如果找不到 `conv:0.0`，框图就打不开；用 `hardware/hls/build_hls.tcl` 重新打包
（版本号必须保持 `0.0`）。

## 框图内容

* `processing_system7_0` —— PS7、DDR3、SD、UART1、HDMI 时钟
* `conv_0` —— HLS 加速器，1 个 AXI4-Lite 控制口 + 16 个 AXI4 主口
  （4 路激活、4 路权重、4 路输出，bias 复用 W1）
* `axi_vdma_0` + `v_tc_0` + `v_axi4s_vid_out_0` + `DVI_Transmitter_0` —— 显示通路
* 5 个 `xbar` / `axi_mem_intercon` —— 把加速器的 16 个主口扇出到 HP0/HP1

## 本版位流的结果（xc7z020clg400-2）

| 项目 | 数值 |
|------|-------|
| Slice LUT | 40 984 / 53 200（77.0 %） |
| Slice Register | 56 243 / 106 400（52.9 %） |
| Block RAM | 101 / 140（72.1 %） |
| DSP48E1 | **213 / 220（96.8 %）** |
| WNS @100 MHz | +0.398 ns（TNS = 0） |
| WNS @150 MHz | +0.530 ns |
| 功耗 | 2.772 W（动态 2.584 + 静态 0.188） |

原始 Vivado 报告在 `reports/`。
