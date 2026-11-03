# hardware/

所有跑在 FPGA 上的东西。三个子目录按 Xilinx 常规流程划分，都可用对应工具直接打开：

```
hardware/
  hls/       Vitis HLS 源码 + 框图实例化所用的打包 IP
  vivado/    Vivado 工程：框图、约束、IP 仓库、报告
  vitis/     PS 端应用（裸机 C/C++）、其 BSP 头文件与构建脚本
```

## 重建顺序

1. **HLS IP**（只有改加速器时才需要）

   ```
   cd hardware/hls
   vitis_hls -f build_hls.tcl          # 重新综合并把 conv 重新打包成 IP
   ```

   产物落在 `hardware/hls/ip/conv640`，Vivado 工程通过 `ip_repo_paths` 自动引用。

2. **Vivado 位流**

   ```
   cd hardware/vivado
   vivado -mode batch -source create_project.tcl
   vivado -mode batch -source build_bitstream.tcl
   ```

   `create_project.tcl` 生成 `prj/yolo_gauge.xpr`、注册两个 IP 仓库、导入
   `srcs/sources_1/bd/system/system.bd`、校验框图并重新生成 wrapper。
   `build_bitstream.tcl` 随后跑综合、实现和 `write_bitstream`，并把结果复制到
   `../sd/system_wrapper_640.bit`。

3. **PS 应用**

   ```
   cd hardware/vitis/gauge_hdmi_640/build
   powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
   ```

   Vitis 图形界面可以按普通应用工程打开这些源码，但在 Vitis 2020.2 里本设计的
   `app build` 会静默地什么都不做，所以脚本直接驱动 `arm-none-eabi-g++`，
   并链接 `hardware/vitis/bsp` 里的 BSP。

4. **运行 / 启动**

   ```
   powershell -NoProfile -ExecutionPolicy Bypass -File ../../../tools/flash.ps1 -RunSeconds 20
   ```

   或者把 `sd/*` 拷到 FAT32 的 SD 卡，把板子切到 SD 启动。

## 资源情况

设计是 **DSP 受限**的：布线后 **213 / 220 个 DSP48E1（96.8 %）**、
**40 984 / 53 200 LUT（77.0 %）**，100 MHz 下 WNS +0.398 ns，整板 2.77 W。
不缩小 `Tm`/`Tr`/`Tc` 的话，已经塞不下第二个 3×3 引擎了。
