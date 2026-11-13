# OldMuon：矿物中的宇宙线与核反冲模拟

本仓库基于上游 PRImuS / primus-paleo，包含矿物响应分析、束流模拟、大气缪子空间模拟、Glashow 共振强子末态输运和可视化工具。仓库已发布至 `GFOPhmx2020/OldMuon`（私有），供获得授权的合作者使用。以下命令面向 Linux / WSL Bash。

## 1. 按研究目标选择入口

| 研究目标 | 入口 | 类型 |
|---|---|---|
| 岩盐长期曝光与径迹谱 | `MSC_analysis_new.ipynb`、`MSC_analysis_local.ipynb` | 读取预计算数据 |
| 橄榄石样品与宇宙线历史 | `CDP_analysis_new.ipynb` | 读取预计算数据 |
| 石英覆盖层历史 | `RSC_analysis.ipynb` | 读取预计算数据，存在已知缺表问题 |
| 飞行运输产生的贡献 | `Estimate_flight_contribution.ipynb` | 分析并保存结果 |
| 旧六列数据沿 z 的核出生分布 | `muon_stalactite.ipynb` | 分析及 PyROOT 绘图 |
| 单能缪子、质子或离子照射立方体 | `Data/Geant4_data/muon_beam` | Geant4 输运 |
| 大气缪子照射圆柱与空间取样 | `Data/Geant4_data/muon_stalactite_full` | Geant4 输运及后处理 |
| W 强子末态在岩盐中的级联 | `glashow_starter` | PYTHIA + Geant4 |
| Glashow 出生点与直线射程显示 | `glashow_starter/html_recoils` | 读取已有结果 |
| 原框架响应表与射程表生成 | `Data/Geant4_data/run_geant.ipynb` | 数据生成，不是日常分析入口 |
| 显微图像径迹识别 | `Test_optimusprimus.ipynb` | 可选模块，当前缺图像与模型 |

`mineral_utils.py` 与 `flux_history.py` 是供 Notebook 导入的核心模块，不需要直接运行。材料定义位于 `Data/mineral_library.yaml`。

## 2. 获取项目与准备环境

接受仓库邀请并完成 GitHub 登录后：

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

每次打开新终端，先进入实际仓库目录，再设置 `PROJECT_ROOT`、激活环境。它指向仓库根目录，不是外层 Old_Muon 工作文件夹。

| 功能 | 额外环境 |
|---|---|
| 预计算矿物响应分析 | 上述 Python 包；现有通量缓存齐全时不必运行 MCEq |
| 旧数据 ROOT 绘图 | 与 Python 版本兼容的 ROOT/PyROOT |
| 两套束流/大气模拟 | C++ 编译器、CMake、Geant4及物理数据、ROOT；束流脚本还需 PyYAML |
| Glashow 模拟 | Geant4、PYTHIA、C++17、CMake、zlib；普通能谱图另需 numpy/matplotlib |
| 重新计算通量 | MCEq、crflux及对应数据 |
| 图像识别 | PyTorch、torchvision、timm、segmentation_models_pytorch、albumentations、OpenCV、Pillow、scikit-image、scikit-learn等 |

Geant4、ROOT、PYTHIA 需要单独安装并加载环境。安装路径由每位合作者自行指定，例如：

```bash
# 先将变量设为本机安装目录，再执行；不要照抄他人的安装路径
source "$GEANT4_PREFIX/bin/geant4.sh"
source "$ROOT_PREFIX/bin/thisroot.sh"
python -c 'import ROOT; print(ROOT.gROOT.GetVersion())'
```

依赖成功导入不等于全部流程已验证。存在 `requirements-analysis.txt` 时可参考其中的历史版本记录，它不是完整环境锁文件。

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

## 来源与许可

上游来源为cgalelli/primus-paleo。保留 [LICENSE](LICENSE)（BSD 3-Clause）、[CITATION.cff](CITATION.cff) 和[上游说明及论文引用](docs/UPSTREAM_README.md)。归档上游README用于来源追溯，不是当前操作指南。本项目新增功能不归称为上游作者的研究成果；第三方依赖遵循各自许可证。
