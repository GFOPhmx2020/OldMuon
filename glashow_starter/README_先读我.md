# W 强子簇射与岩盐核反冲：运行、输出与迁移

本目录为独立程序：PYTHIA 生成 W− 的强子末态，Geant4 在 NaCl 中输运，保存核出生与终点。原项目 Data/Geant4_data 的 C++ 应用没有被修改。模拟条件是“已经发生一次 W 强子衰变”，不包含宇宙通量、地质年龄或保存效率。当前 PeV 物理模型仍待验证，不能因为程序跑完就认为物理准确。

> 从仓库根目录先执行 `export PROJECT_ROOT="$PWD"`。历史 results 与 vendor 不随源码发布；依赖需自行安装，历史验证记录不代表本次重新验证。
> **当前默认启用 1 MeV 次级粒子截断，详见末节。下文早期优化记录中的“不杀掉次级粒子”仅适用于关闭该截断时。**

## 1. 优化记录与默认值

- Release 编译优化，不使用 fast-math，不降低浮点输出精度。
- 默认输出 .tsv.gz：gzip level 1 无损压缩，256 KiB 写入缓冲。保留全部字段和 17 位有效数字，解压内容与普通 TSV 相同。
- 保留上一版内存优化：只给存活的核轨迹携带母粒子信息，随轨迹释放。
- 核出生改为首次跟踪时记录，取消全体粒子的逐步用户回调；暂停后恢复不会重复记录。
- 不再为没有核子代的轨迹重复构造母信息；关闭 PYTHIA 的逐事件屏幕列表，但仍保存完整初级粒子表。
- Python 脚本自动读取旧 TSV 或新压缩 TSV，无须解压。同一表同时存在两种格式时拒绝猜测，以免读取旧数据。
- 本项目维护的 C++、Python、Shell、CMake 均补充了中文注释；不修改第三方 vendor 源码。
- 环境路径集中到 env.sh；换设备只需编辑 local_env.sh。

没有为了提速杀掉电子、光子或中子，没有提高 0.1 keV 记录阈值，没有缩短中子时间窗，也没有抽样或删除核记录。初级粒子表很小，保留它便于复现与守恒检查。压缩主要降低磁盘占用，速度以实测为准，Geant4 簇射仍可能很慢。

当前默认几何：半径 10 m、长度 20 m；编译默认并行数为 8。早期运行曾用长度 200 m，各次真实几何看 run.json。建议明确传入半径和长度，以免混淆。

实测中靶（相同种子）：原版约 155 s，最终压缩版约 158 s，最终普通 TSV 版约 153 s；耗时相近，不能宣称显著提速。压缩数据由约 25.3 MiB 降至 10.8 MiB，节省约 57%。三个版本的四份数据表解压后逐字节一致。详细证据见《优化验证_20261025.md》。

## 2. Linux / WSL 运行步骤

以下在 Ubuntu / WSL 的 Bash 终端执行，不是 PowerShell 或 Jupyter Python 单元。

~~~bash
cd "$PROJECT_ROOT/glashow_starter"
bash build.sh
~~~

看到 Built target GlashowRun 后先运行小靶：

~~~bash
bash run.sh 6320000 1 12347 results/my_new_small01 0.1 0.2
python3 analyze.py results/my_new_small01
~~~

参数顺序：能量_GeV、事件数、随机种子、新输出目录、可选的半径_m和长度_m、可选的 --generate-only。

6320000 = 6.32 PeV；1 是一个 W 事件，不是一条核反冲。输出目录必须用新名字，程序拒绝覆盖。

中靶示例（半径 0.5 m、长度 1 m）：

~~~bash
bash run.sh 6320000 1 12346 results/my_new_medium01 0.5 1
python3 analyze.py results/my_new_medium01
~~~

大靶示例（显式指定半径 10 m、长度 20 m，同时保存终端日志）：

~~~bash
set -o pipefail
bash run.sh 6320000 1 12346 results/my_new_bulk01 10 20 2>&1 | tee my_new_bulk01.log
~~~

如要复现以前的 200 m 长靶，末尾改为 10 200。大靶一个事件也可能处理数亿到十亿级轨迹，不要立即增加统计。

成功标准：终端出现 COMPLETE，run.json 中 complete=true，且 completed_events=requested_events。之后才能用 analyze.py 归一化。

进度检查约每 30 秒一次（每 100000 次轨迹调度才检查，所以可能更久）。tracked 是轨迹调度次数；recorded nuclei 是核出生记录数；live genealogy 是存活谱系对象数。它们不是完成百分比。

只检查生成器：

~~~bash
bash run.sh 6320000 10 12345 results/my_generator01 --generate-only
~~~

这个模式没有材料输运，completed_events=0 正常，不能用它画核反冲谱。

## 3. 输出文件与关键信息

| 文件 | 内容 |
|---|---|
| run.json | 版本、能量、种子、几何、完成状态、物理设置和压缩格式 |
| pythia_settings.cmnd | 实际生成器设置 |
| primaries.tsv.gz | 初级末态 PDG、动量和总能量（GeV） |
| events.tsv.gz | 完成事件账本，包括没有反冲的事件 |
| recoils.tsv.gz | 事件/轨迹/母编号、PDG、Z/A、出生动能 MeV、位置 mm、方向、时间 ns、产生过程、母粒子 PDG/Z/A |
| recoil_endings.tsv.gz | 对应核的终点、最终动能、累计路径、结束过程和状态 |

出生与终点用 (event_id, track_id) 关联。累计路径不等于直线位移，也不等于长期保存的可见损伤径迹。程序没有记录全部微观步的轨迹点序列。

gzip 为无损压缩，不减少条数或精度。手工查看前几行：

~~~bash
python3 -c 'import gzip; from itertools import islice; print("".join(islice(gzip.open("results/my_new_small01/recoils.tsv.gz", "rt"), 5)))'
~~~

如外部程序需要普通 TSV，可以解压一份（会额外占磁盘），或只对下一次运行关闭压缩：

~~~bash
GLASHOW_COMPRESS=0 bash run.sh 6320000 1 12347 results/my_plain01 0.1 0.2
~~~

该变量只接受 0 或 1。local_env.sh 如果固定了此变量，以其中设置为准。旧运行结果不会自动被压缩。

## 4. 完整结果和中断结果怎样分析

完整结果：

~~~bash
python3 analyze.py results/my_new_small01
~~~

输出在该目录的 analysis 下：spectra.png、summary.json、response.npz。

- 左图是核出生能谱（MeV），分 Na23、Cl35、Cl37、其他重核和轻离子。
- 右图使用原岩盐射程表换算 Na/Cl 的射程近似（nm），不是读取 Geant4 累计路径。
- 纵轴为每个完成 W 强子事件、每个 bin 的平均计数，没有除 bin 宽度，也不是每 mg 的可见径迹数。
- 目前只画非衰变产生、母粒子 A≤4 的诊断样本，raw 表保留全部记录。
- 射程表范围外单独计数，不做静默外推。
- analyze.py 为严格查重仍保留编号集合和能量数组；超大样本分析仍需较多内存，gzip 不解决全部内存问题。

中断数据用低内存逐行统计：

~~~bash
python3 audit_partial.py results/my_new_bulk01 partial_summary.json
~~~

中断的 gzip 可能缺少尾部；脚本尽可能恢复已写出的前缀，在 read_warnings 中明确标记，并排除截断行。CRC 损坏仍报错。它不生成每个完整 W 的产额，也不做重复编号审计。

中断前的数据受轨迹栈处理顺序影响，不是完整事件的无偏样本。终点记录可能缺失。run.json 在生成/事件边界更新，中断时部分统计不是最终值。定期刷新输出不等于断点续算，本程序不支持事件中途恢复运行。

## 5. 各文件的作用

| 文件 | 用途 |
|---|---|
| GlashowRun.cc | 主模拟；各类的中文注释说明材料、生成器、事件、谱系与首次跟踪核记录 |
| OutputFile.hh | 缓冲与无损压缩；检查关闭错误，写失败时不标 complete=true |
| CMakeLists.txt | 发现依赖、Release 编译、C++ 测试 |
| env.sh | 共用环境加载和路径配置 |
| local_env.example.sh | 新设备配置模板，复制为 local_env.sh 后修改 |
| build.sh | 编译程序；JOBS 只影响编译，不是模拟线程数 |
| run.sh | 加载环境并执行，保留真实退出码 |
| data_io.py | 新旧文本读取、中断压缩前缀恢复 |
| analyze.py | 完整模拟的能谱和射程近似图 |
| audit_partial.py | 中断数据描述统计 |
| check_regression.py | 比较两次同参数完整运行的解压后内容 |
| test_analyze.py | 分母、单位、射程边界检查 |
| test_data_io.py | 旧文件兼容、gzip、中断和 CRC 检查 |
| test_output.cc | C++ 输出往返、缓冲刷新和磁盘写入失败检查 |

检查命令：

~~~bash
source ./env.sh
ctest --test-dir "$GLASHOW_BUILD_DIR" --output-on-failure
python3 -m unittest -v test_analyze.py test_data_io.py
python3 check_regression.py results/io_before_medium results/io_final_medium
~~~

最后一行要求两个样本还在且均完成。IDENTICAL 只证明输出一致，不证明物理模型准确。不同 Geant4/PYTHIA 版本或编译器不保证相同种子逐位复现。

## 6. 复制到其他设备：要带哪些东西

建议复制整个 primus-paleo，但可以不带巨大的 glashow_starter/results。必须保留本目录源码和脚本，以及原项目 Data/Geant4_data/Halite/Range 中的射程数据。

Geant4 没有打包在项目里，新设备必须另有兼容安装及运行时物理数据，尤其 HP 中子数据。当前实际验证为 Geant4 11.2.0、PYTHIA 8.317。另需 C++17 编译器、CMake、zlib 开发库和带 numpy/matplotlib 的 Python3。

现有脚本面向 Linux / Windows WSL Bash。不要直接把旧 Linux 可执行文件拿到 Windows 原生或 macOS 上运行；其他平台没有验证，应在新环境重新编译依赖与主程序。

### 第一步：创建环境配置，不改 C++ 路径

在新设备进入复制后的目录，例如：

~~~bash
cd /home/yourname/projects/primus-paleo/glashow_starter
cp local_env.example.sh local_env.sh
nano local_env.sh
~~~

取消需要的行前的 #，改为新设备真实路径。例如：

~~~bash
export GEANT4_SH="/opt/geant4-install/bin/geant4.sh"
export Geant4_DIR="/opt/geant4-install/lib/cmake/Geant4"
export PYTHIA8_ROOT="/home/yourname/software/pythia8317"
export PYTHIA8DATA="$PYTHIA8_ROOT/share/Pythia8/xmldoc"
export GLASHOW_BUILD_DIR="build_this_device"
export JOBS=8
export GLASHOW_COMPRESS=1
~~~

这些是示例，不能照抄一个不存在的 /opt 路径：

- GEANT4_SH 指向 geant4.sh，它加载 Geant4 库和数据环境。
- Geant4_DIR 是包含 Geant4Config.cmake 的目录，不是文件本身或源码目录。有些安装在 lib64 或带版本号的子目录。通常指定 GEANT4_SH 后即可发现安装前缀；发现失败时明确设置 Geant4_DIR。
- PYTHIA8_ROOT 下应有 include/Pythia8/Pythia.h，以及 lib 或 lib64 中的库。
- PYTHIA8DATA 指向实际 XML 数据目录。
- GLASHOW_BUILD_DIR 用新名字，避开旧机器 CMake 缓存里的绝对路径。更换依赖路径后也应使用新构建目录。
- JOBS 内存少时可改为 2，只影响编译并行。模拟目前仍串行。

可以在已知的安装父目录下查找：

~~~bash
find /opt /home/yourname/software -name geant4.sh -o -name Geant4Config.cmake
~~~

若只指定 Geant4_DIR 而不指定 GEANT4_SH，必须自行提前加载 Geant4 运行时数据环境。找不到数据时应修复安装或环境，不能指向空目录。

本机不创建 local_env.sh 时，脚本会从 PATH 或 /usr/local/bin/geant4.sh 加载，PYTHIA 默认使用本项目 vendor/pythia8317。

### 第二步：新设备上的 PYTHIA

已有相容安装可直接指定 PYTHIA8_ROOT。若用项目里的官方源码压缩包，建议解压到新目录并构建，避免旧设备的 .o/.so 混入：

~~~bash
mkdir -p vendor_this_device
tar -xzf vendor/pythia8317.tgz -C vendor_this_device
cd vendor_this_device/pythia8317
./configure --without-python
make -j2
cd ../..
~~~

将 local_env.sh 的 PYTHIA8_ROOT 改为该新构建目录的绝对路径。没有压缩包时另取官方 PYTHIA 8.317 源码。这里仅用 C++ 接口，不需要 PYTHIA Python 绑定。

### 第三步：Python 依赖和主程序

新设备若缺 Python 依赖，可创建独立环境：

~~~bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install numpy matplotlib
bash build.sh
~~~

若当前 Python 环境已有 numpy/matplotlib，可跳过重复安装。CMake 若提示缺少 zlib，应安装新系统的 zlib 开发包；Ubuntu/Debian 通常叫 zlib1g-dev。

### 第四步：小样本验证

~~~bash
source ./env.sh
ctest --test-dir "$GLASHOW_BUILD_DIR" --output-on-failure
python3 -m unittest -v test_analyze.py test_data_io.py
bash run.sh 6320000 1 12347 results/new_device_small01 0.1 0.2
python3 analyze.py results/new_device_small01
~~~

若只复制本目录，原射程数据放在别处，画图时指定：

~~~bash
python3 analyze.py results/new_device_small01 --range-dir /home/yourname/data/Halite/Range
~~~

先完成这些检查再扩大计算。修改版本或物理设置后必须重新验证。

## 7. 当前仍需解决的物理问题

- 输入端检查了能量、动量、电荷守恒；还需完整的 Geant4 接收端核对及全事件能量收支。
- 全局上限 20 PeV 不保证各模型都覆盖或验证到该能量：实际日志中部分电磁模型仍显示 100 TeV、轻子核作用模型显示 1 PeV，需要逐项检查。
- FTFP_BERT_HP 的高能外推、重味强子简化衰变仍需独立基准。
- 低能中子、5 微米 production cut、0.1 keV 记录阈值和 1 ms 中子时间窗需要收敛验证。
- 射程表不是晶格损伤模型，没有退火、腐蚀和读出效率。
- 核出生动能不是独立能量沉积，直接累加可能重复统计级联能量。

当前结果用于程序检查和探索，不能直接当作古岩盐可见径迹的可靠预测。本次性能优化不解决上述物理问题。

## 8. 空间清理与验证记录

输出主要在 results/某次运行目录。确认不再需要逐核数据后可删除对应目录，不影响源程序。删除整个 results 会丢掉全部模拟和测试样本；汇总报告不能恢复原始记录。

不要把 vendor、Geant4 安装和原项目 Data 当成输出删除。旧数据不会自动压缩。

上一版中靶内存优化：峰值从 1,607,432 KiB 降至 105,948 KiB，均记录 82,370 个核，四份 TSV 一致。限时大靶测试不代表完整事件验证。

本次速度、压缩率和一致性测试结果见同目录《优化验证_20261025.md》。编译或测试通过不等于物理准确性验证。
# 次级粒子动能截断：2026-10-30 更新

默认在出生后删除动能低于 **1 MeV** 的所有 Geant4 次级粒子，包括中子、电子、光子和反冲核；初级粒子不受影响。恰好等于阈值保留，通过筛选的轨迹之后减速到阈值以下仍继续输运。这是跟踪截断，不是禁止物理模型产生粒子。

低能核不再进入出生和终点表。此近似也会改变后续高能产物和反冲谱，不能当作完整、无偏的反冲模拟。丢弃动能不会自动计为材料沉积能量。原先文中“完整输运”等描述仅适用于关闭此新增截断的情况。

WSL 中重新编译后，原命令默认启用 1 MeV 截断：

```bash
bash build.sh
bash run.sh 6320000 1 12346 results/cut_1mev
# 关闭新增截断，恢复此前输运配置：
GLASHOW_SECONDARY_MIN_MEV=0 bash run.sh 6320000 1 12346 results/cut_off
# 修改为 0.5 MeV：
GLASHOW_SECONDARY_MIN_MEV=0.5 bash run.sh 6320000 1 12346 results/cut_half
```

每次使用不存在的新输出目录。run.json 新增 secondary_min_kinetic_mev（阈值）、killed_secondaries（累计删除数）、killed_secondary_kinetic_mev（累计删除动能，MeV）。这些统计在事件结束时更新，强行中断可能只留下上次更新值；generate-only 不输运，删除数为零。

