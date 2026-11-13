# OldMuon：矿物中的宇宙线与核反冲模拟

本仓库基于上游 PRImuS / primus-paleo，并加入缪子束流、大气缪子空间模拟、Glashow 共振强子末态输运及可视化工具。面向共同开发的 Linux / WSL 用户。仓库规划为 `GFOPhmx2020/OldMuon`（私有）；当前尚未发布。

## 来源与许可

上游代码来源为 cgalelli/primus-paleo。保留原始 [LICENSE](LICENSE)（BSD 3-Clause）、[CITATION.cff](CITATION.cff) 及[上游说明与论文引用](docs/UPSTREAM_README.md)。上游说明作为历史来源归档，不是本仓库当前操作指南；其中联系方式属于上游作者。本项目新增内容不应归称为上游作者的研究成果。第三方依赖保留各自许可证。

## 目录与入口

| 入口 | 用途 |
|---|---|
| `mineral_utils.py`、`flux_history.py` | 矿物响应与宇宙线/覆盖层历史 |
| `CDP_analysis_new.ipynb`、`MSC_analysis_new.ipynb` | 上游橄榄石、岩盐分析流程 |
| `OptimusPrimus.py`、`Test_optimusprimus.ipynb` | 显微图像径迹识别，另需训练数据/模型 |
| [muon_stalactite.ipynb 说明](README_muon_stalactite.md) | 旧六列数据的 z 分布分析 |
| [muon_beam](Data/Geant4_data/muon_beam/README_muon_beam.md) | 单能缪子照射立方体 |
| [muon_stalactite_full](Data/Geant4_data/muon_stalactite_full/README_muon_stalactite_full.md) | 大气缪子、圆柱几何、空间分布及取样分析 |
| [glashow_starter](glashow_starter/README_先读我.md) | PYTHIA W 强子末态与 Geant4 岩盐输运 |
| [Glashow HTML](glashow_starter/html_recoils/README核出生与反冲.md) | 出生点和直线射程交互显示 |
| `Data/mineral_library.yaml` | 材料定义 |
| `Data/Geant4_data/*/Range` | 能量—射程查表数据，必须保留 |

## 获取和环境

仓库发布且获得访问权限后：

```bash
git clone git@github.com:GFOPhmx2020/OldMuon.git
cd OldMuon
export PROJECT_ROOT="$PWD"
python3 -m venv .venv
source .venv/bin/activate
python -m pip install numpy scipy matplotlib mendeleev pyyaml jupyter
```

以上为上游分析基础依赖，不是所有功能的完整锁定环境。PyROOT/ROOT、Geant4 及物理数据、PYTHIA 均需单独安装；加载各自环境后使用能够导入所需模块的 Python/Jupyter 内核。图像识别另外需要 PyTorch、scikit-learn 和 scikit-image；重算宇宙线通量可能需要 MCEq。不要将普通 pip 安装当作 Geant4/ROOT 安装。

Glashow 文档记录的历史环境为 Geant4 11.2.0、PYTHIA 8.317；并不保证其他版本逐位复现。每位开发者维护自己的环境路径，勿提交 `local_env.sh`。

## 运行顺序

1. 选择上表中的模拟或分析入口，阅读对应 README。
2. 检查材料、几何、阈值、能量单位、随机种子和输出路径。先用少量事件验证；部分默认配置为十万事件。
3. 编译所选独立 CMake 工程，运行到新输出目录。保留配置和完成状态，勿覆盖已有模拟。
4. 检查输出完整性，再运行对应 Notebook 或后处理脚本。
5. 记录依赖版本、代码提交、随机种子、归一化分母和数据来源。

Notebook 从仓库根目录启动 `jupyter notebook`；若旧 Notebook 含个人路径，需要改为本机数据路径。当前文档整理未修改或执行 Notebook。历史 run03/run04、批量输出与训练资源不保证随源码发布，缺失时先获取对应数据，不可用不同数据冒充复现。

## 物理与统计边界

- 核出生计数、CSDA 射程和可保存的损伤径迹是不同量；直线射程图不是完整 Geant4 曲线轨迹。
- 旧六列 outNuclei 数据只有 z，不能恢复真实 XYZ 分布。
- Glashow 模拟条件是一次 W 强子衰变，不直接给出宇宙中微子年事例率。
- **Glashow 当前默认删除出生动能低于 1 MeV 的次级粒子**；低能反冲会缺失。关闭方式及影响见子项目 README，不能仅降低绘图阈值恢复数据。
- 编译、抽样测试、完整输运和物理模型验证需分别记录；本次文档整理未进行输运验证。

## 协作与数据发布

阅读 [发布清单与 WSL 步骤](docs/PUBLISHING.md)。日常开发建议每项工作一个分支，经另一位合作者审阅后合并。修改运行参数、输出字段或统计口径时同步维护相应 README。提交 Notebook 前检查输出是否含个人路径或大型内嵌数据。
