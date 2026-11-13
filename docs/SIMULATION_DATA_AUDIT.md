# 模拟入口与数据交付审计

检查日期：2026-11-13。仅检查本地源码、配置、文件存在性和历史 metadata；未重新构建、输运或逐行校验大型 CSV。路径以下均相对 primus-paleo。

## 当前配置与文档差异

| 入口 | 实际配置 | 数据依赖/交付要求 |
|---|---|---|
| Data/Geant4_data/muon_beam/run.sh | config.mac 当前为 ion，Z=8、A=18、Q=6、46.8 GeV、10000 事件；输出 output_Halite_O18_2p6GeVu_01 | run_beam.py 读取 Data/mineral_library.yaml（存在），生成 materials.tsv；需要 Data/Geant4_data/Halite/Range（存在）及共享绘图脚本/模板 |
| Data/Geant4_data/muon_stalactite_full/run.sh | config.mac 当前为 Halite、半径10 m、长度100 m、100–10000 GeV、100000事件；输出 output_Halite_run04 | materials.tsv、Halite/Range、共享绘图脚本/模板；Geant4/ROOT 环境及 Geant4 数据包另行安装 |
| analyze_sample.py sample.mac | 输入 output_Halite_run04；输出 samples/run04_bottom_1000；中心(0,0,-45)m，完整边长(10,10,10)m | 读取输入 metadata.txt 和 nuclei.csv；历史输入存在，但不属于源码交付 |
| analyze_depth_tracks.py depth_tracks.mac | 输入 output_Halite_run03；输出 samples/run03_depth_tracks；470000年，80–100 m，4层，100箱，25 µm上限 | 输入 metadata.txt、nuclei.csv，外加程序目录 materials.tsv；历史输入存在 |
| glashow_starter/run.sh | 将命令行参数交给 GlashowRun；加载 env.sh/local_env.sh | Geant4，PYTHIA8_ROOT 下库与 PYTHIA8DATA 指向的 xmldoc；默认 vendor/pythia8317，排除 vendor 后必须另行安装并配置 |
| glashow_starter/analyze.py | 从指定运行目录读取事件账本与反冲表，接受 tsv/tsv.gz | 至少交付 run.json、events.tsv[.gz]、recoils.tsv[.gz]；完整复核应保留 primaries、recoil_endings、pythia_settings.cmnd |

审计初次发现 README 的束流、大气和取样默认说明与卡片不符；已同步两份 README 的当前配置表，并将旧 mu- 与10 GeV下限、run03顶部小盒标为历史示例。研究配置未改动。

## 历史结果证据与限制

run03 和 run04 均存在 metadata.txt、primaries.csv、nuclei.csv、summary.csv 和 ROOT 文件；metadata 均记录 events=100000。run03 为10 GeV下限，曝光15.823174541304秒；run04 为100 GeV下限，曝光626.404315871069秒。nuclei.csv 分别为326620311和1109515345字节。此检查未逐行验证事件数，也未检查 ROOT 内容，因此不能据此声称当前源码通过完整输运验证。metadata 内 range_source 含原机器绝对路径，应作为历史来源保留，不作为新机配置。

Glashow 的 run.json 中，io_truncated_test、memory_after_bulk_stress、my_new_bulk01、W_test01 的 complete=false；不能与完整样本合并使用。其余检查到的 run.json 标记 complete=true，但 generator_check01、io_portable_generator、verified_generator01 的 completed_events=0、generated_events>0，属于生成器记录，不能按完整输运样本解释。my_optimized_bulk 目录未发现运行文件。本次未校验压缩文件完整性或事件账本一致性。

## 构建路径与交付边界

muon_beam/build.sh 和 muon_stalactite_full/build.sh 根据 BASH_SOURCE 定位自身，使用各自 build-wsl，未发现个人绝对路径；前者调用 Python 测试，后者调用 CTest。应在新机器重新构建，不交付旧 CMake 缓存作为可复用环境。

Glashow build.sh/run.sh 通过 env.sh 读取本机配置。当前 local_env.sh 指定 /opt/geant4、/usr/bin/gcc、/usr/bin/g++ 和 build_cluster_gcc11；这些是机器配置，不是所有协作者都可用的安装位置。仅交付 local_env.example.sh，并要求本地配置。vendor 不随源码发布时必须解释 PYTHIA 安装和 PYTHIA8DATA 配置。

源码发布必须保留各 CMakeLists、源代码、脚本、配置卡、材料表、Data/mineral_library.yaml、所选材料 Range 表以及 Geant4_data 的共享 HTML 生成器和模板。模拟输出可单独归档；要重现历史后处理，须另外提供对应完整运行目录或记录获取方法与校验和。不要用忽略所有 Data 的规则排除必要输入。

证据来源：上述 config.mac、sample.mac、depth_tracks.mac、run_beam.py、analyze_sample.py、analyze_depth_tracks.py、两份 build.sh；glashow_starter 的 env.sh、local_env.sh、build.sh、run.sh、analyze.py、results/*/run.json；run03/run04 文件元数据。

## my_new_bulk06 数据分发评估

多数现有 Glashow run.json 标记 complete=true 且 completed_events=1，包含 my_new_bulk02–06、my_new_medium01–03、my_new_small01 等；并非只有生成器结果可用。my_new_bulk06 明确 generate_only=false、requested/generated/completed_events 均为1，历史程序声明完成一次输运。该状态仍不等价于本次独立完整性验证。

完整六文件：events.tsv.gz 130字节、primaries.tsv.gz 965字节、pythia_settings.cmnd 374字节、recoil_endings.tsv.gz 311416653字节、recoils.tsv.gz 500869471字节、run.json 1213字节，总计812288806字节（约774.7 MiB）。适合单独作为历史数据包交付，保留六文件和目录结构；不建议将两张大型原始表直接纳入普通 Git 提交。Python 后处理读取 gzip，无需重跑 Geant4/PYTHIA；重跑则需相应依赖、相同参数、版本与种子。建议打包前另做 gzip 完整性、账本一致性和 SHA256 校验。

该样本的条件包括：6.32 PeV、种子12346、Halite圆柱半径10 m长度20 m、FTFP_BERT_HP + EM option4、Geant4 11.03-patch-02、PYTHIA8 8.317、1 MeV次级输运能量截断和1000 keV记录阈值。complete=true 不能消除这些模型和截断限制；不要称为无截断或完全验证的物理预测。
