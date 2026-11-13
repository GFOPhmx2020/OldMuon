"""按出生位置/动量方向及已有 CSDA 射程生成直线径迹 HTML。"""
import base64,csv,json,math,struct,argparse
from pathlib import Path

def displacement(row):
    direction=[float(row[k]) for k in ("dir_x","dir_y","dir_z")]
    norm=math.sqrt(sum(v*v for v in direction))
    if not math.isfinite(norm) or abs(norm-1)>1e-5:raise ValueError("Invalid birth direction; expected unit vector")
    length=float(row["range_nm"])*1e-9
    if not math.isfinite(length) or length<0:raise ValueError("Invalid range")
    return [v/norm*length for v in direction]

def generate(directory, *, view=None):
    directory=Path(directory)
    meta=dict(line.split("=",1) for line in (directory/"metadata.txt").read_text().splitlines() if "=" in line)
    length=float(meta["length_m"]);width=float(meta.get("side_m",2*float(meta.get("radius_m",0))))
    blob=bytearray();counts={};total=skipped=0
    with (directory/"nuclei.csv").open(newline="") as stream:
        reader=csv.DictReader(stream)
        if not {"dir_x","dir_y","dir_z"}.issubset(reader.fieldnames or []):
            raise ValueError("旧 nuclei.csv 未保存出生动量方向，不能恢复真实方向。请重新 build 后用新输出目录模拟；禁止用随机方向代替。")
        for row in reader:
            if row["range_status"]!="ok":skipped+=1;continue
            xyz=[float(row[k]) for k in ("x_m","y_m","z_m")]
            if not all(math.isfinite(v) for v in xyz):raise ValueError("Non-finite position")
            delta=displacement(row);z=int(row["Z"]);elastic=int(row["process"]=="hadElastic")
            # 两端分别保存出生坐标与位移，避免先加纳米位移再存 Float32 导致端点信息丢失。
            blob.extend(struct.pack("<8f",*xyz,0,0,0,z,elastic))
            blob.extend(struct.pack("<8f",*xyz,*delta,z,elastic))
            c=counts.setdefault(z,[0,0]);c[0]+=1;c[1]+=elastic;total+=1
    assert len(blob)==total*64
    payload=dict(geometry="cube" if "side_m" in meta else "cylinder",length=length,width=width,name=directory.name,total=total,skipped=skipped,counts=counts,scale=max(length,width)*.65,dimensions=f"横向 {width:g} m，高 {length:g} m")
    if view is not None:
        payload.update(view)
    template=Path(__file__).with_name("recoil_tracks_viewer.html").read_text(encoding="utf-8")
    html=template.replace("/*META*/null",json.dumps(payload,ensure_ascii=True)).replace("/*BINARY*/",base64.b64encode(blob).decode())
    output=directory/"recoil_tracks.html";output.write_text(html,encoding="utf-8")
    print(f"Created {output}: {total} tracks, skipped {skipped}")
if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("directories",nargs="+",type=Path)
    for directory in p.parse_args().directories:generate(directory)
