# hardware/vitis

仪表读数系统的裸机应用（PS 端）。

```
gauge_hdmi_640/src      C/C++ 源码（主循环、检测驱动、解码、OSD、CV 指针）
gauge_hdmi_640/build    build.ps1 + ffconf.h，产物在 build/out/
bsp/include             合并后的 standalone BSP 头文件（取自生成的平台）
bsp/lib/libxil.a        standalone BSP 库
bsp/drivers/conv_v0_0   与 IP 配套的加速器驱动（xconv.c / xconv_hw.h）
bsp/ffcfg/ffconf.h      构建所用的 FatFs 配置：FF_USE_LFN=1、FF_CODE_PAGE=437
```

## 构建

```
cd hardware/vitis/gauge_hdmi_640/build
powershell -NoProfile -ExecutionPolicy Bypass -File build.ps1
```

脚本只要求装了 `Vitis 2020.2`（工具链 + xilffs 源码）；BSP 头文件与 `libxil.a`
都随仓库提供，所以构建不依赖某个已生成的工作区。

为什么不用 Vitis 图形界面：在 Vitis 2020.2 里 `app build` 会先把平台库编出来，
然后**静默跳过应用**；而且 `conv` 驱动的 Makefile 里那条 recipe 以 `\t#echo` 开头，
`SHELL=CMD` 无法执行。脚本用同一套工具链编同样的源码、链接同一个 `libxil.a`，
产物等价。

## 如果你还是想要一个 Vitis 工程

```
# 在 Vitis IDE 或 xsct 里：
setws .
platform create -name meiter640_plat -hw ../sd/system_wrapper_640.xsa
platform active meiter640_plat
domain create -name standalone_ds -proc ps7_cortexa9_0 -os standalone
domain active standalone_ds
bsp setlib -name xilffs
bsp write
bsp regenerate
app create -name gauge_hdmi_640 -platform meiter640_plat -domain standalone_ds -template {Empty Application}
importsources -name gauge_hdmi_640 -path ./gauge_hdmi_640/src
```

之后要么用 `build.ps1`（见上）构建，要么先把驱动 Makefile 的 recipe 修好
（`#echo` 改成 `@echo`）再用 `app build`。

## 运行时注意

* `lscript.ld` 必须给 `_HEAP_SIZE = 0x10000000`（256 MB）与
  `_STACK_SIZE = 0x2000000`（32 MB）；默认 8 KB 的堆装不下检测器对象和它的静态缓冲。
* UART1 为 115200 8N1，走板载 CH340（每次复位后 COM 号可能变化）。
* 应用从 FAT32 的 SD 卡读 `/gauge_weights/*.bin`、`/gauge_model.json`、
  `/gauge_scale.txt` 与 `/test1.bmp ...`。
