> 已停用：用户决定直接上传 primus-paleo，不继续复现审计。本文为未完成的旧方案，不是已验证的交付承诺。

# 数据与复现操作

本说明取代早期“只需97.5 MiB”的判断。代码之外，需要 docs/reproduction-data.tsv 中列出的完整输入，原始约4.12 GiB。复现指从同一输入重新分析；重新输运还需要安装 Geant4/PYTHIA/ROOT，其随机结果不保证跨版本逐位相同。

## 实际依赖

| 入口 | 必需输入 |
|---|---|
| MSC_analysis_local / MSC_analysis_new | Halite 的 mu-/mu+/neutron 的全部 outNuclei 表和 geometry；StdRock 的 mu-/neutron 表；Halite Range；mineral_library.yaml、nuclear_data、Baseline_5.0GV.npz |
| CDP_analysis_new | Olivine 三类响应表、StdRock 两类响应表、Olivine Range、核数据；1.0km 的 Baseline 5GV/0GV 和 SN250 5GV/0GV 四份通量缓存 |
| Estimate_flight_contribution | Olivine/StdRock 响应和射程、Baseline_5.0GV_11.0km.npz；末尾会写 Data/tracks_Olivine_1hr11.0km_flight.npz |
| RSC_analysis | Quartz/neutron、StdRock 响应、Quartz Range、核数据和默认 Baseline 缓存；当前 Quartz 无缪子响应表，只能复现当前代码可用物种，不是完整缪子预测 |
| muon_stalactite | Olivine/mu-/outNuclei 全部表和 geometry.json；PyROOT |
| run03/run04 空间取样与深度谱 | 对应 nuclei.csv、metadata.txt，原配置/primaries/summary用于核验；materials.tsv 是后处理必需输入，必须保留 |
| Glashow 大靶分析和 HTML | my_new_bulk06 的 run.json、events、recoils、recoil_endings、primaries、pythia_settings；Halite Range |
| OptimusPrimus | 缺 images、标注和 models；经所有者确认作为不可复现的可选上游模块保留 |

不能删减响应表能量点：mineral_utils.py 按文件列表确定能量网格，缺文件可能改变结果而非直接报错。geometry.json 也须保留，缺失时部分代码使用默认几何。StdRock 是覆盖层响应，不可因它不是目标矿物而排除。基准通量缓存已存在，不需要为现有场景重新运行 MCEq。

`testing_environment.ipynb` 是旧机器诊断记录，含绝对路径及改写射程表操作，不列为一键运行入口。`Data/Geant4_data/run_geant.ipynb` 是生成数据的重型流程，不应为复现已有分析自动执行。

## 新机器准备

使用 Linux / WSL、Python 3.10。从发布后的仓库根目录执行：

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r requirements-analysis.txt
python -m ipykernel install --user --name oldmuon --display-name 'Python (OldMuon)'
# 将随项目交付的 OldMuon-data.tar.gz 放在仓库根目录
sha256sum -c OldMuon-data.tar.gz.sha256
tar -xzf OldMuon-data.tar.gz
sha256sum -c docs/reproduction-data.sha256
mkdir -p output/reproduced
```

解压必须在干净克隆中进行；它会恢复原相对路径。文件级校验通过后才执行分析。数据包尚未发布到服务器，不能假定 git clone 会自动获取它。交付时仓库和数据包须同时提供。

## 现有分析

```bash
python -m jupyter nbconvert --to notebook --execute RSC_analysis.ipynb --ExecutePreprocessor.kernel_name=oldmuon --ExecutePreprocessor.timeout=3600 --output-dir=output/reproduced
# 其他分析以同样方式替换文件名；计算可能较久
```

muon_stalactite 绘图还需加载兼容 Python 3.10 的 ROOT：先设置 ROOT_PREFIX 为实际安装目录，再 `source "$ROOT_PREFIX/bin/thisroot.sh"`，确认 `python -c 'import ROOT'` 成功。不要自动采用原作者机器的安装路径。

## 已有大气模拟的深度谱（无需 Geant4/ROOT）

```bash
cd Data/Geant4_data/muon_stalactite_full
# 保留原配置，以新输出目录运行，避免覆盖已有结果
sed 's|^output .*|output samples/reproduced_run03_depth|' depth_tracks.mac > reproduce_depth.mac
python3 analyze_depth_tracks.py reproduce_depth.mac
```

查看 samples/reproduced_run03_depth/depth_tracks.html。再次执行前更换输出目录名。run04 取样参考 sample.mac，同样使用新输出目录；坐标和尺寸保持实际卡片值。

## Glashow

已有 my_new_bulk06 为1个完成事件、1 MeV截断、Geant4 11.3.2、PYTHIA 8.317。它不是低能无截断样本，不能与旧11.2.0小靶测试混称同一版本。

```bash
cd glashow_starter
python3 analyze.py results/my_new_bulk06
python3 html_recoils/render_glashow.py results/my_new_bulk06
```

这些命令会生成/覆盖派生 analysis 与 html_recoils，不改原始模拟表。大靶查重分析可能占用较多内存。重新生成事件按子项目 README 配置依赖后执行；本轮不承诺用当前源码重现历史事件的逐行内容。

## 验证边界

具体执行结果见 REPRODUCTION_STATUS.md。依赖成功导入、数据存在、Notebook执行完成和物理结论正确是四种不同层级；未完成的检查不得描述为已通过。
