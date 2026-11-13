# muon_stalactite.ipynb

读取已有 Geant4 六列 outNuclei_*.txt，按材料/入射能量统计产生核沿 z 的线密度，使用 PyROOT 绘制曲面及其它图形。
旧数据只有 z，不能重建真实 XZ/XYZ 空间分布；其三维曲面是 z–入射能量–密度，不是空间三维。
在具有 NumPy、PyROOT 的 Jupyter 内核中从仓库根目录打开 muon_stalactite.ipynb 并逐单元运行。
默认原始计数密度，只有核实各文件初级粒子数后才能解释为每入射粒子的产额。
真实空间数据请使用 Data/Geant4_data/muon_stalactite_full 或 muon_beam，分别参阅其按主程序命名的 README。
修改 notebook 行为、默认参数或输出时同步维护本文档。

输入 outNuclei 数据须自行生成或由合作者提供；精简发布包不默认包含批量原始模拟。检查 Notebook 中的数据路径及初级粒子数，再运行分析。
