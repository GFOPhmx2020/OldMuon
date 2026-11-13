"""YAML 材料导出、配置卡运行与离线绘图；模拟失败时不生成完整结果图。"""
import argparse,shlex,subprocess,json
from pathlib import Path
from cube_density import write_html

def read_card(path):
    values={}
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        parts=shlex.split(line,comments=True)
        if parts:
            if len(parts)!=2 or parts[0] in values: raise ValueError("无效或重复卡片项")
            values[parts[0]]=parts[1]
    return values

def main():
    import yaml  # pip install PyYAML；完整 YAML 解析，支持用户后续编辑复杂 YAML。
    base=Path(__file__).resolve().parent
    parser=argparse.ArgumentParser()
    parser.add_argument("card",nargs="?",type=Path,default=base/"config.mac")
    parser.add_argument("--yaml",type=Path,default=base.parent.parent/"mineral_library.yaml")
    parser.add_argument("--nx",type=int,default=20);parser.add_argument("--ny",type=int,default=20);parser.add_argument("--nz",type=int,default=20)
    a=parser.parse_args();card=a.card.resolve();v=read_card(card)
    if min(a.nx,a.ny,a.nz)<=0 or a.nx*a.ny*a.nz>100000:raise ValueError("Invalid HTML bins")
    yaml_bytes=a.yaml.read_bytes()
    library=yaml.safe_load(yaml_bytes.decode("utf-8-sig"))
    rows=[]
    for name,m in library.items():
        if len(m["nuclei"])!=len(m["stoich"]) or not m["nuclei"]:raise ValueError("Invalid material composition")
        if float(m["density_g_cm3"])<=0 or any(float(n)<=0 for n in m["stoich"]):raise ValueError("Invalid density/stoichiometry")
        rows.append(" ".join([name,str(m["density_g_cm3"])]+[str(x) for pair in zip(m["nuclei"],m["stoich"]) for x in pair]))
    if v["/beam/material"] not in library and v["/beam/material"]!="StdRock":raise ValueError("材料未在 YAML 中定义")
    out=(card.parent/v["/beam/output"]).resolve()
    if out.exists():raise FileExistsError(f"输出目录已存在：{out}")
    table=(card.parent/v["/beam/materials"]).resolve()
    table.write_text("\n".join(rows)+"\n",encoding="utf-8")
    table_bytes=table.read_bytes()
    subprocess.run([str(base/"build-wsl/muon_beam"),str(card)],check=True)
    (out/"materials_used.tsv").write_bytes(table_bytes)
    (out/"mineral_library_used.yaml").write_bytes(yaml_bytes)
    write_html(out,a.nx,a.ny,a.nz)
    # 每条核记录对应一个出生坐标点，不分箱、不抽样。
    subprocess.run([__import__("sys").executable,str(base.parent/"render_birth_points.py"),str(out)],check=True)
    subprocess.run([__import__("sys").executable,str(base.parent/"render_recoil_tracks.py"),str(out)],check=True)
if __name__=="__main__":main()
