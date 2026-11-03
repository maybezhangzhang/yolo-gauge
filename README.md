# Zynq-7020 指针式仪表读数系统（640×640，四类目标）

裸机运行的模拟仪表检测与读数工程：在 Zynq-7020 上跑 YOLOv4-Tiny（四类：`base` 轴心、
`maximum` 刻度终点、`minimum` 刻度起点、`tip` 指针尖），由自主设计的 HLS 卷积 IP 加速。
应用把原图与检测框画到 HDMI（1024×600）上，并通过 UART1 打印各阶段耗时。

板上实测：**640×640 单帧 2.5 秒**（读图 133 ms、预处理 25 ms、推理 2516 ms、解码 5 ms）。

```
输入 640x640 RGB  ->  letterbox 到 640  ->  YOLOv4-tiny 主干（卷积 IP）
                  ->  out0 20x20x27（stride 32）+ out1 40x40x27（stride 16）
                  ->  解码 + NMS  ->  画到 HDMI 帧缓冲
```

## 目录结构

按 Xilinx 常规的 `hardware / software` 划分，每一段都能用对应工具单独打开与重建。

```
final_entry/
├─ README.md                        本文件
│
├─ hardware/                        ── 跑在 FPGA 上的东西 ──────────────
│  ├─ README.md                     三段重建顺序、资源情况
│  ├─ hls/                          加速器（Vitis HLS 2020.2）
│  │  ├─ src/                       conv / std_conv / pwconv / maxpool / upsample
│  │  ├─ ip/conv640/                打包好的 IP：xilinx.com:hls:conv:0.0（框图引用它）
│  │  ├─ build_hls.tcl              C 综合；HLS_EXPORT=1 时再尝试打包 IP
│  │  └─ README.md                 数据契约、tile 移位缺陷、两个 LUT 数字的差异
│  ├─ vivado/                       位流工程（不保存二进制，脚本重建）
│  │  ├─ create_project.tcl         生成 prj/yolo_gauge.xpr（框图 + IP 仓库 + 约束）
│  │  ├─ build_bitstream.tcl        综合 → 实现 → write_bitstream → ../sd
│  │  ├─ srcs/                      框图 system.bd、IP 实例、约束 xdc
│  │  ├─ ip_repo/DVI_TX/            自建 DVI 发送 IP（显示通路）
│  │  ├─ sim/                       conv IP 的 RTL 测试台 + 波形（run_sim.bat）
│  │  ├─ reports/                   布线后资源 / 时序 / 功耗报告
│  │  └─ README.md
│  └─ vitis/                        PS 端裸机应用
│     ├─ gauge_hdmi_640/
│     │  ├─ src/                    主循环、检测驱动、解码、OSD、CV 指针、读数
│     │  │  ├─ display_ctrl_hdmi/   HDMI 控制器（VDMA/VTC）
│     │  │  └─ vdma_api/            VDMA 封装
│     │  └─ build/                  build.ps1（手工编译脚本）+ ffconf.h（FatFs 配置）
│     ├─ bsp/                       头文件 + libxil.a + conv_v0_0 驱动 + FatFs 配置
│     └─ README.md
│
├─ software/                        ── 主机侧 ─────────────────────────
│  └─ training/                     PyTorch 训练与权重导出
│     ├─ train.py                   训练入口（GAUGE_INPUT_SIZE 决定尺寸）
│     ├─ auto_anchors.py            按尺寸重聚类 6 个锚点
│     ├─ kmeans_for_anchors.py      原始 k-means 实现（参考）
│     ├─ export_deploy.py           导出 ONNX + deploy_spec.json
│     ├─ export_true640.py          导出板上用的 42 个 w/b .bin + manifest
│     ├─ evaluate_gauge.py          测试集 mAP
│     ├─ make_test_bmp.py           任意图 → 板端 24 位 BMP
│     ├─ yolo.py                    推理封装（单图/批量）
│     ├─ nets/  utils/              网络定义与训练/评估工具
│     ├─ model_data/                类别表 + 640/416 两套锚点
│     └─ train.bat                  一键训练包装
│
├─ sd/                              ── SD 卡载荷（内容直接拷到卡根）──────
│  ├─ BOOT.BIN                      FSBL + 位流 + 应用（boot_640.bif 生成）
│  ├─ boot_640.bif                  bootgen 输入（相对路径，可复现）
│  ├─ fsbl.elf  system_wrapper_640.bit  gauge_hdmi_true640.elf
│  ├─ gauge_model.json              类别 / 锚点 / 输入尺寸 / 预处理
│  ├─ gauge_scale.example.txt       标定示例（两块表）
│  ├─ gauge_weights/                42 个 w/b .bin + manifest.json
│  ├─ test1.bmp … test3.bmp         24 位 BMP 测试图（有图 → 静态图模式）
│  └─ README.md
│
├─ tools/                           ── 一键脚本 ───────────────────────
│  ├─ flash.ps1 / flash.bat         位流 + FSBL + 应用，全程 JTAG
│  ├─ sdcard.ps1                    重打 BOOT.BIN 并写卡
│  ├─ openmv_source.py              切换 OpenMV 的实时采集 / 文件回放
│  └─ README.md
│
├─ skill/                           ── 可复用技能包（本项目）───────────
│  ├─ SKILL.md                      流程、检查清单、失败模式
│  ├─ scripts/                      decode_framebuffer / scale_from_labels / check_sd_payload
│  ├─ references/pitfalls.md        十个坑的长文 + 证据
│  └─ README.md
│
├─ skill_pynq/                      ── 通用 PYNQ 技能（任意 HLS 加速器）─
│  ├─ SKILL.md
│  ├─ scripts/                      pynq_verify / pynq_sweep_shift / pynq_bench
│  ├─ references/pynq_notes.md
│  └─ README.md
│
└─ docs/                            ── 文档与证据 ─────────────────────
   ├─ rebuild_flow.md               逐步重建：HLS → Vivado → 固件 → BOOT.BIN → 上板
   ├─ design_description.md         功能 / 架构 / 接口 / 软硬件划分 / 优化依据 / 基线对比
   ├─ verification.md               四级验证 + 仿真与实现报告
   ├─ board_bringup.md              内存映射、启动模式、CPSR/MMU 两个坑、UART 日志
   ├─ ai_collaboration.md           协作记录：提示 → 结果 → 纠错轨迹
   ├─ DESIGN_REPORT_CN.pdf/.docx    参赛设计报告（17 页）
   └─ screenshot_*.png              板上实拍
```

`hardware/vivado/prj/`、`software/training/logs_*/`、编译产物与 `*.dcp/*.jou/*.log` 都不入库，
由上面的脚本重新生成（见 `.gitignore`）。

## 0. 用 Vivado / Vitis 重建硬件

工程按 Xilinx 常规划分组织，每一段都能用对应工具单独打开与重建（细节见 `hardware/README.md`）：

```
hardware/hls      vitis_hls -f build_hls.tcl        -> ip/conv640  (VLNV conv:0.0)
hardware/vivado   vivado -mode batch -source create_project.tcl
                  vivado -mode batch -source build_bitstream.tcl
hardware/vitis    powershell -File gauge_hdmi_640/build/build.ps1
tools             powershell -File flash.ps1 -RunSeconds 20
```

Vivado 工程不保存任何二进制产物：`create_project.tcl` 会从 `srcs/`（框图 + IP 实例）和两个
IP 仓库（`hardware/hls/ip`、`hardware/vivado/ip_repo`）重新生成 `prj/yolo_gauge.xpr`，
并校验框图——已在 Vivado 2020.2 上实测通过。

## 1. 训练（PyTorch）

`software/training/` 是用于训练该模型的 [yolov4-tiny-pytorch] 分支：

```
cd software/training
py -3 train.py            # 640x640，120 轮，mosaic 0.5，余弦学习率
py -3 evaluate_gauge.py   # 在测试集上算 mAP
py -3 export_deploy.py --size 640   # 可选：导 ONNX + deploy_spec.json
```

* 数据集：`meiter_analog_clean`（训练/验证/测试 = 2007 / 250 / 263 张，全部 640×640）
* 锚点：`model_data/yolo_anchors_meiter_640.txt`，`anchors_mask = [[3,4,5],[0,1,2]]`
* 随工程发布的权重（`ep120`）测试集 mAP@0.5：**62.8 %**
  （base 99.99 / maximum 75.78 / minimum 53.06 / tip 22.29）

`software/training/export_true640.py` 把 checkpoint 转成板端权重格式（见下节）并写出 `gauge_model.json`。

## 2. 板端权重

IP 读取 **int16** 权重，激活为 `ap_fixed<16,7,AP_RND,AP_SAT>`（1 位符号 + 6 位整数 + 9 位小数，
LSB = 1/512，范围 ±64）。`export_true640.py` 写的是原始 float32 张量，板端加载时统一乘 512，
所以板上的数值域是 `round(x * 512)`。

SD 卡上：

```
/gauge_weights/          42 个 .bin（每层 w1..w4 / b1..b4）+ manifest.json
/gauge_model.json        类别、锚点、输入尺寸、预处理说明
/test1.bmp ...           可选的静态测试图（24 位 BMP，尺寸任意）
```

必须支持长文件名（`gauge_weights` 有 13 个字符），因此
`hardware/vitis/gauge_hdmi_640/build/ffconf.h` 里设了 `FF_USE_LFN 1` 与 `FF_CODE_PAGE 437`。

## 3. 编译应用

Vitis 的 `app build` 对本工程会静默地什么都不做，所以目标文件由脚本手工编译并链接：

```
powershell -NoProfile -ExecutionPolicy Bypass -File hardware\vitis\gauge_hdmi_640\build\build.ps1
```

Vitis 安装路径、合并 BSP 的头文件目录、生成的平台里的 `libxil.a`，都在脚本开头可配置。

## 4. 上板运行

```
powershell -NoProfile -ExecutionPolicy Bypass -File tools\flash.ps1 -RunSeconds 20
```

它会下载 `system_wrapper_640.bit`、运行 FSBL（初始化 DDR）、下载应用，并在 20 秒后把帧缓冲
导出到 `tools/fb.bin`。另有 `flash.bat` 可直接双击；每个参数的含义见 `tools/README.md`。

有两个容易踩的步骤，脚本里已经内置：

* 用 `rst -processor`，**不要**用 `rst -system`（后者会把 DDR 控制器一起复位）；
* `con` 之前把 `cpsr` 置为 `0x1d3`，否则 PL 侧挂起的中断会在 `main()` 之前跳进空向量表。

`test1.bmp`（SWISS MOVEMENT 表）上的预期结果：

| 类别 | 置信度 | 框（640 像素坐标） |
|-------|-------|--------------|
| base | 0.998 | 299.0, 292.0, 344.5, 336.2 |
| maximum | 0.985 | 475.8, 457.5, 501.3, 481.9 |
| minimum | 0.972 | 140.1, 460.9, 166.1, 487.5 |
| tip | 0.562 | 83.6, 186.5, 101.5, 202.1 |

若要独立启动，用 `sd/boot_640.bif` 生成 `BOOT.BIN`
（`bootgen -image boot_640.bif -arch zynq -o BOOT.BIN -w on`），并把板子的启动模式从 JTAG 改成 SD。

固件会自己根据卡内容选模式，所以同一份 `BOOT.BIN` 覆盖两种场景：卡上至少有一张 `testN.bmp`
时静默跑静态图循环（完全不初始化相机链路、不握手）；一张都没有时切到相机模式，消费 OpenMV
经 UART3 推来的帧。细节与相机排障见 `docs/board_bringup.md` 和 `sd/README.md`；
`tools/openmv_source.py` 可查看或切换 OpenMV 的实时 / 回放模式。

## 5. 读数（优先用 CV 指针线，YOLO 的 tip 框只做交叉校验）

`hardware/vitis/gauge_hdmi_640/src/gauge_reading.h` 负责把几何关系换算成读数，
`hardware/vitis/gauge_hdmi_640/src/gauge_config.cpp` 提供当前表盘的标定。刻度数字不再写死在代码里：

* `span = (amax - amin) mod 2π`、`pos = (atip - amin) mod 2π`、
  `value = vmin + (vmax - vmin) * pos / span`。端点留 5° 容差，指针正好停在刻度端点时也能读。
* `vmin` / `vmax` / `unit` / `clockwise` 来自 `g_gauge_scale`。编译默认是 `0 .. 16`、单位 `MPa`
  （实验台上的那块表）。SD 卡上的 `gauge_scale.txt` 可在不重编的情况下覆盖它；删掉该文件即回到编译默认。
* `amin` / `amax` 是两个端点数字的方向。**检测到 `minimum` / `maximum` 框时**用框算，
  否则用 `gauge_scale.txt` 里的 `angle_min` / `angle_max`。密集刻度盘上必须钉死这两个角度：
  Tomsk 表上检测器会把 `minimum` 认到**右上角**（它咬住了 `10`/`12`），那样整条刻度会转 180°。
* `base` 框中心是轴心，始终必需。

标定文件（`gauge_scale.txt`，复制到 SD 卡根目录；所有键可选，`#` 开头是注释）：

```
min       = 0
max       = 16
unit      = MPa
clockwise = 1
# angle_min / angle_max 可选：只有在刻度盘密集、检测器把 minimum/maximum 认错时才需要给。
```

底部横幅会打印标定来源后缀（`cfg` = 角度来自文件，`det` = 角度来自检测框），标定错了当场可见。

`hardware/vitis/gauge_hdmi_640/src/pointer_cv.cpp` 里的射线搜索**每帧都跑**，而不是只在 tip 框不可信时
才跑——过轴心的黑色直线比 20×13 的框稳得多，两者会同时画在屏幕上，一旦不一致一眼就能看出来。
检测器的 `tip` 框仍会画（青色），其角度与直线的差值写进日志。只有射线搜索失败时，才退回用框读数
（`source=yolo`）：

* 从轴心向 360 个方向发射射线，搜索范围从 `r_in`（轴心帽外一点点）到 `0.92 * radius`，
  其中 `radius = min(|min-base|, |max-base|)`；
* 每条射线按“比表盘底色暗多少”打分，底色取 360 条射线中最亮四分位的均值，胜者在 ±1° 内以 0.05° 细化；
* 判定条件：`score > 0.12` **且** 越过轴心后的连续暗像素长度超过搜索跨度的四分之一。

选中的指针线用品红画出并标注 `CV needle`；底部横幅与 UART 同时给出读数、角度与来源：

```
GAUGE_NEEDLE angle_cdeg=31900 score_milli=707 run_px=158 ok=1
GAUGE_NEEDLE tip_delta_cdeg=2 tip_score_milli=562
GAUGE_READING value_centi=1091 angle_cdeg=31900 source=cv arc=det
READING 10.91 MPa | 319 deg | cv/det          <- HDMI 上的横幅
```

在 Swiss-Movement 测试表上，CV 找到的指针线与标注的针尖相差 0.6°，读数与 float 参考差 0.01 bar。
在 Tomsk 表上，CV 射线（319°，暗线 158 px）明显压过反方向（139°，94 px）——配重比指针短，
这正是“最长暗线”判据可靠的原因。

## 6. 容易踩的坑

* **3×3 卷积输出沿 W 方向整体循环移位了一个 tile。** HLS 的 `std_conv` 把输出整体旋转了 `Tc` 个像素
  （640 版 `Tc = 10`，旧的 416 版是 `13`）。`hardware/vitis/gauge_hdmi_640/src/basic_op.cpp` 在
  `k == 3` 时把它补偿回来；否则网络输出是垃圾（与 PC 参考的相关系数从 0.985 掉到 0.40，NMS 后一个框都不剩）。
* **`load_weight` 不能静默失败。** 权重文件缺失时程序会卡在 `read_params` 里死循环；看 UART 是否打印
  `FATAL: weight file not readable`。
* **不要相信独立的 `csynth.rpt`。** 它给加速器报 LUT 77 211 = 145 %，但同一份 IP 在 Vivado 工程里做
  OOC 综合只有 35 122 LUT = 66 %，布线后整板 77 % 收敛。发布位流里确实是这份 IP（用 `conv.v` 的
  MD5 校验过）。详见 `hardware/hls/README.md`。
* 应用只支持 640：权重、锚点、`anchors_mask` 与两个输出头（20×20 / 40×40）都按 640 输入写死。

## 7. 已知限制

* 单帧 2.5 秒——瓶颈在 IP，不在 CPU。
* `tip` 是最弱的一类（AP 22.3 %，测试图上 0.56），目标最小（约 20×13 px）且贴在细指针上。
  这正是读数取自 CV 直线、检测框只做校验的原因。
* 静态图循环偶尔会在第二帧卡在卷积 IP 里；`basic_op.cpp` 里的看门狗约 20 秒后打印
  `CONV TIMEOUT:` 并从卡死中恢复。给 PL 断电重配可以清掉。

## 8. 许可证

本项目以 MIT 许可证发布，见根目录 `LICENSE`；可自由使用、修改与再分发，保留版权与许可声明即可。
如需专利授权条款，可改为 Apache-2.0。