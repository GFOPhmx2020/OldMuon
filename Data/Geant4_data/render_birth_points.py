"""将已有核 CSV 的全部出生点嵌入独立 WebGL HTML，不分箱、不随机抽样。"""
import base64,csv,json,math,struct,argparse
from pathlib import Path

def generate(directory, *, view=None):
    directory=Path(directory)
    meta=dict(line.split("=",1) for line in (directory/"metadata.txt").read_text().splitlines() if "=" in line)
    length=float(meta["length_m"])
    width=float(meta.get("side_m",2*float(meta.get("radius_m",0))))
    blob=bytearray();counts={};total=0
    with (directory/"nuclei.csv").open(newline="") as stream:
        for row in csv.DictReader(stream):
            xyz=[float(row[k]) for k in ("x_m","y_m","z_m")]
            if not all(math.isfinite(v) for v in xyz):raise ValueError("Non-finite position")
            z=int(row["Z"]);elastic=int(row["process"]=="hadElastic")
            blob.extend(struct.pack("<5f",*xyz,z,elastic))
            c=counts.setdefault(z,[0,0]);c[0]+=1;c[1]+=elastic;total+=1
    assert len(blob)==total*20 and sum(c[0] for c in counts.values())==total
    payload=dict(geometry="cube" if "side_m" in meta else "cylinder",length=length,width=width,name=directory.name,total=total,counts=counts,scale=max(length,width)*.65,dimensions=f"横向 {width:g} m，高 {length:g} m")
    if view is not None:
        payload.update(view)
    template=Path(__file__).with_name("birth_points_viewer.html").read_text(encoding="utf-8")
    html=template.replace("/*META*/null",json.dumps(payload,ensure_ascii=True)).replace("/*BINARY*/",base64.b64encode(blob).decode())
    output=directory/"birth_points.html";output.write_text(html,encoding="utf-8")
    print(f"{output}: {total} points, {output.stat().st_size/1e6:.1f} MB")
    return total
if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("directories",nargs="+",type=Path)
    for directory in parser.parse_args().directories:generate(directory)
