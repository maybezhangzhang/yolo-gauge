# 卷积加速器（HLS）

`src/` 是板端应用所驱动 `conv` IP 的 HLS 源码。它在同一套 AXI-Lite 寄存器接口后面实现了四种算子：

| `kernel` | 运算 |
|----------|-----------|
| 3 | 3×3 卷积（`std_conv.cpp`） |
| 1 | 1×1 逐点卷积 / pwconv（`pwconv.cpp`） |
| 2 | 2×2 最大池化（`maxpool.cpp`） |
| 0 | 2 倍上采样（`upsample.cpp`） |

顶层签名（`conv.h`）：

```c
void conv(data_t* in1, data_t* in2, data_t* in3, data_t* in4,
          data_t* w1,  data_t* w2,  data_t* w3,  data_t* w4,
          data_t* b,
          data_t* out1, data_t* out2, data_t* out3, data_t* out4,
          int ch_in, int ch_out, int fsize, int stride, int kernel, int act);
```

`data_t` 为 `ap_fixed<16,7,AP_RND,AP_SAT>`：1 位符号 + 6 位整数 + 9 位小数
（LSB = 1/512，饱和范围 ±63.999）。权重以 int16 传入，已经乘过 512。

## 数据契约（移植驱动时必须遵守）

* 四个 `in*` 指针必须是**同一个**地址；第 `c` 个通道位于 `base + c * fsize * fsize`，
  第 2..4 个平面分别是 `+1*size, +2*size, +3*size`；
* 四个 `w*` 指针同址，权重按 OIHW 扁平存放；
* 四个 `out*` 指针同址，输出通道线性排布；卷积后的尺寸是 `fsize/stride`，由 tile 逻辑向下取整；
* bias 在 MAC 循环之前被装进累加缓冲，也就是说它同样在 16 位域里参与累加。

## tile 移位缺陷（重要）

`std_conv` 写出的结果沿 W 方向整体旋转了一个 tile 宽度。tile 宽度来自头文件宏：
416 版 `Tc = 13`，本 640 版 `Tc = 10`。应用侧把每一行 3×3 输出向左循环平移 `Tc` 个像素来补偿
（`hardware/vitis/gauge_hdmi_640/src/basic_op.cpp`，搜 `k == 3`）。没有补偿时，卷积输出与 PC
参考的相关系数只有 0.40；补偿后是 0.985。

一旦改 `std_conv.h` 里的 `Tr`/`Tc`，软件侧那个常量必须同步改。

另外注意：`pwconv.h` 与 `std_conv.h` 都定义了 `Tm` 和 `MAX_LEN`（分别是 `8/1024` 与 `32/512`）。
`conv.h` 先包含 `pwconv.h`，所以最终生效的是 `std_conv` 的值，`pwconv.cpp` 实际以 `Tm = 32` 编译，
而它的内层索引是按 `Tm = 8` 写的。结果仍是确定的，只是浪费了一些工作；改动这两个文件时要留意。

## 能不能放进器件

`ip/` 里打包好的这份 IP 就是框图中实例化的那份，它是放得下的：布线后 LUT 77.0 %、DSP 96.8 %。

同一份源码存在两个不同的数字，值得知道原因：

| 报告 | LUT | DSP | 说明 |
|------|-----|-----|-------|
| `solution1/syn/report/csynth.rpt`（独立 csynth） | 77 211（145 %） | 194（88 %） | 悲观：IO 无约束、没有 OOC 上下文 |
| Vivado 工程内的 OOC 综合（`conv_0` OOC） | 35 122（66.0 %） | 213（96.8 %） | 这才是真正被实现的版本 |

工程对 IP 做的是 out-of-context 综合，Vivado 才能吸收那些很宽的 m_axi 端口；
**不能用独立 `csynth` 报告判断是否放得下**。`../sd/` 里的位流正是由这份 IP 生成的：
`sd_bmp_hdmi.gen/.../ipshared/5d06/` 里的 `conv.v` 与 `ip/conv640/hdl/verilog/` 里那份逐字节相同
（MD5 `96CCAAAB97E91EBD6E1CEEC561253186`）。

即便如此，设计仍是 DSP 受限的：不降低并行因子（`Tm`、`Tr`/`Tc`、`MAX_LEN`），
已经塞不下第二个 3×3 引擎。

重新打包 IP：

```
cd hardware/hls
vitis_hls -f build_hls.tcl                       # 只做 C 综合（总是执行）
set HLS_EXPORT=1 && vitis_hls -f build_hls.tcl   # 另外尝试重新打包 IP
```

导出时的版本号必须保持 `0.0`：框图实例化的是 `xilinx.com:hls:conv:0.0`。

### 为什么打包做成了可选项

Vitis HLS 2020.2 用 `YYMMDDHHMM` 生成 IP core revision。系统日期在 2026 年时得到类似
`2611030830` 的值，超出 32 位有符号整数，打包器直接中止：

```
bad lexical cast: source type value could not be interpreted as target
    while executing "rdi::set_property core_revision 2611030830 {component component_1}"
ERROR: [IMPL 213-28] Failed to generate IP.
```

这个版本的 `export_design` **没有** `-revision` 选项。所以 `ip/conv640` 里的包就是权威产物：
它与发布位流所用的网表完全一致（`hdl/verilog/conv.v`，741,923 B，
MD5 `96CCAAAB97E91EBD6E1CEEC561253186`）。如果确实要重新打包，可临时把系统时钟调到 2021 年，
导出完再改回来。
