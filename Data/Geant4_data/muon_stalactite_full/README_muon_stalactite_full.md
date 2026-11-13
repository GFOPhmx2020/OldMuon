# WSL 一键构建与配置卡运行

运行前需在 WSL/Linux 中安装并加载 Geant4、ROOT 环境及所需 Geant4 数据包。`PROJECT_ROOT` 表示仓库根目录；`ROOT_PREFIX` 表示本机 ROOT 安装目录，例如设置 `export ROOT_PREFIX=/path/to/root` 后执行 `source "$ROOT_PREFIX/bin/thisroot.sh"`。这里的路径需按本机安装替换，ROOT 环境变量不等同于项目根目录。

文中 `output_*`、run03/run04、`samples/` 及历史日志是本地运行示例，不随源码仓库分发。后处理前应先生成或取得对应数据，再修改配置卡中的输入、输出路径。所有历史验证记录仅说明当时的版本；本次文档整理未重新编译或运行 Geant4/ROOT。

```bash
# 从克隆后的仓库根目录开始
export PROJECT_ROOT="$PWD"
cd "$PROJECT_ROOT/primus-paleo/Data/Geant4_data/muon_stalactite_full"
bash build.sh
bash run.sh
```

先加载 Geant4/ROOT 环境。build.sh 默认 8 个编译任务，可用 `BUILD_JOBS=4 bash build.sh` 改变。
构建和测试任一步失败都会停止。可向脚本传入 `-DGeant4_DIR=... -DROOT_DIR=...` 等 CMake 参数。
脚本不安装依赖，也不自动开始模拟。

今后改 `config.mac` 即可调整模拟与绘图参数，直接 `bash run.sh`；只有改 C++ 才需重新构建。
可复制卡片后执行 `bash run.sh other.mac`。所有输入/输出路径相对卡片所在目录解析。
每次运行输出目录必须不存在；程序保存原卡片到 `config_used.mac`，有效主要参数也写入 metadata。
旧的 `--events`、`--material` 等命令行选项已由配置卡替代。

配置卡可改事件数、种子、材料、材料文件、数据目录、输出目录、圆柱尺寸、能量上下限、gamma、
电荷比例、CDF 网格、生产 cut、核记录阈值、日志间隔和所有直方图分箱。
物理模型 QGSP_BIC_HP + EM option4、全下行半球顶面入射、GT 常数及绘图样式是固定实现；修改这些算法仍需改代码。
材料的密度/元素组成继续由 materials.tsv 定义，可通过卡片选择另一份材料表。

注意：这是程序专用的 .mac 卡片命令，不支持添加任意 `/run/`、`/gun/` Geant4 UI 命令。
未知命令、重复键、非有限数值和无效范围会在输运前报错。
更换 StdRock 时请自行把 length_m 改为 1000，不再自动覆盖卡片设置。
首次建议 events=10，完成冒烟测试后改回 100000，并修改 output 为新目录。

## 修改最低入射 μ 子能量

在 `config.mac` 修改这一行（单位 GeV，表示动能）：

```text
/stalactite/energyMin_GeV 100
```

当前卡片下限为100 GeV；例如改为 50 即从 50 GeV 开始抽样。必须满足 `0 < energyMin_GeV < energyMax_GeV`。
此项现为必填：缺失会报错，不会回退到源码默认值。
改卡片后直接 `bash run.sh`；首次使用或更新源码后需执行 `bash build.sh`。
新的能窗同时用于 GT 抽样和通量归一化，自然曝光时间随之重新计算。
`recordThreshold_MeV` 则是产生核的输出记录阈值，不控制入射 μ 子能量。

## 文件职责与参数参考

| 文件 | 用途 |
|---|---|
| muon_stalactite_full.cc | GT 抽样、Geant4 输运、核记录、射程插值和 ROOT 绘图 |
| CMakeLists.txt | Geant4/ROOT 构建及独立 sampler 测试 |
| build.sh | 增量构建到 build-wsl，随后运行 CTest |
| run.sh | 执行已编译程序，默认读取同目录 config.mac |
| config.mac | 模拟和分析参数卡 |
| materials.tsv | 项目 mineral_library.yaml 的矿物组成/密度快照 |

以下命令统一使用 `/stalactite/` 前缀。数值单位由命令名规定，不追加单位字符串。

| 参数 | 当前默认值 | 含义 |
|---|---|---|
| material | Halite | 材料名 |
| materials / data | materials.tsv / .. | 材料表 / Geant4 数据根目录 |
| output | output_Halite_run04 | 新建输出目录，不允许覆盖 |
| events / seed | 100000 / 20261031 | 初级 μ 子数 / 随机种子 |
| progressEvery | 1000 | 进度打印间隔 |
| radius_m / length_m | 10 / 100 | 圆柱半径 / 总长，米 |
| energyMin_GeV / energyMax_GeV | 100 / 10000 | μ 子动能范围 |
| gamma / muPlusOverMinus | 2.7 / 1.3 | GT 谱指数 / 正负电荷比例 |
| energyBins / angleBins | 800 / 400 | log(E)、cos(theta) 联合抽样网格 |
| productionCut_mm | 0.005 | Geant4 生产 cut，毫米 |
| recordThreshold_MeV | 0.001 | 记录核的动能下限，MeV，严格大于 |
| xBins / zBins / rangeBins | 80 / 100 / 100 | 绘图分箱数 |
| rangeMin_nm / rangeMax_nm | 0.001 / 10000000 | 射程图范围，纳米 |

`output`、`data`、`materials`、`energyMin_GeV` 必须写入卡片。整数值要求 1–10^9；CDF 网格乘积不得超过 10^7，单幅直方图分箱乘积不得超过 10^6。
配置卡内 `#` 后为注释，路径不能包含 `#`；带空格路径用双引号。

## 建议运行顺序

1. 已安装依赖的 WSL 中执行 `bash build.sh`。首次构建或修改 C++/CMake 后都执行一次。
2. 编辑 `config.mac`：events 改为 10，output 改为新的冒烟测试目录。
3. 执行 `bash run.sh`，确认程序正常结束，并检查 summary、metadata 和输出图。
4. 将 events 改回 100000，output 改为新的正式目录，再执行 `bash run.sh`。

保存终端日志：

```bash
set -o pipefail
bash run.sh 2>&1 | tee "run_$(date +%Y%m%d_%H%M%S).log"
```

运行过程中的 CSV 可能尚未写完；ROOT 和绘图在输运结束后生成。中断运行留下的输出目录不能直接复用。
不要在运行中修改输入材料表或射程表；启动时读取的配置已固定，改卡片不会改变当前运行。

## 常见问题

- `does not appear to contain CMakeLists.txt`：在程序子目录运行 `bash build.sh`，不要从项目根目录执行 `cmake -S .`。
- 找不到 Geant4/ROOT：先加载 WSL 中的 Geant4/ROOT 环境；必要时使用 `bash build.sh -DGeant4_DIR=/实际路径 -DROOT_DIR=/实际路径`。Windows 库不能替代 WSL 的 Linux 库。
- 编译并行参数：脚本默认 8 个任务；手写 CMake 命令用 `-j 8`，不是 `-8j`。模拟程序本身为单线程，编译任务数不是模拟线程数。
- `Output directory exists`：改卡片的 output 为新目录；不自动删除或覆盖已有数据。
- 缺少 HP 中子数据：检查 Geant4 数据包安装和环境变量。build.sh 不安装这些数据包。
- 没有某元素的射程图数据：检查 summary 的 missing_table/outside_table；材料能输运并不意味着所有同位素射程表都齐全。

## 后续维护约定

修改程序、构建/运行脚本、配置卡、默认值、输出格式或物理统计口径时，同步更新本 README 的命令、参数表、输出解释和验证状态。
仅改变自己的一次运行配置不必重写默认参数表；项目默认卡片发生变化则需要同步。
验证状态必须区分源码检查、抽样测试、Geant4 输运、图像产出与图像外观检查，不能用旧版结果证明新版已验证。

## 模型和单位

- 采用本项目 GT 三段表达式；默认 gamma=2.7，能量按动能处理，当前配置范围 100–10000 GeV，均以配置卡为准。
- GT 是 μ 正负电荷总通量；以常数 μ+/μ−=1.3 拆分，此比例是独立假设。
- 在 log(E) 与 cos(theta) 的 800×400 网格作中点积分，反演二维 CDF，单元内均匀；联合密度为 `2*pi*GT(E,c)*c*E`。默认包含全部下行天顶角，不含从侧面进入的宇宙线。
- 顶面均匀照射，不再是中心轴束流；圆柱外侧为真空，出侧面的粒子可离开靶，不是无限岩层。
- 采用 QGSP_BIC_HP + EM option4，生产 cut 为 0.005 mm。未额外施加 tracking time/energy kill。物理表内置过程与限值仍生效；它不等同于“所有微观物理过程都已描述”。
- 每条次级核轨迹只在初次开始输运时记录；包含质子、轻核、残余核和级联产生核，排除中子。动能必须 >1 keV，阈值不用于停止输运。无 radioactive-decay 长时放射性背景模拟。
- `process` 可帮助区分弹性散射和核反应产物。所有图默认是“产生核”统计，并非仅靶核弹性反冲。
- `Range_um` 线性插值后乘 1000 转 nm；这是已有 CSDA 射程，不是 Geant4 路径累计长度，也不是保证可观测的潜径迹。
- 缺少同位素射程表或能量超表范围：记录 `range_nm=-1` 和状态，仍计入 XZ 图，不进入射程图。不能通过静默外推补齐。

## GT 归一化与自然曝光时间（2026-11-01 更新）

本节历史数值示例抽样 10–10000 GeV 的 μ 子；当前 config.mac 使用100–10000 GeV，不能直接套用下列曝光时间。对每个配置卡先积分本项目定义的 GT 强度：

`Phi = 2*pi * integral(Emin..Emax) integral(0..1) GT(E,c)*c dc dE`，单位 cm^-2 s^-1。

圆柱顶面面积 `A = pi * radius_m^2 * 10000`（cm²）；N 个入射 μ 子对应
`T_seconds = N/(A*Phi)`，`T_years = T_seconds/(365.25*24*3600)`。
这是顶面入射、指定能窗内的期望自然曝光时间；不是计算机运行时间，也不是全年全部能量/侧面入射的事件数。
GT 已是 μ+ 与 μ− 总通量，不再乘电荷比；Phi 已包含角积分及投影因子，不再额外乘 pi。

历史10 GeV下限示例在 N=100000、半径 10 m、gamma=2.7 时，800×400 网格积分得到
Phi≈0.00201166893 cm^-2 s^-1，T≈5.01406144×10^-7 年，约 15.8232 秒。
时间短源于顶面面积约 314.16 m²；不代表输运计算也只需这些时间。
改变事件数、半径、能窗或 gamma 后，程序会重新计算，不使用固定换算系数。

开始输运前终端打印计划曝光时间，同时写入 `planned_exposure.txt`。成功完成后，
`metadata.txt` 保存 `equivalent_top_exposure_s`、`equivalent_top_exposure_years` 和 `seconds_per_year`。
中断时计划曝光不等于实际已完成曝光，不能用 planned 文件声称模拟全部完成。

历史独立 C++ 测试通过：10 GeV 下限、抽样定义域、网格积分收敛、事件数线性缩放与半径平方反比。
当前版本的完整 Geant4/ROOT 构建及输运仍需在 WSL 执行验证。

## 输出

- `planned_exposure.txt`：启动时计算的计划事例数对应自然曝光时间（秒、年）及能窗、面积参数。
- `primaries.csv`：所有入射事件的能量、角度、符号与位置，用于独立检查抽样。
- `nuclei.csv`：事件/轨迹/母轨迹编号、Z/A、核名称、生成过程、出生能量及 xyz、深度、射程、表状态。
- `summary.csv`：逐元素有效射程数、缺表数、越界数、射程图上下溢出数量。
- `muon_stalactite_full.root`：按 Z 分组的 XZ 计数、XZ 面密度、z–射程二维计数直方图。
- `Z*_xz.png/pdf`：X–Z 密度，N/m²，是沿 Y 投影，按本次模拟累计计数归一化；不是每秒体积率。
- `Z*_z_range.png/pdf`：SURF2 三维曲面，横轴 z、纵轴 CSDA 射程（对数轴）、高度 N/bin。默认射程分箱为 10^-3–10^7 nm（可在卡片调整）；上下溢出保存在 ROOT 直方图中并在 summary 中汇报。
- `config_used.mac`：启动时读取的原始配置卡副本，配合材料表和射程表保存可改善复现性。
- `metadata.txt`：种子、几何、阈值、物理表、有限能窗内的 GT 通量与等效顶面曝光时间。未计算通量模型系统误差。

不同元素有不同射程表覆盖，比较射程图之前先检查 summary。没有某元素产生时不生成空图。

## 不依赖 Geant4/ROOT 的抽样测试

```sh
g++ -std=c++17 -O2 -DSTALACTITE_SELF_TEST muon_stalactite_full.cc -o sampler_test
./sampler_test
```

或 `cmake -S . -B sampler_build -DSAMPLER_ONLY=ON`。
测试覆盖 GT 三分支、定义域、网格积分收敛、十万次抽样的能量和角度统计；不等于 Geant4 十万事件输运。
验证状态（2026-10-31 检查已有文件）：

- 独立 C++ 抽样及配置解析检查通过；配置解析检查包括重复键、未知键、负尺寸、NaN 和非法整数。
- WSL 的 `build-wsl/Testing/Temporary/LastTest.log` 记录 sampler 测试通过。
- 旧命令行版本的 `output_Halite_smoke_20261031_230828` 已生成 metadata、CSV、ROOT 和 Z11/Z16/Z17 的 PNG/PDF 文件。这说明此前版本已在用户环境产出绘图文件；该次检查未做图像外观验收。
- 旧版十万事件日志 `output_Halite_100k_20261031_231002.log` 检查时仅记录至 `Primary event 1000`，不能据此认定十万事件已完成。
- 上述 WSL 运行早于配置卡改动，不能替代当前版本验证。请先 `bash build.sh`，再用当前卡片完成 10 事件冒烟测试。


## 离线交互圆柱密度 HTML（2026-11-01）

`bash run.sh` 在模拟成功后调用 Python 3 标准库脚本，生成 `cylinder_density.html`。
无需 Plotly、网络或额外 pip 包，直接在浏览器打开；拖动旋转、滚轮缩放、悬停查看统计。
支持按元素选择、全部产生核/hadElastic 弹性过程切换、线性/对数颜色和密度下限筛选。
该图使用独立 Canvas 三维投影渲染，点是体素中心，不是单个核位置；外围线框为实际比例圆柱。

径向边界 `r_i=R*sqrt(i/nr)`、均匀方位角与 z 分箱，因此体素体积相同：
`V=pi*R²*L/(nr*nphi*nz)`。颜色为累计核数 N/V（N/m³），不是投影面密度或每秒产生率。
几何读取运行 metadata.txt，避免使用后来修改的卡片尺寸。所有元素计数守恒检查失败或坐标越界时报错。

配置卡新增 `/stalactite/htmlRadialBins`（10）、`htmlPhiBins`（24）、`htmlZBins`（50），各命令都要带 `/stalactite/` 前缀。
总分箱数不得超过 100000。修改这些参数无需重编译，但更新 C++ 参数解析代码后需先 build 一次。
改变当前参数重绘历史结果时可使用以下命令显式指定分箱，不必重新模拟：

```bash
python3 cylinder_density.py output_Halite_run01 --nr 10 --nphi 24 --nz 50
```

重新生成会覆盖同名派生 HTML，不改变原始 CSV/ROOT 数据。
生成器为 cylinder_density.py，模板为 cylinder_viewer.html，两者应一同保留。
若只直接运行 C++ 可执行文件，不经过 run.sh，则需手动执行上述绘图命令。

## 原始出生点散点 HTML

新增 `birth_points.html`：直接读取 nuclei.csv 的 x_m/y_m/z_m，每条记录对应一个点，无分箱、无随机抽样。
按元素着色，可选择元素与 hadElastic 过程、拖动旋转、滚轮缩放和调节点大小。
使用 WebGL 和内嵌数据，离线可打开；需要浏览器支持 WebGL，推荐 Edge/Chrome。
GPU 坐标使用 Float32，仅有浮点精度转换，未改变原始 CSV。
大量点可能相互遮挡，颜色深浅不是体积密度，不能与分箱图的 N/m³ 色标等同。
文件可能达几十至数百 MB，浏览器内存需求高于原体素图。

在本程序目录对既有输出生成，无需重跑 Geant4：

```bash
python3 ../render_birth_points.py output_Halite_run01
```

生成器与模板位于 Geant4_data/render_birth_points.py 和 birth_points_viewer.html。
打开输出目录里的 birth_points.html，不要打开模板。此功能目前为独立后处理，不改变原 run.sh。

## 出生方向直线径迹 HTML

新版本 nuclei.csv 末尾增加 dir_x、dir_y、dir_z，记录每条核轨迹第一次开始输运时的动量单位方向。
端点计算：r_end = r_birth + direction * range_nm * 1e-9（单位 m）。
射程来自已有能量—CSDA 射程表，并非实际多次散射轨迹或可观测损伤径迹。
默认按真实长度绘制；页面可设置线长放大倍率，倍率只影响显示，出生位置不变。
米级坐标上纳米线段受显示像素和 GPU 浮点精度限制，真实比例时可能看不见；放大后可超出靶，不表示实际离开靶的输运轨迹。
没有有效射程的核跳过并显示数量，不伪造长度或方向。使用全部有效记录，不随机抽样。

重新 build 后用新输出目录运行，run.sh 成功结束后自动生成 recoil_tracks.html。
旧 CSV 没有出生方向，无法事后恢复；生成器明确报错，不用随机方向代替。
对含方向的新数据单独重绘：`python3 ../render_recoil_tracks.py 输出目录`。
打开输出目录里的 recoil_tracks.html，模板 recoil_tracks_viewer.html 不含数据。
完整 WebGL 页面可能较大（每核两个顶点），需支持 WebGL 的浏览器。
历史开发检查通过单位转换、方向位移、HTML 生成及旧格式拒绝；尚未验证新版 Geant4 输运与浏览器画面。

### 径迹图几何参考与坐标

recoil_tracks.html 增加真实尺寸的靶体线框：beam 为立方体，大气程序为圆柱。
显示 X/Y/Z 三色轴、原点、正负坐标刻度和顶底面标注；小靶使用 cm，较大靶使用 m。
参考层与径迹使用同一旋转/缩放投影，不随径迹线长倍率变化，可用“几何与坐标”开关隐藏。
线框叠加显示用于定位，不表示不透明实体遮挡。仍按 d=L/2−z 解释向下深度。
已有结果可运行 `python3 ../render_recoil_tracks.py 输出目录` 更新 HTML，无需重新模拟。

### 出生点图的几何参考

birth_points.html 现增加实际靶体线框、X/Y/Z三色坐标轴、原点、刻度及顶底面标注。beam显示立方体，大气结果显示圆柱；小尺寸使用cm，其它使用m。参考与点云同步旋转缩放，可关闭“几何与坐标”。该修改不改变出生坐标、元素颜色或记录数。新生成的 HTML 自动带参考框；已有文件需重新生成。

### HTML 整体平移

所有体素密度、出生点、径迹 HTML 支持：左键拖动旋转，右键或中键拖动/Shift+左键拖动平移，滚轮缩放。几何参考与数据同步移动；重置视角同时清除平移。放大后用平移将感兴趣区域移到窗口中心，操作不改变任何物理坐标。已有 HTML 需重新生成以使用该功能。

## 已完成模拟的局部体积取样

在本目录运行 `python3 analyze_sample.py sample.mac`。这是独立后处理卡，不需编译或重新输运。
`/sample/input` 和 `/sample/output` 指定源运行目录及新的分析目录，路径相对卡片。`/sample/center_m x y z` 是相对靶中心的位置，`/sample/size_m dx dy dz` 是沿三个坐标轴的完整边长，单位 m。要求盒子完全位于原圆柱内。
按出生点落入闭区间选择全部核；只穿过盒子的外部出生径迹不入选。图中坐标以盒子中心为原点；CSV 的 global_x_m、global_y_m、global_z_m 保留靶坐标，depth_m 仍相对原靶顶面。CSDA 示意线可越过取样边界。没有有效射程时仍画出生点，旧数据缺少方向时仅生成出生点 HTML。
当前 sample.mac 读取 output_Halite_run04，取中心 (0,0,-45) m、尺寸 (10,10,10) m，输出 samples/run04_bottom_1000。历史示例读取 run03、中心 (0,0,49) m、尺寸 (1,1,2) m，结果目录 samples/run03_top 不随仓库分发。重新分析请确认输入并设置新的输出目录。

取样分析的暂存目录与结果目录同级，继承 samples 的访问权限，方便从 Windows 和 WSL 打开。

## 长期曝光的深度—径迹长度谱

在本目录运行 `python3 analyze_depth_tracks.py depth_tracks.mac`。默认用 run03 累计 47 万年，按顶面以下 80–100 m 每 5 m 生成一幅阶梯谱。所有核、Na、Cl 使用相同的 0–25 µm 线性分箱（每档 0.25 µm）和对数纵轴；所有核包含 Na、Cl。零计数档留空，超过上限的记录按层、元素另列。每档纵值为原始数 × 目标年数 / 模拟曝光年数 / 当层质量（mg），每行 CSV 同时给出原始计数和累计值。输出仍为 depth_tracks.html 和 depth_tracks.csv。

### 用 ROOT 重画 80–100 m 分层径迹谱

在本目录的 WSL 终端运行以下命令。输入为深度分析已生成的 CSV，run03 可将命令中的 run04 改为 run03。

```bash
source "${ROOT_PREFIX:?请先设置 ROOT_PREFIX}/bin/thisroot.sh"
root -l -b -q 'plot_depth_tracks.C("samples/run04_depth_tracks")'
```

同一取样目录生成 depth_tracks.pdf、depth_tracks.png 和 depth_tracks.root。四个面板每层厚 5 m，各含所有核、Na 和 Cl。每个 0.25 μm 分箱直接使用 CSV 中的 tracks_per_mg（累计径迹数/mg），曝光年数取自 CSV。超过绘图范围的计数密度保存在 ROOT 溢出箱内。

### 全射程分层谱（run04）

在本目录的 WSL 终端加载 ROOT 后运行：

```bash
root -l -b -q 'plot_depth_tracks_full.C()'
```

宏从 run04 的 nuclei.csv 提取深度 80–100 m 内射程有效且为正的核，按原 CSV 中的累计年份及每条记录的权重绘图。四层共用覆盖实际最小和最大值的 100 个等对数宽度分箱。纵轴为每毫克每个对数分箱的累计径迹数，箱宽以 μm 衡量时随射程增大。输出为 samples/run04_depth_tracks/depth_tracks_full.{png,pdf,root}。宏校验各层各组的总数和超过 25 μm 的计数。run04 的最长质子 CSDA 射程对应 Halite 质子表中多个高能点共有的 1.902963 m 平台值，描述材料中的射程估计，不代表实际在样品内走过的长度。
