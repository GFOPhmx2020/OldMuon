# OldMuon：矿物中的宇宙线与核反冲模拟

本仓库基于上游 PRImuS / primus-paleo，包含矿物响应分析、束流模拟、大气缪子空间模拟、Glashow 共振强子末态输运和可视化工具。仓库已发布至 `GFOPhmx2020/OldMuon`（私有），供获得授权的合作者使用。以下命令面向 Linux / WSL Bash。

## 1. 按研究目标选择入口
项目1，2，3是我们最近研究的项目。其它的来自另一个项目库https://github.com/cgalelli/primus-paleo 
感谢！！
| 研究目标 | 入口 | 类型 |
|---|---|---|
| 岩盐长期曝光与径迹谱 | `MSC_analysis_new.ipynb`、`MSC_analysis_local.ipynb` | 读取预计算数据 |
| 橄榄石样品与宇宙线历史 | `CDP_analysis_new.ipynb` | 读取预计算数据 |
| 石英覆盖层历史 | `RSC_analysis.ipynb` | 读取预计算数据，存在已知缺表问题 |
| 飞行运输产生的贡献 | `Estimate_flight_contribution.ipynb` | 分析并保存结果 |
| 旧六列数据沿 z 的核出生分布 | `muon_stalactite.ipynb` | 分析及 PyROOT 绘图 |
| 单能缪子、质子或离子照射立方体 | `Data/Geant4_data/muon_beam` | 项目1.惠州束流 |
| 大气缪子照射圆柱与空间取样 | `Data/Geant4_data/muon_stalactite_full` | 项目2.缪子钟乳石 |
| W 强子末态在岩盐中的级联 | `glashow_starter` | 项目3.RockCube |
| Glashow 出生点与直线射程显示 | `glashow_starter/html_recoils` | 读取已有结果 |
| 原框架响应表与射程表生成 | `Data/Geant4_data/run_geant.ipynb` | 数据生成，不是日常分析入口 |
| 显微图像径迹识别 | `Test_optimusprimus.ipynb` | 可选模块，当前缺图像与模型 |

`mineral_utils.py` 与 `flux_history.py` 是供 Notebook 导入的核心模块，不需要直接运行。材料定义位于 `Data/mineral_library.yaml`。

## 2. 获取项目与准备环境

完成 GitHub 登录后：

```bash
git lfs install
git clone https://github.com/GFOPhmx2020/OldMuon.git
cd OldMuon
export PROJECT_ROOT="$PWD"
python3 -m venv .venv
source .venv/bin/activate
python -m pip install numpy scipy pandas matplotlib mendeleev PyYAML tqdm jupyter
python -m ipykernel install --user --name oldmuon --display-name 'Python (OldMuon)'
```

Geant4、ROOT 需要单独安装并加载环境。安装路径由每位合作者自行指定，例如：

```bash
# 先将变量设为本机安装目录，再执行；不要照抄他人的安装路径
source "$GEANT4_PREFIX/bin/geant4.sh"
source "$ROOT_PREFIX/bin/thisroot.sh"
python -c 'import ROOT; print(ROOT.gROOT.GetVersion())'
```

## 3. 岩盐、橄榄石、石英与飞行贡献 Notebook

从仓库根目录启动：

```bash
cd "$PROJECT_ROOT"
jupyter notebook
```

打开目标 Notebook，选择 `Python (OldMuon)` 内核，检查参数后按顺序运行。不要依赖旧单元输出判断本次是否成功。

| Notebook | 先检查的参数 | 主要输入 |
|---|---|---|
| `MSC_analysis_new.ipynb` | Halite、年龄、曝光窗口、沉积与覆盖层历史；当前主要计算 fast 情景 | Halite各粒子响应、StdRock响应、射程、核数据、默认通量 |
| `MSC_analysis_local.ipynb` | 同上，包含 fast/slow 情景 | 同上 |
| `CDP_analysis_new.ipynb` | Olivine、样品年龄、1 km高度、超新星与地磁情景 | Olivine/StdRock响应、射程、核数据、对应通量缓存 |
| `RSC_analysis.ipynb` | Quartz、覆盖层变化、300 kyr年龄 | Quartz/neutron、StdRock、Quartz射程等 |
| `Estimate_flight_contribution.ipynb` | 材料、飞行时间、高度；当前为Olivine、1小时、11 km | Olivine/StdRock响应、11 km通量缓存 |

响应文件为 `Data/Geant4_data/<材料>/<粒子>/outNuclei_*.txt`；几何文件为同目录的 `geometry.json`；射程表为 `<材料>/Range/DEDX_Z*_A*.txt`。`StdRock` 描述覆盖层响应，也是必需输入。`Data/nuclear_data` 提供核背景数据，`Data/flux_data` 提供通量缓存。

输出通常显示在 Notebook 中。飞行 Notebook 最后会写入 `Data/tracks_<材料>_1hr<高度>km_flight.npz`，重新运行会更新该文件。

**数据限制：**Git克隆只包含实际提交的文件，历史运行输出不一定随仓库分发。缺响应表时须向维护者取得原数据，不能删掉对应计算单元后将结果称为完整复现。当前已知 `RSC_analysis.ipynb` 执行曾在缺少 `Quartz/Range/DEDX_Z4_A4.txt` 处失败；不可拿其他材料的射程表替代。当前Quartz目录只有neutron响应，不能据此声称包含完整直接缪子响应。

## 4. 旧数据的 z 分布：muon_stalactite.ipynb

入口：[专用说明](README_muon_stalactite.md)。从根目录打开 `muon_stalactite.ipynb`，首先检查：

- `MATERIAL`、`PARTICLE`：当前为Olivine、mu-。
- `N_Z`：z分箱数。
- `N_PRIMARY_PER_FILE`：仅在知道每个文件初级粒子数时设置；默认不做每初级归一化。
- `GEOMETRY_OVERRIDE`：通常读取原 `geometry.json`，勿用不匹配的几何覆盖。

输入为六列 `outNuclei_*.txt`。输出位于 `output/muon_stalactite/<材料>/<粒子>/`，包含CSV、来源记录、PNG和ROOT文件。ROOT绘图需PyROOT内核。

该数据只有z坐标；三维曲面表示“z—入射能量—密度”，不是XYZ空间重建。

## 5. 单能束流：muon_beam

入口：[完整说明](Data/Geant4_data/muon_beam/README_muon_beam.md)。

```bash
cd "$PROJECT_ROOT/Data/Geant4_data/muon_beam"
bash build.sh
cp -n config.mac config_my_run.mac
nano config_my_run.mac
bash run.sh config_my_run.mac
```

编辑配置时设置粒子、动能、材料、几何、事件数、种子及新的 `/beam/output`。首次可设置10个事件。配置副本应留在本目录，避免相对数据路径变化。

**当前随目录保存的配置是O18离子：Z=8、A=18、Q=6、动能46.8 GeV、10000事件，并非默认缪子运行。** 若研究缪子，请明确将 `/beam/particle` 设为 `mu-` 或 `mu+`，并设置 `/beam/energy_GeV`。不要改成总能量而仍按动能解释。

脚本从项目YAML生成材料表，运行输运后生成空间可视化。输出包含核出生记录、入射记录、统计及ROOT/HTML图；细节以专用README为准。每次使用新输出目录，不能直接覆盖已有结果。

## 6. 大气缪子圆柱：muon_stalactite_full

入口：[完整说明](Data/Geant4_data/muon_stalactite_full/README_muon_stalactite_full.md)。

```bash
cd "$PROJECT_ROOT/Data/Geant4_data/muon_stalactite_full"
bash build.sh
cp -n config.mac config_my_run.mac
nano config_my_run.mac
bash run.sh config_my_run.mac
```

当前配置为Halite、半径10 m、长度100 m、100–10000 GeV、100000事件。修改 `/stalactite/events` 与 `/stalactite/output` 做小样本试跑，再决定正式统计量。确认 `/stalactite/materials` 所指材料表存在。

关键输出：`primaries.csv`、`nuclei.csv`、`metadata.txt`、`summary.csv`、ROOT文件及圆柱密度HTML。`planned_exposure.txt` 仅为计划曝光，不代表输运已完成。使用metadata中的实际几何、能窗和等效曝光归一化。

对已完成输出补画出生点及直线射程，无需重跑：

```bash
python3 ../render_birth_points.py output_my_run
python3 ../render_recoil_tracks.py output_my_run
```

这里 `output_my_run` 替换为真实输出目录。旧核数据若缺方向字段，不能恢复直线射程方向。

### 局部体积取样

```bash
cp -n sample.mac sample_my_run.mac
nano sample_my_run.mac
python3 analyze_sample.py sample_my_run.mac
```

设置 `/sample/input` 为已完成运行，`/sample/output` 为新的分析目录；`center_m` 是相对靶中心坐标，`size_m` 是完整边长。当前原卡读取run04，中心(0,0,-45)m、尺寸(10,10,10)m。选取的是出生点落入盒内的核，不包括从盒外穿入的所有轨迹。

### 长期曝光深度—射程谱

```bash
cp -n depth_tracks.mac depth_my_run.mac
nano depth_my_run.mac
python3 analyze_depth_tracks.py depth_my_run.mac
```

设置input、output、years、深度范围和分箱。当前原卡为run03、470000年、80–100 m；不要将它直接用于不覆盖该深度的靶。该后处理读取运行metadata、nuclei.csv及本程序目录的materials.tsv，输出 `depth_tracks.csv`、`depth_tracks.html`。

## 7. Glashow：W强子末态输运

入口：[完整说明](glashow_starter/README_先读我.md)。

```bash
cd "$PROJECT_ROOT/glashow_starter"
cp -n local_env.example.sh local_env.sh
nano local_env.sh
bash build.sh
bash run.sh 6320000 1 12347 results/my_small01 0.1 0.2
python3 analyze.py results/my_small01
```

`local_env.sh` 设置本机Geant4、PYTHIA和构建路径，不提交Git。参数依次为能量GeV、W事件数、随机种子、新输出目录、半径m、长度m。例子是6.32 PeV、1个W强子事件、小靶，不是1条核反冲。

当前默认启用1 MeV次级粒子截断；低能次级粒子及其后续反冲会缺失。要关闭新增截断，显式运行：

```bash
GLASHOW_SECONDARY_MIN_MEV=0 bash run.sh 6320000 1 12347 results/my_small_no_cut01 0.1 0.2
```

关闭截断可能明显增加计算量。完整运行应满足 `run.json` 中 `complete=true` 且 `completed_events=requested_events`。`--generate-only` 只检验生成器，不能用其结果分析材料反冲。

主要输出为run.json、事件账本、初级粒子、核出生及核终点TSV/gzip表。`analyze.py` 生成 `analysis/spectra.png` 等，要求完整事件。中断样本用 `audit_partial.py` 作描述性统计，不可当作完整事件产额。

历史运行依赖版本不同：早期小靶记录为Geant4 11.2.0；my_new_bulk06记录为11.3.2，PYTHIA为8.317。复核某次运行时以它的run.json为准。

## 8. Glashow HTML后处理

入口：[完整说明](glashow_starter/html_recoils/README核出生与反冲.md)。

```bash
cd "$PROJECT_ROOT/glashow_starter"
python3 html_recoils/render_glashow.py results/my_small01
```

Python标准库即可运行；需要完整运行的run.json、recoils表和Halite射程表。输出在运行目录的 `html_recoils/`：`birth_points.html`、`recoil_tracks.html`、`summary.json`。浏览器打开HTML即可交互查看，无需重新输运。

默认最多抽样显示200000个核，原始表不变。射程线是出生方向乘查表CSDA射程，不是真实逐步轨迹；缺表和越界记录会在summary中说明。重复执行会覆盖派生HTML。

## 9. 数据生成与可选模块

`Data/Geant4_data/run_geant.ipynb` 应从其所在目录使用，调用原Geant4应用生成响应表、几何记录和射程表。检查编译入口、材料、事件数、能量扫描及目标目录后再运行，不要直接Run All覆盖研究数据。

`Test_optimusprimus.ipynb` 调用 `OptimusPrimus.py`，包含切图、标注掩膜、训练、分类和推理。当前仓库工作目录没有所需 `Data/images` 和 `Data/models`，因此仅保留为可选上游功能，不能承诺开箱复现。训练前必须补齐图像、标注、权重和对应依赖。

`testing_environment.ipynb` 为旧设备诊断记录，含个人路径及射程表写入操作，不作为新用户一键入口。

## 10. 结果解释与协作

- 核出生计数、CSDA射程、能量沉积和长期保存的损伤径迹不同；不得混用。
- Glashow输运以已经发生W强子衰变为条件，不直接输出宇宙中微子年事例率。
- 抽样测试、编译成功、完整输运及物理模型验证分别记录。
- 修改参数、数据字段或归一化时同步修改对应README；记录代码提交、环境版本、配置、种子和数据来源。
- 使用功能分支和Pull Request协作。现有输出及构建目录可能被.gitignore排除，缺数据时联系维护者。

## 11. 运行与分析配置卡（.mac）说明

### 11.1 每个入口使用哪种配置

| 运行或分析入口 | 配置文件/方式 | 修改后是否需要重新输运 |
|---|---|---|
| `muon_beam/run.sh` → `run_beam.py` → `muon_beam` | 同目录 `config.mac`，键以 `/beam/` 开头 | 束流、材料、几何等变化需要新运行 |
| `muon_stalactite_full/run.sh` → 同名可执行文件 | 同目录 `config.mac`，键以 `/stalactite/` 开头 | 模拟配置变化需要新运行 |
| `analyze_sample.py` | `sample.mac`，键以 `/sample/` 开头 | 不需要，只重新选择已有出生点 |
| `analyze_depth_tracks.py` | `depth_tracks.mac` 或 `depth_tracks_run04.mac`，键无前缀 | 不需要，重算已有数据的加权谱 |
| `cylinder_density.py` | 可用 `--card 配置卡`；也可直接传输出目录和分箱参数 | 不需要，只重新绘图 |
| `render_birth_points.py`、`render_recoil_tracks.py` | 不读取独立mac；命令行传运行目录 | 不需要 |
| `plot_depth_tracks.C`、`plot_depth_tracks_full.C` | ROOT宏，后缀是 `.C`，不是 `.mac`；读取分析CSV/模拟数据 | 不需要，须检查宏内路径及参数 |
| `glashow_starter/run.sh` | 命令行参数和 `local_env.sh`，无运行mac | 模拟参数变化需要新运行 |
| `glashow_starter/analyze.py`、`audit_partial.py`、`html_recoils/render_glashow.py` | 命令行传目录/选项，无mac | 不需要 |
| 第3节全部分析Notebook、`muon_stalactite.ipynb` | Notebook参数单元，无mac | 分析本身不重跑输运，须有对应输入 |
| `Test_optimusprimus.ipynb` | Notebook中的图像路径、模型和训练参数，无mac | 不涉及Geant4输运 |
| `Data/Geant4_data/run_geant.ipynb` | 自动生成 `build/materials.mac` 和 `build/run.in` | 执行模拟单元会进行新输运 |

前三类专用前缀卡和深度谱卡由各自程序解析，**不能互换，也不能在其中随意加入 `/run/beamOn`、`/gun/energy` 等标准Geant4命令**。后缀相同不代表格式相同。

配置卡内相对输入/输出路径以卡片所在目录为基准。复制卡片时建议放在原程序目录，只改文件名。数值单位已写在键名里：例如 `/beam/energy_GeV 100`，不要追加 `GeV`。标准Geant4宏则需要相应命令的单位参数。注释以 `#` 开头，不要保留重复的有效配置键。下表列的是当前卡片值，不保证等于程序在缺省参数时采用的值。

### 11.2 项目1：惠州束流 config.mac

位置：`Data/Geant4_data/muon_beam/config.mac`。以下参数名均带 `/beam/` 前缀。

| 参数 | 当前值 | 物理含义或用途 |
|---|---|---|
| `material` | `Halite` | 靶材料，名称与材料库匹配 |
| `particle` | `ion` | `mu-`、`mu+`、`proton` 或 `ion` |
| `ionZ / ionA / ionQ` | `8 / 18 / 6` | 离子原子序数、质量数、电荷态；ion时使用，Q以元电荷为单位 |
| `energy_GeV` | `46.8` | 每个入射粒子的动能GeV；O18的2.6 GeV/u对应18×2.6=46.8 GeV，不直接填2.6 |
| `side_cm` | `10` | 立方体完整边长cm |
| `spotRadius_cm` | `3` | 圆盘束斑半径cm，应不超过立方体半边长 |
| `events` | `10000` | 初级粒子事件数 |
| `seed` | `20261101` | 随机种子 |
| `output` | `output_Halite_O18_2p6GeVu_01` | 新运行输出目录，不得已存在 |
| `data` | `..` | 射程数据根目录，即Geant4_data |
| `materials` | `materials.tsv` | run_beam.py根据项目YAML生成的材料表 |
| `recordThreshold_MeV` | `0.001` | 次级核出生动能记录阈值，0.001 MeV=1 keV；不是初级束能，也不是输运截断 |
| `productionCut_mm` | `0.005` | Geant4 production cut，长度单位mm；不是统一最低粒子动能 |
| `xBins / zBins` | `80 / 100` | ROOT XZ图分箱数 |

沿用卡片其他字段、改做100 GeV负缪子10事件试跑时，把相应行改成：

```text
/beam/particle mu-
/beam/energy_GeV 100
/beam/events 10
/beam/output output_Halite_mu_smoke01
```

这些是修改片段，不是完整卡。运行 `bash run.sh config_my_run.mac`。HTML体素分箱由Python命令行 `--nx`、`--ny`、`--nz` 设置，不是上述mac键，例如 `bash run.sh config_my_run.mac --nx 20 --ny 20 --nz 20`。

### 11.3 项目2：缪子钟乳石 config.mac

位置：`Data/Geant4_data/muon_stalactite_full/config.mac`。以下参数名均带 `/stalactite/` 前缀。

| 参数 | 当前值 | 含义 |
|---|---|---|
| `material / materials / data` | `Halite / materials.tsv / ..` | 材料名称、材料表、射程数据根目录 |
| `output` | `output_Halite_run04` | 新模拟目录 |
| `events / seed / progressEvery` | `100000 / 20261031 / 1000` | 事件数、种子、进度输出间隔 |
| `radius_m / length_m` | `10 / 100` | 圆柱半径和完整长度m；中心为原点 |
| `energyMin_GeV / energyMax_GeV` | `100 / 10000` | 入射缪子动能抽样范围；0<下限<上限 |
| `gamma` | `2.7` | GT通量模型的谱指数参数 |
| `muPlusOverMinus` | `1.3` | 正负缪子数量比 |
| `energyBins / angleBins` | `800 / 400` | 能量—角度联合抽样CDF网格；不是输出能谱分箱 |
| `productionCut_mm` | `0.005` | Geant4 production cut |
| `recordThreshold_MeV` | `0.001` | 核出生记录阈值；不停止低于阈值的粒子输运 |
| `xBins / zBins` | `80 / 100` | ROOT位置图分箱 |
| `rangeBins` | `100` | 射程分箱数 |
| `rangeMin_nm / rangeMax_nm` | `0.001 / 10000000` | 射程图范围nm，采用对数分箱 |
| `htmlRadialBins / htmlPhiBins / htmlZBins` | `10 / 24 / 50` | 圆柱HTML径向、方位角、轴向分箱；乘积不得超过100000 |

运行 `bash run.sh config_my_run.mac`。只修改配置值无需重新编译；修改C++或新增命令后需重新构建。改变能窗、几何、事件数会改变等效曝光，不能继续套用旧运行的时间换算。

仅重画历史密度图可运行：

```bash
python3 cylinder_density.py output_Halite_run04 --nr 10 --nphi 24 --nz 50
```

这是对既有数据重新分箱，不能改变那次模拟实际采用的几何或能窗。

### 11.4 局部体积分析 sample.mac

位置：`Data/Geant4_data/muon_stalactite_full/sample.mac`。这是 `analyze_sample.py` 的后处理输入，不传给Geant4。

```text
/sample/input output_Halite_run04
/sample/output samples/my_bottom_box01
/sample/center_m 0 0 -45
/sample/size_m 10 10 10
```

| 键 | 说明 |
|---|---|
| `/sample/input` | 含metadata.txt和nuclei.csv的已有模拟目录 |
| `/sample/output` | 新分析目录，不能已存在 |
| `/sample/center_m x y z` | 盒中心在原靶坐标中的位置m，原点为圆柱中心 |
| `/sample/size_m dx dy dz` | 盒子的三个完整边长m，不是半长 |

全部四个键必须出现且不能重复。取样盒必须完全位于原圆柱内。对100 m长圆柱，上例取z∈[-50,-40]m，对应顶面以下90–100 m；不能把z=-45 m读作顶面以下45 m。

执行 `python3 analyze_sample.py sample_my_run.mac`。输出的x/y/z相对取样盒中心，global_x_m等保留原坐标；depth_m仍相对原靶顶面。没有出生方向的旧数据只能画出生点。修改卡片不改变原模拟CSV。

### 11.5 深度—射程谱 depth_tracks.mac

位置：`Data/Geant4_data/muon_stalactite_full/`。这类卡片的键**不加 `/sample/` 或 `/stalactite/` 前缀**。

```text
input output_Halite_run03
output samples/my_run03_depth01
years 470000
depth_m 80 100
depth_bins 4
range_bins 100
range_max_um 25
elements all
```

| 键 | 含义 |
|---|---|
| `input` | 已有模拟目录，需要metadata.txt和nuclei.csv |
| `output` | 本次分析目录，建议每次使用新名称 |
| `years` | 目标累计曝光年数，不是程序运行时间 |
| `depth_m lo hi` | 相对圆柱顶面的深度区间m，须处于实际靶长内 |
| `depth_bins` | 深度等宽分层数量；80–100 m分4层，每层5 m |
| `range_bins` | 0到range_max_um内的线性射程分箱数量 |
| `range_max_um` | 射程显示上限μm；当前25，配合100箱时箱宽0.25 μm；超出上限另行统计 |
| `elements` | 当前解析器只接受 `all`，同时生成所有核、Na、Cl三组，不支持直接写Na或Cl |

`depth_tracks.mac` 指向run03；`depth_tracks_run04.mac` 指向run04及对应输出。`reproduce_depth.mac` 是本地核验时的卡片，不是新的物理模型；如仓库有该文件，使用前同样检查input/output。

执行 `python3 analyze_depth_tracks.py depth_my_run.mac`。质量归一化还读取程序旁的materials.tsv；years的缩放使用输入metadata中的等效曝光。只改years相当于按原模拟的同一通量条件线性外推，不会模拟新的覆盖层历史。

### 11.6 项目3：RockCube没有运行mac

Glashow的 `run.sh` 接收位置参数，不接受上述config.mac：

```bash
bash run.sh 6320000 1 12347 results/my_small02 0.1 0.2
```

对应能量GeV、事件数、种子、输出目录、半径m、长度m。`local_env.sh` 管理安装路径、构建位置及环境开关；`pythia_settings.cmnd` 是运行保存的生成器设置记录，`run.json` 是运行元数据，不是编辑后即可重跑的mac。

`GLASHOW_SECONDARY_MIN_MEV` 控制次级粒子截断，`GLASHOW_COMPRESS` 控制输出压缩。**如果local_env.sh无条件export了同名变量，它会覆盖命令前设置的值。** 要改变阈值，应同步修改或注释local_env.sh中的赋值，并核对新run.json记录的真实阈值。

后处理也使用命令行，不另造mac：

```bash
python3 analyze.py results/my_small02
python3 audit_partial.py results/my_interrupted_run partial_summary.json
python3 html_recoils/render_glashow.py results/my_small02 --max-points 200000
```

目录应替换为真实完整或中断运行。HTML工具的 `--event` 指定事件号、`--min-mev` 指定显示最低出生动能、`--range-dir` 指定射程表目录、`--analysis-selection` 使用与analyze.py一致的核筛选。显示筛选不改变输运阈值，也不能恢复已被截断的粒子。

### 11.7 上游 run_geant.ipynb 自动生成的宏

它与前三类专用配置卡不同：生成真正的Geant4命令宏，由 `build/Run` 执行。

- `build/materials.mac`：由 `Data/mineral_library.yaml` 生成，使用 `/testhadr/defineMaterial` 定义材料。应修改YAML后重新生成，而非长期手改派生宏。
- `build/run.in`：虽不是.mac后缀，内容仍是Geant4宏；由 `run_geant4_simulation(...)` 生成并执行。

| 生成的命令 | 来源/用途 |
|---|---|
| `/control/execute <材料宏>` | 加载materials.mac |
| `/testhadr/TargetMat` | mineral_name |
| `/testhadr/TargetRadius ... m` | target_radius_m |
| `/testhadr/TargetLength ... m` | target_length_m |
| `/run/setCut 0.005 mm` | 此生成函数中的production cut设置 |
| `/run/initialize` | 初始化物理与几何 |
| `/gun/particle` | particle_type |
| `/gun/ion Z A Q E` | ion模式的离子设置；当前生成函数使用Q=Z、激发能0 |
| `/gun/energy ... GeV` | energies_gev中的每个能量点 |
| `/run/beamOn N` | 每个能量点的n_events |

从Notebook修改函数参数再生成宏。调用会更新build/run.in，并移动输出至材料/粒子目录；已有同名响应文件须事先妥善保存。`geometry.json` 是配套记录，不能只复制outNuclei而遗漏它。

## 来源与许可

上游来源为cgalelli/primus-paleo。保留 [LICENSE](LICENSE)（BSD 3-Clause）、[CITATION.cff](CITATION.cff) 和[上游说明及论文引用](docs/UPSTREAM_README.md)。归档上游README用于来源追溯，不是当前操作指南。本项目新增功能不归称为上游作者的研究成果；第三方依赖遵循各自许可证。
