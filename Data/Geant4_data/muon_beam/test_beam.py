"""最小回归测试：立方体体积、边界、元素/过程计数与越界拒绝。"""
from pathlib import Path
import tempfile
from cube_density import aggregate
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/"metadata.txt").write_text("side_m=0.1\n")
    (p/"nuclei.csv").write_text("x_m,y_m,z_m,Z,process\n0,0,0,11,hadElastic\n0.05,-0.05,0.05,17,neutronInelastic\n")
    d=aggregate(p,10,10,10)
    assert d["total"]==2 and abs(d["volume"]-1e-6)<1e-15
    assert sum(sum(g[1].values()) for g in d["groups"].values())==1
    (p/"nuclei.csv").write_text("x_m,y_m,z_m,Z,process\n0.06,0,0,11,hadElastic\n")
    try:aggregate(p)
    except ValueError:pass
    else:raise AssertionError("Outside cube accepted")
print("PASS: cube volume, boundary, count conservation and outside rejection")
