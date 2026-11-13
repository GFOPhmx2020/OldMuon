# GlashowRun 使用说明（详细参考手册）

> 2026-10-25 更新提示：当前默认输出为无损压缩 TSV.GZ，核出生记录移至首次跟踪回调，环境路径集中到 env.sh / local_env.sh，当前默认长度保留为 20 m。本文旧的输出文件名和实现细节可能尚未同步；最新操作、迁移设备和性能验证请以 README_先读我.md 为准。

> 适用目录：`glashow_starter/`
> 本文是**完整参考手册**；想按顺序快速上手，先读同目录下的 `README_先读我.md`。
> 环境：**Ubuntu/WSL 终端**（所有命令都是 bash，不要粘贴进 PowerShell 或 Jupyter Python 单元）。

---

## 1. 项目简介与物理目标

`GlashowRun` 是一套**单文件** Geant4 + PYTHIA 程序，研究**Glashow 共振**在岩盐（天然 NaCl）中的强子响应：

- 用 PYTHIA 生成 `ν̄e + e⁻ → W⁻ → 强子` 事件（W 只走强子衰变道，夸克 1,2,3,4,5）；
- 把质心系产生的强子 boost 回实验室系（入射中微子沿 +z）；
- 用 Geant4 在**岩盐圆柱**中输运整个强子簇射，记录每一次**核反冲**。

**输出的物理量是"条件响应"**：给定一次已发生的 W 强子衰变，材料里产生怎样的核反冲（能量谱、长度代理谱）。它**不是**通量权重预测，也**不是**可观测径迹数——不乘反电子中微子通量、地质年龄、保存概率或读出效率。

Glashow 共振：当 `E_ν ≈ m_W²/(2 m_e) ≈ 6.3 PeV` 时 s 通道 W 共振增强。程序允许 1–10 PeV，正好覆盖这个峰。

---

## 2. 目录结构

```text
glashow_starter/
├── GlashowRun.cc          # 主程序（唯一 C++ 源文件）
├── CMakeLists.txt         # CMake 构建脚本
├── build.sh               # 一键编译脚本（bash）
├── run.sh                 # 一键运行脚本（bash，设好数据路径）
├── analyze.py             # 后处理：画能谱 + 长度代理量谱
├── test_analyze.py        # 对 analyze.py 的单元测试
├── README_先读我.md         # 步骤式快速上手
├── USAGE.md               # 本文件（参考手册）
├── build/                 # CMake 构建产物，可执行文件 build/GlashowRun
├── results/               # 运行输出目录（示例样本、已验证样本）
├── vendor/pythia8317/     # 内置 PYTHIA 8.317 源码与构建（勿修改）
└── *.log                  # 构建/生成器/输运验证日志
```

| 文件 | 用途 | 需要编辑吗 |
|---|---|---|
| `GlashowRun.cc` | W 生成、NaCl 几何、Geant4 输运、核反冲记录 | 第一遍不用 |
| `CMakeLists.txt` | 生成可执行程序 | 不用 |
| `build.sh` / `run.sh` | 编译 / 运行包装脚本 | 不用 |
| `analyze.py` / `test_analyze.py` | 后处理 / 数值测试 | 不用 |
| `vendor/pythia8317/` | 官方 PYTHIA 8.317，目录内依赖 | 不用修改 |

---

## 3. 运行环境与依赖

- **操作系统**：Ubuntu / WSL（项目实际在 Windows 的 WSL 里跑）。
- **Geant4**：本机为 **11.2.0**，安装在 `/usr/local`（`/usr/local/bin/geant4.sh`）。
- **PYTHIA**：已内置在 `vendor/pythia8317/` 并构建成功，产物为
  - `vendor/pythia8317/lib/libpythia8.so`（及 `.a`）
  - `vendor/pythia8317/include/Pythia8/Pythia.h`
  - `vendor/pythia8317/share/Pythia8/xmldoc`（运行时数据）
  - 未安装到 `/usr/local`，不影响系统其他程序。
- **Python**（仅后处理/测试）：`python3` + `numpy`、`matplotlib`。用 WSL 的 `python3`（本机 Windows 的 `python` 缺 matplotlib）。

> 环境变量（`run.sh` 已自动设置）：`PYTHIA8DATA` 指向 `vendor/pythia8317/share/Pythia8/xmldoc`，`LD_LIBRARY_PATH` 加入 `vendor/pythia8317/lib`。

---

## 4. 构建

### 4.1 一键构建（推荐）

```bash
cd /mnt/d/GFOPhmx/Research_work/Old_Muon/primus-paleo/glashow_starter
bash build.sh
```

`build.sh` 会：
1. `source /usr/local/bin/geant4.sh`（加载 Geant4 环境）；
2. 检查 `vendor/pythia8317/lib/libpythia8.so` 是否存在，缺失则报错退出；
3. `cmake -S . -B build -DGeant4_DIR=... -DPYTHIA8_ROOT=$PWD/vendor/pythia8317`；
4. `cmake --build build -j8`。

**成功标志**：末尾出现 `Built target GlashowRun`。可执行文件为 `build/GlashowRun`。
出现 `error:` 或 `CMake Error` 时**不要**继续下一步，保留报错信息。

### 4.2 手动 cmake（等价）

```bash
source /usr/local/bin/geant4.sh
cmake -S . -B build \
    -DGeant4_DIR=/usr/local/lib/cmake/Geant4 \
    -DPYTHIA8_ROOT="$PWD/vendor/pythia8317"
cmake --build build -j8
```

> `CMakeLists.txt` 默认在 `${CMAKE_CURRENT_SOURCE_DIR}/vendor/pythia8317` 找 PYTHIA，也支持 `-DPYTHIA8_ROOT=/path/to/pythia` 覆盖。

### 4.3 换机器 / 重建 PYTHIA

当前机器已有 PYTHIA，无需重复。若换到没有本地构建的机器：

```bash
mkdir -p vendor && cd vendor
curl -fL https://pythia.org/releases/pythia83/pythia8317.tgz -o pythia8317.tgz
tar -xzf pythia8317.tgz
cd pythia8317
./configure --without-python
make -j2
cd ../..
bash build.sh
```

---

## 5. 命令行参考

### 5.1 用法

```text
build/GlashowRun energy_GeV events seed NEW_output_directory [radius_m length_m] [--generate-only]
```

等价的一键调用：

```bash
bash run.sh <energy_GeV> <events> <seed> <NEW_output_directory> [radius_m length_m] [--generate-only]
```

### 5.2 参数表

| 位置 | 参数 | 含义 | 约束 |
|---|---|---|---|
| 1 | `energy_GeV` | 入射 ν̄e 能量 (GeV) | **1e6 – 1e7**（即 1–10 PeV），有限数 |
| 2 | `events` | W 事件数 | ≥ 1 |
| 3 | `seed` | 随机种子 | 1 – 900000000 |
| 4 | `NEW_output_directory` | 输出文件夹 | **必须尚不存在** |
| 5（可选） | `radius_m` | 盐柱半径 | > 0，默认 10 |
| 6（可选） | `length_m` | 盐柱长度 | > 0，默认 200 |
| 末（可选） | `--generate-only` | 只产生事件、不做输运 | 放最后 |

**注意**：
- 输出目录**不能提前存在**，这是防覆盖的安全机制。重复运行请换新目录名，不要删除旧结果。
- `radius_m`/`length_m` 必须**成对**出现（4 参数或 6 参数，不允许单独给一个）。
- `--generate-only` 必须放在最后，识别后不计入参数个数。

### 5.3 退出码

| 退出码 | 含义 |
|---|---|
| 0 | 成功（`COMPLETE` / `GENERATION ONLY COMPLETE`） |
| 1 | 运行时错误（守恒失败、中止事件、IO 错误、PYTHIA 初始化失败等） |
| 2 | 用法错误（参数个数不对） |

---

## 6. 运行模式

### 6.1 完整输运（默认）

```bash
bash run.sh 6320000 1 12346 results/W_test01
```

- 用 Geant4 输运整簇射，记录核反冲与终止。
- 成功末尾：
  ```text
  Completed W event 1/1, recorded nuclei: ...
  COMPLETE: ...
  ```
- 一次 W 衰变可产生**很多**核反冲，`events=1` 不表示只有一个反冲核。PeV 簇射耗时长，**先跑 1 个事件看耗时**，再放大统计。

### 6.2 仅生成（`--generate-only`）

```bash
bash run.sh 6320000 10 12345 results/generator_check01 --generate-only
```

- 只调用 PYTHIA 生成并检查末态（能量/动量/电荷守恒、确认硬过程有 W⁻），**不做材料输运**。
- 成功末尾：`GENERATION ONLY COMPLETE; no material transport: ...`
- 产物只有 `primaries.tsv` 和 `run.json`（其 `completed_events=0` 是预期行为）。
- **不要对这类目录运行 `analyze.py`**（没有核反冲数据）。

---

## 7. 输出文件详解

完整输运结束后，`NEW_output_directory/` 内：

| 文件 | 内容 |
|---|---|
| `run.json` | 元数据：能量、种子、几何、事件数、版本、完成状态、物理/局限说明 |
| `pythia_settings.cmnd` | PYTHIA 实际使用的配置 |
| `primaries.tsv` | 每个 W 事件的初级强子四动量 |
| `events.tsv` | 事件账本（每完成事件一行，含无反冲事件） |
| `recoils.tsv` | 每个显式核反冲的出生信息 |
| `recoil_endings.tsv` | 对应反冲核的终点信息 |

### 7.1 `recoils.tsv`（核反冲"出生"记录）

列：`event_id  track_id  parent_id  pdg  z  a  energy_mev  x_mm  y_mm  z_mm  ux  uy  uz  time_ns  creator  parent_pdg  parent_z  parent_a`

| 字段 | 含义 |
|---|---|
| `event_id` / `track_id` | 事件号 / 该反冲核的轨迹号 |
| `parent_id` | 产生它的父轨迹号 |
| `pdg` `z` `a` | 反冲核的 PDG 码、电荷数、质量数 |
| `energy_mev` | **出生时**动能 (MeV) |
| `x_mm y_mm z_mm` | 出生位置 (mm) |
| `ux uy uz` | 出生动量方向（单位矢量） |
| `time_ns` | 出生时间 (ns) |
| `creator` | 产生该核的物理过程名 |
| `parent_pdg parent_z parent_a` | 父核的 PDG / Z / A |

记录条件（`Steps` 类）：父轨迹非初级、是该轨迹**第一步**、核粒子（z≥1, a≥2）、发生在 `Target` 体积内、出生动能 ≥ **0.1 keV**。

### 7.2 `recoil_endings.tsv`（反冲核"终点"记录）

列：`event_id  track_id  final_energy_mev  path_length_mm  x_mm  y_mm  z_mm  end_process  status`

| 字段 | 含义 |
|---|---|
| `final_energy_mev` | 轨迹终止时的动能 |
| `path_length_mm` | 累计路径长度 |
| `x/y/z_mm` | 终止位置 |
| `end_process` | 终止过程（如 `hIonisation`、`RadioactiveDecay`、`none`） |
| `status` | Geant4 轨迹状态码（`static_cast<int>(G4TrackStatus)`） |

> 与 `recoils.tsv` 通过 `(event_id, track_id)` 一一对应。

### 7.3 `events.tsv`（事件账本）

列：`event_id  energy_gev  n_primaries  n_recorded_nuclei  sum_primary_energy_gev  sum_primary_pz_gev`

每完成一个 W 事件写一行，`n_recorded_nuclei` 是归一化分母，**包括没有反冲的事件**。

### 7.4 `primaries.tsv`（初级强子）

列：`event_id  index  pdg  px_gev  py_gev  pz_gev  energy_gev`

W 强子化后在实验室系的完整末态（已 boost）。

### 7.5 `run.json`（元数据）

关键字段：`complete`（是否完整，**不完整时不要当样本用**）、`energy_gev`、`requested/completed_events`、`seed`、`radius_m`/`length_m`、`density_g_cm3`(2.16)、`record_threshold_keV`(0.1)、电子/W 质量与宽度、`max_primary_mass_difference_gev`、Geant4/PYTHIA 版本、物理列表、`neutron_time_limit_ns`(1000000)、`conditional_hadronic_W`、`vertex`、`limitations`。

> 手工中断、报错或未完成时 `complete` 保持 `false`。`.tsv` 可用文本编辑器/Excel 打开，但**保留原文件不被 Excel 改写**。

---

## 8. 后处理：`analyze.py`

```bash
python3 analyze.py <run_directory> [--range-dir <射程表目录>]
```

- `<run_directory>`：某次完整输运的输出目录（必须 `run.json` 中 `complete=true`，否则拒绝归一化）。
- `--range-dir`：岩盐射程表目录，默认 `Data/Geant4_data/Halite/Range`（即仓库根 `Data/Geant4_data/Halite/Range`，存放 `DEDX_Z{Z}_A{A}.txt`）。

### 8.1 做了什么

1. 读取 `events.tsv` 校验事件账本与元数据一致；
2. 读取 `recoils.tsv`，按核素分组：`Na23`、`Cl35`、`Cl37`、`other_heavy`（Z>2 的重核）、`light_ions`（Z≤2）；
3. **筛选**：只统计"非衰变产生、且父核 A≤4"的核出生记录（避免重核及其重核后代重复计数）——这是明确限定的诊断样本，**不是完整材料损伤模型**；
4. 能量谱：`recoils` 出生动能直方图（能量格点 `1e-4 – 1e10 MeV`，141 bins，对数）；
5. 长度代理谱：对 Na23/Cl35/Cl37 用岩盐射程表把能量 → 射程(nm)，再直方图（`0.1 – 1e9 nm`，151 bins）；
6. 所有谱按 **每个完成的 W 事件** 归一化。

### 8.2 产物（写在 `<run_directory>/analysis/`）

| 文件 | 内容 |
|---|---|
| `spectra.png` | 左：反冲核出生动能谱；右：Na/Cl 射程代理谱 |
| `response.npz` | bin 边界与各核素响应数组（后续 Python 复用） |
| `summary.json` | 分组计数、筛掉的记录、范围外计数 |

纵轴单位是 **count / hadronic W event / bin**，不是每 mg、每年或可见径迹数。一次事件统计极不稳定，不能直接用于论文结论。

> **右图有数据 ≠ 盐里留下了可见径迹**：没有缺陷形成概率、长期退火、腐蚀和显微镜效率，也未把高能碎片中途核反应做成完整损伤模拟。

---

## 9. 测试

```bash
python3 -m unittest -v test_analyze.py
```

覆盖 `analyze.py` 的关键数值函数：
- 零反冲事件仍留在归一化分母里；
- 射程单位换算与禁止越界外推；
- 溢出计数被报告而非偷偷重归一化；
- 非法分母被拒绝。

---

## 10. 物理设置与模型假设

| 项 | 设置 |
|---|---|
| 反应 | `ν̄e + e⁻ → W⁻ → 夸克(1,2,3,4,5)`（强子道） |
| 初态粒子 | PYTHIA `Beams:idA=-12`（ν̄e），`idB=11`（e⁻），`frameType=1` |
| 质心能量 | `eCM = √(m_e² + 2 m_e E_ν)`，随后 boost 回实验室系（沿 +z） |
| 初态辐射 | 关（`PartonLevel:ISR=off`） |
| 长寿命强子 | PYTHIA 不衰变寿命>1 ns 的粒子（`tau0Max=1e-9`），交给 Geant4 衰变表 |
| 几何/材料 | NaCl 圆柱，ρ=2.16 g/cm³，默认 r=10 m、L=200 m，顶点在柱心 |
| 强子物理 | `FTFP_BERT_HP`（含高精度中子） |
| 电磁物理 | `G4EmStandardPhysics_option4`（替换默认） |
| 能区上限 | 强子 & 电磁均设 20 PeV |
| 生产阈值 | `0.005 mm` |
| 中子截止 | 时间 1 ms、动能 0（慢中子完整输运） |
| 记录阈值 | 核反冲出生动能 ≥ 0.1 keV |

### 10.1 已知局限（写进 `run.json` 的 `limitations`）

- **自由静止电子**近似：未计入束缚/热运动对共振窗的影响。
- **PeV 强子模型未验证**：能区上限提到 20 PeV 不代表模型已验证。
- **重味衰变近似**：Geant4 的重味衰变实现是近似，物理解释前需替换/验证。
- **长度代理 ≠ 保存损伤**：射程代理只是能量→距离换算，不涉及缺陷/退火/蚀刻/效率。
- 中子的 1 ms 时间截止需做收敛检查。
- 本程序强制 W **强子**衰变；强子分支比只能乘一次，不能把它当全部 W 混合样本。

### 10.2 将来算"绝对径迹数"还缺什么

要回答"样品里最终能看到多少条"，还需：反电子中微子通量、样品/母岩几何、地质年龄、保存概率、读出效率，并按电子靶数与截面积分（`mineral_utils.py` 负责这部分宏观折叠）。

---

## 11. 常见问题 / 故障排查

| 现象 | 原因 / 处理 |
|---|---|
| `Output directory already exists; use a NEW output name.` | 输出目录已存在，换新目录名（勿删旧结果） |
| `Missing Geant4 particle PDG ...` | 某个 PDG 不在 Geant4 粒子表，记录报错信息 |
| `Primary four-momentum/charge conservation failed` | boost 或质量守恒被破坏，事件被丢弃并报错 |
| `PYTHIA initialization failed` | 检查 `vendor/pythia8317` 是否构建、`PYTHIA8DATA` 是否指向 xmldoc |
| `Executable missing. Run: bash build.sh` | 未编译，先跑 `bash build.sh` |
| `PYTHIA library missing.` | 缺 `libpythia8.so`，重建 PYTHIA（见 4.3） |
| 画图报 matplotlib 缺失 | 用 WSL 的 `python3`，不要用 Windows 的 `python` |
| 跑到一半中断/报错 | `run.json` 里 `complete=false`，不可当样本用，重跑请换输出目录 |
| 重复运行同参数 | 输出目录已存在 → 换目录名 |

---

## 12. 验证状态（截至 2026-10-25）

- 主程序在 Geant4 11.2.0 / PYTHIA 8.317 下**编译成功**；
- 10 次生成器事件通过程序内四动量/电荷守恒检查；
- 小靶（r=0.1 m, L=0.2 m）完整输运记录 161 条核出生记录，`analyze.py` 成功出图；
- `test_analyze.py` 4 项测试通过；
- 已验证样本：`results/verified_generator01`、`results/verified_small_target02`（**检查样本，非正式统计量**）；
- **尚未完成**：默认 r=10 m、L=200 m 大靶的完整验证；PeV 强子模型精度未经物理验证。

---

## 13. 参考链接

- PYTHIA 主页：<https://pythia.org/>
- PYTHIA 电弱过程手册：<https://pythia.org/latest-manual/ElectroweakProcesses.html>
- Geant4 FTFP_BERT 物理表：<https://geant4.web.cern.ch/documentation/dev/plg_html/PhysicsListGuide/reference_PL/FTFP_BERT.html>
