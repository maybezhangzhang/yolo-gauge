# tools/

主机侧辅助脚本。板子运行并不需要这里的东西，它们只是省事。

| 脚本 | 作用 | 典型用法 |
| --- | --- | --- |
| `flash.bat` | `flash.ps1` 的双击包装 | 手动烧板 |
| `flash.ps1` | 位流 + FSBL + 应用，全程 JTAG，不需要 Vitis 图形界面 | 每次重编后 |
| `sdcard.ps1` | 重打 `sd/BOOT.BIN` 并把载荷写入读卡器 | 做 SD 卡时 |
| `openmv_source.py` | 经 REPL 在 `file` / `sensor` 模式间切换 OpenMV | 屏幕上是固定照片时 |

## flash.ps1 —— 一键烧录，不需要 Vitis

它做的事等价于 Vitis 里的 *Program FPGA* 加 *Run*，但走命令行，而且顺序是这块板子上真正能跑通的顺序：

1. `targets 1` —— 选中 DAP；
2. `rst -system`、`fpga -file system_wrapper_640.bit` —— 配置 PL；
3. `dow fsbl.elf`、`con`、等 3 秒、`stop` —— 由 FSBL 初始化 DDR、时钟和 MIO；
4. `rst -processor` —— **关键**：CPU 复位后 MMU 关闭，下载器按 1:1 地址访问，才能写完整镜像；
5. `dow gauge_hdmi_640.elf`、`rwr cpsr 0x1d3`、`con` —— **关键**：运行前屏蔽 IRQ/FIQ，
   否则 PL 挂起的中断会在 `main()` 之前跳进空向量表。

```powershell
cd E:\final_entry\tools

# 普通烧录：跑 20 秒后 halt
powershell -NoProfile -ExecutionPolicy Bypass -File flash.ps1

# 跑 25 秒，并把帧缓冲存成 fb.png（可直接当成果图）
powershell -NoProfile -ExecutionPolicy Bypass -File flash.ps1 -RunSeconds 25 -Capture

# 位流取 Vivado 的输出，而不是 sd/ 里那份
powershell -NoProfile -ExecutionPolicy Bypass -File flash.ps1 -Source build

# 下载随工程发布的 ELF，而不是刚编的
powershell -NoProfile -ExecutionPolicy Bypass -File flash.ps1 -App sd

# 只打印会用哪些文件
powershell -NoProfile -ExecutionPolicy Bypass -File flash.ps1 -List
```

| 参数 | 默认 | 含义 |
| --- | --- | --- |
| `-Source sd\|build` | `sd` | 位流 + FSBL 从哪取 |
| `-App build\|sd` | `build` | 下载哪个应用 ELF |
| `-RunSeconds N` | `20` | 跑多久后 halt（0 = 一直跑） |
| `-Capture` | 关 | 导出 `0x20100000`（1024×600×3）并转成 `fb.png` |
| `-DownloadOnly` | 关 | 下载并启动后立即返回 |
| `-List` | 关 | 只解析并打印路径 |
| `-Vitis` | `D:\Vitis\2020.2` | 工具安装位置 |

退出码：`0` 成功，`2` 失败（脚本会打印三条检查项：供电、JTAG 排线、DAP 锁死）。

## sdcard.ps1 —— 重打并写卡

```powershell
# 只重打 sd/BOOT.BIN
powershell -NoProfile -ExecutionPolicy Bypass -File sdcard.ps1 -BuildOnly

# 重打，然后把 BOOT.BIN、ELF、gauge_scale.txt 写到 H:
powershell -NoProfile -ExecutionPolicy Bypass -File sdcard.ps1 -Drive H

# 同上，并额外放两张 BMP 进卡（→ 静态图模式）
powershell -NoProfile -ExecutionPolicy Bypass -File sdcard.ps1 -Drive H `
    -Images 'E:\my_entry\gauge_training\test_bmps\swiss_1-13_5401.bmp'

# 用另一份标定文件
powershell -NoProfile -ExecutionPolicy Bypass -File sdcard.ps1 -Drive H -Scale .\my_scale.txt
```

它用相对路径的 `sd/boot_640.bif` 重新生成 `BOOT.BIN`（所以镜像可复现），旧文件留成 `*.prev`，
最后告诉你这张卡会进哪个模式（`0 test images -> CAMERA mode`）。

## openmv_source.py —— 相机为什么显示固定照片

OpenMV 的脚本既可以推传感器，也可以回放一张存好的 BMP：

```powershell
python openmv_source.py --port COM14                       # 查看当前模式
python openmv_source.py --port COM14 --set sensor          # 实时采集
python openmv_source.py --port COM14 --set file --restart  # 回放 mpa_src_qvga.bmp
```

`--restart` 会经 REPL 发 Ctrl-C 加 `exec(open('main.py').read())`，不拔线就能让改动生效。
它是按字节改而不是按行改，因为 `main.py` 是 GBK 编码。
