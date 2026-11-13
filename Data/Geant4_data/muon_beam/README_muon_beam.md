# muon_beam：单能粒子束流照射岩石立方体

运行前需在 WSL/Linux 中安装并加载 Geant4、ROOT 环境及所需 Geant4 数据包。`PROJECT_ROOT` 表示仓库根目录；`ROOT_PREFIX` 表示本机 ROOT 安装目录，例如设置 `export ROOT_PREFIX=/path/to/root` 后执行 `source "$ROOT_PREFIX/bin/thisroot.sh"`。这里的路径需按本机安装替换，ROOT 环境变量不等同于项目根目录。

文中 `output_*`、run03/run04、`samples/` 及历史日志是本地运行示例，不随源码仓库分发。后处理前应先生成或取得对应数据，再修改配置卡中的输入、输出路径。所有历史验证记录仅说明当时的版本；本次文档整理未重新编译或运行 Geant4/ROOT。

## WSL 一键构建与运行

```bash
# 从克隆后的仓库根目录开始
export PROJECT_ROOT="$PWD"
cd "$PROJECT_ROOT/primus-paleo/Data/Geant4_data/muon_beam"
bash build.sh
bash run.sh
```

独立 CMake 工程、可执行文件 muon_beam、build-wsl 与输出目录，不复用或删除 muon_stalactite_full 的构建/数据。
当前配置卡为 O18 离子束（Z=8、A=18、Q=6），总动能46.8 GeV，即2.6 GeV/u，共10000事件；首次建议改 config.mac 的 events 为10、output为新目录做冒烟测试。
构建默认8个编译任务；可 `BUILD_JOBS=4 bash build.sh`。程序输运为单线程。

## 配置卡

下表对应当前提交的 config.mac。历史 mu-、100 GeV、100000事件设置属于此前运行示例；运行前应检查实际卡片，输出目录必须是新目录。

所有命令为本程序专用 `/beam/` 命令，不支持任意 Geant4 UI 宏。修改卡片无需重新编译。

| 参数（均带 /beam/ 前缀） | 默认值 | 含义 |
|---|---|---|
| material | Halite | YAML 材料名称 |
| particle | ion | 支持 mu- / mu+ / proton / ion |
| ionZ / ionA / ionQ | 8 / 18 / 6 | 离子核电荷数、质量数及电荷态；ion 模式要求 Z≥1、A≥Z、1≤Q≤Z |
| energy_GeV | 46.8 | 单个入射粒子的总动能 GeV；当前 O18 对应2.6 GeV/u |
| side_cm | 10 | 立方体边长 cm |
| spotRadius_cm | 3 | 均匀圆盘束斑半径 cm，不超过半边长 |
| events / seed | 10000 / 20261101 | 初级事件数、随机种子 |
| output | output_Halite_O18_2p6GeVu_01 | 新建输出目录，不覆盖已有目录 |
| data / materials | .. / materials.tsv | 射程数据根目录、运行时生成的材料表 |
| recordThreshold_MeV | 0.001 | 只记录出生动能 >1 keV 的次级核 |
| productionCut_mm | 0.005 | Geant4 生产 cut，不等于记录阈值 |
| xBins / zBins | 80 / 100 | ROOT XZ 分箱 |

束流固定沿 -z，从 z=+5 cm 上方1 μm的真空处入射，x/y 在半径3 cm圆盘内按面积均匀。
本程序不使用 GT，也不推算自然时间：单能束流的流率尚未给定。

## 材料来源

run_beam.py 每次从 `../../mineral_library.yaml`（相对于本目录）读取项目材料，支持 Halite、Olivine、Quartz、Spinel、Apatite、Orthopyroxene、Clinopyroxene、Diamond、Calcite、Obsidian。
密度和化学计量自动转换为 Geant4 材料输入，非整数配比按原子量转换质量分数。
另保留项目旧 StdRock 定义作为补充，几何仍为卡片规定的立方体。
指定另一 YAML：`bash run.sh config.mac --yaml /path/mineral_library.yaml`。
更改材料组成请编辑 YAML，生成的 materials.tsv 会被运行脚本更新；成功输出保存材料表与 YAML 副本。
卡片中的所有路径相对卡片目录；默认 YAML 相对程序目录。

## 物理和输出口径

物理表 QGSP_BIC_HP + EM option4。保留后代输运，不以记录阈值杀死轨迹。
在次级核开始输运时记录一次，包含质子、轻核、残余核、反应产物；不是仅 μ 直接产生或仅弹性反冲。
CSV 保留 process/parent/track，HTML 可选 hadElastic，但无法据此推断母粒子种类。

- primaries.csv：束流事件、符号、动能、方向及位置。
- nuclei.csv：核 Z/A、出生位置、动能、生成过程、CSDA 射程与状态。
- metadata.txt、config_used.mac、materials_used.tsv、mineral_library_used.yaml：运行配置与材料快照。
- muon_beam.root、Z*_xz.png/pdf：按元素统计的 XZ 图；颜色为沿Y投影的累计 N/m²，不是 N/m³。
- Z*_z_range.png/pdf：额外保留已有射程图功能；射程来自项目 Range 表，非实际损伤径迹长度。
- summary.csv：按元素计数、缺失射程表/越界状态。
- cube_density.html：离线可旋转立方体图，体素中心点颜色为 N/V（N/m³），可按元素、弹性过程筛选。

HTML 默认20×20×20体素，每格体积(0.1m)^3/8000。支持旋转、缩放、悬停、对数色标，无需网络。
打开输出目录里的 cube_density.html，不要直接打开 cube_viewer.html 模板。
可设置HTML分箱：`bash run.sh config.mac --nx 25 --ny 25 --nz 25`。
对既有数据重绘：`python3 cube_density.py output_Halite_beam01 --nx 20 --ny 20 --nz 20`，无需重跑 Geant4。
材料可输运不代表其所有核素都有射程表；缺表不影响 XZ 和 HTML 计数。

## 验证与维护

历史开发记录：已验证 Python 立方体体积、几何边界、元素/过程计数与越界拒绝。
完整 Geant4/ROOT 编译与十万事件模拟在该历史记录中未执行；请在WSL先进行10事件冒烟测试。
后续修改本程序、配置、脚本、输出或统计口径时同步更新本 README_muon_beam.md。

## 原始出生点散点 HTML

新增 `birth_points.html`：直接读取 nuclei.csv 的 x_m/y_m/z_m，每条记录对应一个点，无分箱、无随机抽样。
按元素着色，可选择元素与 hadElastic 过程、拖动旋转、滚轮缩放和调节点大小。
使用 WebGL 和内嵌数据，离线可打开；需要浏览器支持 WebGL，推荐 Edge/Chrome。
GPU 坐标使用 Float32，仅有浮点精度转换，未改变原始 CSV。
大量点可能相互遮挡，颜色深浅不是体积密度，不能与分箱图的 N/m³ 色标等同。
文件可能达几十至数百 MB，浏览器内存需求高于原体素图。

在本程序目录对既有输出生成，无需重跑 Geant4：

```bash
python3 ../render_birth_points.py output_Halite_beam01
```

生成器与模板位于 Geant4_data/render_birth_points.py 和 birth_points_viewer.html。
打开输出目录里的 birth_points.html，不要打开模板。束流 run.sh 现已自动调用该后处理。

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

### 束流出生点图自动输出

运行成功后自动生成 cube_density.html（体素密度）、birth_points.html（原始出生坐标散点）和 recoil_tracks.html（方向与射程线段）。出生点图直接使用全部核的 xyz，不分箱、不随机抽样。已有结果补图无需重编译或重新模拟：`python3 ../render_birth_points.py 输出目录`。

### 出生点图的几何参考

birth_points.html 现增加实际靶体线框、X/Y/Z三色坐标轴、原点、刻度及顶底面标注。beam显示立方体，大气结果显示圆柱；小尺寸使用cm，其它使用m。参考与点云同步旋转缩放，可关闭“几何与坐标”。该修改不改变出生坐标、元素颜色或记录数。新生成的 HTML 自动带参考框；已有文件需重新生成。

### HTML 整体平移

所有体素密度、出生点、径迹 HTML 支持：左键拖动旋转，右键或中键拖动/Shift+左键拖动平移，滚轮缩放。几何参考与数据同步移动；重置视角同时清除平移。放大后用平移将感兴趣区域移到窗口中心，操作不改变任何物理坐标。已有 HTML 需重新生成以使用该功能。
