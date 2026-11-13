# Glashow 核出生点与直线射程 HTML

本目录是独立后处理工具。没有修改 C++、阈值、物理模型、原分析程序或运行脚本；不需要重新编译或重新模拟。Python 3 标准库即可运行，不依赖 numpy、ROOT、Geant4、网络或图形桌面。

## 集群操作

从仓库根目录设置 `export PROJECT_ROOT="$PWD"`，保持三个代码文件放在一起。输入模拟结果需另行生成或获取。

```bash
cd "$PROJECT_ROOT/glashow_starter/html_recoils"
python3 render_glashow.py ../results/my_new_bulk06
```

输入目录必须有 complete=true 的 run.json，以及 recoils.tsv 或 recoils.tsv.gz。两种表同时存在时会报错，避免误读旧文件。默认使用项目的 Data/Geant4_data/Halite/Range 射程表，不复制模拟数据或射程表。

若射程表位置不同：

```bash
python3 html_recoils/render_glashow.py results/my_new_bulk06 --range-dir /你的项目路径/Data/Geant4_data/Halite/Range
```

输出在该模拟目录的 html_recoils 子目录：

- birth_points.html：已记录核的出生位置，按元素着色。
- recoil_tracks.html：出生位置 + 出生方向 × 表中射程的直线示意。
- summary.json：输入数、筛选数、抽样数、缺失射程表/超出有效范围/方向无效的数量。射程统计仅针对显示样本。

在 VS Code 远程资源管理器中下载两个 HTML 到本地，用 Chrome/Edge 打开；每个 HTML 均独立、自带数据。左键旋转、右键或 Shift+左键平移、滚轮缩放；支持元素和 hadElastic 筛选、几何线框、径迹倍率。倍率只放大线长，不移动出生点。默认倍率 1 时，米级靶中的短线可能看不见。重复执行会覆盖这三个后处理输出，不改模拟表。

## 筛选和内存

默认叠加所有事件、包含所有已记录核（包括轻离子），不使用 analyze.py 的额外筛选。最多均匀抽样显示 200000 个核，固定随机种子 12345；页面明确标示抽样数量。抽样只影响 HTML，不修改原始文件。不做每事件或体积归一化。

```bash
# 只显示 event_id=0，出生动能不低于 500 MeV
python3 html_recoils/render_glashow.py results/my_new_bulk06 --event 0 --min-mev 500
# 与 analyze.py 的核选择条件一致
python3 html_recoils/render_glashow.py results/my_new_bulk06 --analysis-selection
# 全部绘制：大文件可能占用大量 Python 和浏览器内存
python3 html_recoils/render_glashow.py results/my_new_bulk06 --max-points 0
```

--min-mev 仅是显示筛选，不修改模拟的产生/追踪/记录阈值，不能恢复模拟中被删除或未记录的核。原始文本逐行读取；内存主要由显示样本决定。

## 物理含义及限制

复用本地 Data/Geant4_data 的两个 WebGL 查看器，输入适配为集群 TSV 字段：x_mm/y_mm/z_mm 转米；z/a 为元素及质量数；ux/uy/uz 为出生方向；energy_mev 为出生动能。按同位素查 DEDX_Z*_A*.txt，双对数插值 Range_um，再转米。缺表和越界均跳过，绝不外推或用表端点代替。部分轻离子表高能端存在常数平台，本程序保守排除首次平台涉及的点及后续部分，将其计入 outside_table。

射程线不是 Geant4 真实逐步轨迹，不是已保存的损伤径迹，也没有包含追踪阈值、后续核反应、散射或离开靶体的效果。已有 recoil_endings.tsv 只有终点及累计路程，无法恢复实际曲线路径。本工具没有将终点或累计路程冒充完整路径。射程线可能延伸出靶体，且此图不能验证高能物理模型。出生点图非空但射程图为空可能只是缺表/越界，查看 summary.json 即可区分。

## 工具文件

- render_glashow.py：读表、筛选/有界抽样、单位转换、查表、嵌入 HTML。
- birth_points_viewer.html：复用本地出生点查看器，修正 TSV/抽样说明及 +z 入射方向标签。
- recoil_tracks_viewer.html：复用本地直线射程查看器，修正射程来源和方向标签。
- README核出生与反冲.md：本文。
