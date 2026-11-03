# SD 卡布局

板子从 FAT32 卡里读取它需要的一切。把本目录的内容复制到卡根目录：

```
/BOOT.BIN              FSBL + 位流 + 应用（由 boot_640.bif 经 bootgen 生成）
/system_wrapper_640.bit  PL 位流（走 JTAG 启动时用这一份）
/fsbl.elf             第一级引导（初始化 DDR），tools/flash.ps1 使用
/gauge_hdmi_true640.elf  应用 ELF，tools/flash.ps1 使用
/gauge_model.json     类别 / 锚点 / 输入尺寸 / 预处理说明
/gauge_scale.txt      表盘标定：min、max、unit、clockwise、angle_min、angle_max
/gauge_weights/       42 个 .bin（每层 w1..w4、b1..b4）+ manifest.json
/test1.bmp ...        可选的 24 位 BMP 测试图（尺寸任意，板上做 letterbox）
```

注意事项：

* **只认 24 位 BMP**（`bpp == 24`、`biCompression == 0`）。尺寸任意，固件会 letterbox 到
  640×640 并在内部转灰度，所以彩色图和灰度图结果一致。
* `testN.bmp` 必须从 1 连号——`find_img_count()` 碰到第一个缺号就停。
* 必须支持长文件名（`gauge_weights` 有 13 个字符）；文件系统驱动以 `FF_USE_LFN=1`、
  `FF_CODE_PAGE=437` 编译。
* `gauge_scale.txt` 可在不重编的情况下覆盖编译进去的标定。随工程发布时卡上是
  `min = 0`、`max = 16`、`unit = MPa`（实验台那块表）；本目录的
  `gauge_scale.example.txt` 里有两块表的完整示例和每个键的说明。删掉该文件即回到
  编译默认（`0..16 MPa`）。

  ```
  min       = 0
  max       = 16
  unit      = MPa
  clockwise = 1
  ```
  `angle_min` / `angle_max` 是可选的。它们是两个端点数字的方向（单位度，
  0 = +X，顺时针），从轴心量起；给了它们就不必依赖检测器去找刻度端点——
  在密集刻度盘上检测器并不可靠。
* `BOOT.BIN` 用
  `bootgen -image boot_640.bif -arch zynq -o BOOT.BIN -w on` 生成。

## 启动模式由卡的内容决定

固件不需要拨码开关，也不需要重编——它在启动时看一眼卡，自己选模式：

| 卡上的内容 | 模式 | 行为 |
| --- | --- | --- |
| 至少一张 `testN.bmp` | **静态图** | 静默：完全不初始化相机链路、不做 UART 握手，循环显示并读取这些图 |
| 没有 `testN.bmp` | **相机** | 初始化 OpenMV 链路，请求并应答每一帧，实时画面 + 读数 |

所以“卡上没图”才会让板子去驱动 OpenMV 相机，而丢一张 `test1.bmp` 进去就变成不依赖相机的
独立演示。两个分支都在 `main.cpp` 里（`find_img_count()` → `nimgs`）。

若没有 `gauge_scale.txt`，固件用编译默认（`0..16 MPa`），弧端点取检测到的 `minimum` /
`maximum` 标记。当刻度盘密集、检测器漏掉端点时，可以用 `tools/scale_from_labels.py`
从标注文件反推出那两个角度。

相机模式要求 OpenMV 真的在推流。如果屏幕上的照片和镜头看到的不一样，说明 OpenMV 跑的是
“文件回放”变体；`tools/openmv_source.py --set sensor --restart` 可以切回实时采集。
