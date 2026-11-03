# Skill：PYNQ 加速器点亮（通用）

一套与具体板卡无关、也不依赖厂商示例的做法：在 PYNQ 上用 Python 把**任意** HLS/BD 加速器跑起来，
证明它算出的数与 numpy 参考一致，并测出它的速度。

| 文件 | 用途 |
| --- | --- |
| `SKILL.md` | 流程：idle 检查 → DMA 安全缓冲 → 配置/启动/轮询 → 比对 → 测速 → 契约扫描 |
| `scripts/pynq_verify.py` | 与 numpy 参考做单层等价性比对（板端模式 + 离线 dump 的 `--offline` 模式） |
| `scripts/pynq_sweep_shift.py` | 扫描整数循环移位，找出写回偏移；纯 numpy，任何机器都能跑 |
| `scripts/pynq_bench.py` | 缓冲只分配一次后的稳态 ms/iter，并检查是否有状态残留 |
| `references/pynq_notes.md` | 真正会坏的五件事，按它们出问题的顺序排列 |

## 适用范围

适用：手上有一个 `.bit` + `.hwh` 和一份寄存器映射，想在 PYNQ 上用 Python 把真实数据送进
加速器、并与软件参考逐值对齐；也适用于在把算法搬进裸机固件**之前**先验证 IP。

不适用：需要层间动态调度的网络（调度应进 PL）；硬实时（Python 每次寄存器写 10–100 µs）；
需要 >256 MB 连续 DMA 缓冲（改用带物理分配的驱动）。

## 使用方法

```bash
python scripts/pynq_verify.py --bit design_1.bit --map conv_0.json \
       --input in.npy --weights w.npy --bias b.npy --ref ref.npy --tol 1e-2
python scripts/pynq_bench.py --bit design_1.bit --map conv_0.json \
       --input in.npy --iters 50 --out-shape 32,320,320
python scripts/pynq_sweep_shift.py --got board_out.npy --ref ref.npy --row 320 --max-shift 32
```

寄存器映射是唯一需要按工程改写的东西（`ctrl` / `data` 两组偏移 + `ip` 名字）。
`pynq_verify.py --offline --got a.npy --ref b.npy` 可以脱离板子，用 JTAG dump 做同样的判定。

## 失效条件

* `.bit` 与 `.hwh` 不同源 → overlay 能加载但寄存器映射是错的，所有读写都"成功"而结果全错。
* 缓冲区只留一个引用（或在 DMA 期间被 GC）→ 第一次正确、之后随机。
* 忘记 `flush()` / `invalidate()` → 结果依赖上一条指令的缓存状态，表现为"时对时错"。
* IP 的 `done` 早于 `idle` → 下一次 `start` 被静默丢弃，表现为周期性长时间卡顿。

## 验证效果

| 检查 | 通过判据 |
| --- | --- |
| 起始 `ap_ctrl` | `4`（idle） |
| 结束后 | `2`（done）再 `4`（idle） |
| 单层对齐 | corr > 0.95 且 max_abs_err < tol |
| 重复运行 | 输出完全一致 |
| 吞吐 | 多次迭代稳定（循环内不再分配） |

这套判据在原始工程上抓出过一个"整网能跑、但 3×3 输出沿行方向偏移一个 tile"的契约 bug：
单层相关性 0.40 → 补偿 10 像素后 0.98，整网随即从 0 框变成 4 框。
