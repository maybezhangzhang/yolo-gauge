# Skill：Zynq 上的指针式仪表读数（检测 + HLS 加速 + 读数）

本目录是一个**可复用的技能包**：把本仓库里"如何把这条流水线点亮、调试、扩展"的经验固化下来，
形式可以在本项目之外继续使用。

| 文件 | 用途 |
| --- | --- |
| `SKILL.md` | 面向智能体的定义：适用面、流程、检查清单、失败模式 |
| `scripts/decode_framebuffer.py` | 把 `mrd -bin` 抓到的帧缓冲转成 PNG |
| `scripts/scale_from_labels.py` | YOLO 标注文件 → `gauge_scale.txt`（弧端点角度 + 预期读数） |
| `scripts/check_sd_payload.py` | 插卡前校验（BMP 格式、42 个权重、图片连号） |
| `references/pitfalls.md` | 失败模式表的长文版，附证据 |

## 适用范围

适用：Zynq-7000 / MPSoC 上**裸机**运行的"小目标检测 + 几何读数"类作品，PS 通过
AXI-Lite 逐层驱动一个 HLS 卷积 IP，结果显示到 HDMI 帧缓冲，图片来自 SD 卡。

不适用：需要每层动态调度的推理引擎（应把调度 FSM 放进 PL）；要求 <100 ms 的
实时场景（PS 轮询与逐层指针写入会成为瓶颈）；运行期换模型（tile 与量化契约已编译进
驱动）。

## 使用方法

1. 先读 `SKILL.md` 里的 "The order that works"：**先证 IP、再证位流、再证单层、最后跑整网**。
2. 三个脚本可直接用：

```bat
python skill\scripts\check_sd_payload.py E:\final_entry\sd
python skill\scripts\scale_from_labels.py labels\img_0001.txt --min -1 --max 3 --unit bar
python skill\scripts\decode_framebuffer.py tools\fb.bin fb.png
```

3. 按 `SKILL.md` §3.2 的 tcl 序列烧写；`flash.ps1` 已封装这段流程。

## 失效条件

* 换了 HLS 版本或改了 `Tr/Tc/Tm` 参数 → tile 移位补偿量必须同步改（本工程 640 版是
  10 px，旧的 416 版是 13 px），否则整网"跑完但零框"。
* 换了 FatFs 配置（关掉 LFN 或改代码页）→ 权重目录读不到，固件会停在错误屏。
* 系统日期 ≥ 2026 时 HLS 2020.2 的 `export_design` 会因 revision 溢出而失败
  （见 `references/pitfalls.md`），只能沿用已打包的 IP。
* Windows + HLS 2020.2 的 C/RTL 协同仿真对"16 路 m_axi + offset=slave"接口会
  SIGSEGV，属工具限制，不是设计问题；用板上逐层 dump 取代。

## 验证效果

这份 skill 的每条结论都在本机上复现过，证据类型与结果：

| 结论 | 证据 | 结果 |
| --- | --- | --- |
| 加速器本身可用 | 裸机 ELF 直接配寄存器、数 `ap_done` | 1×1/3×3/maxpool 全部 done（1,435,304 / 91,046 / 102,479 次轮询） |
| 位流与源码一致 | `conv.v` MD5 比对 | `96CCAAAB…` 与打包 IP 相同 |
| 数据契约正确 | 板上单层输出 vs numpy | 相关 0.98（补偿后），0.40（补偿前） |
| 整网可用 | HDMI 抓帧 | `det 4`，四类框正确 |
| 读数正确 | 板端 vs PC 参考 | 0.05 bar vs 0.036 bar（差 0.01） |
| 换表不改代码 | SD 卡 `gauge_scale.txt` | Tomsk 表读出 10.83 kgf/cm²（参考 10.91） |

对应的完整证据在 `docs/verification.md` 与 `hardware/vivado/reports/`。
