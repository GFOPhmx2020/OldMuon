#!/usr/bin/env python3
"""从 nuclei.csv 生成无需网络、无需第三方 Python 包的交互立方体体素密度 HTML。"""
import argparse
import csv
import json
import math
import shlex
from pathlib import Path


def aggregate(directory, nr=10, nphi=24, nz=50):
    # 几何取实际运行元数据，不读取可能已被修改的当前 config.mac。
    meta = dict(line.split("=", 1) for line in (directory / "metadata.txt").read_text().splitlines() if "=" in line)
    length = float(meta["side_m"]); radius = length/2
    if length<=0 or min(nr,nphi,nz)<=0 or nr*nphi*nz>100000:
        raise ValueError("Invalid cube geometry or bin counts")
    volume = length**3/(nr*nphi*nz)
    groups, outside, total = {}, 0, 0
    with (directory / "nuclei.csv").open(newline="") as stream:
        for row in csv.DictReader(stream):
            x, y, z = (float(row[k]) for k in ("x_m", "y_m", "z_m"))
            if not all(math.isfinite(v) for v in (x, y, z)):
                raise ValueError("核坐标包含非有限数值")
            total += 1
            if any(abs(v)>radius+1e-9 for v in (x,y,z)):
                outside += 1
                continue
            ir=min(nr-1,max(0,int((x+radius)/length*nr)))
            ip=min(nphi-1,max(0,int((y+radius)/length*nphi)))
            iz=min(nz-1,max(0,int((z+radius)/length*nz)))
            index = (iz*nphi+ip)*nr+ir
            # 保留全部产生核与 hadElastic 弹性过程两种口径，后者不代表只由 μ 直接产生。
            key = row["Z"]
            group = groups.setdefault(key, [{}, {}])
            group[0][index] = group[0].get(index, 0)+1
            if row["process"] == "hadElastic":
                group[1][index] = group[1].get(index, 0)+1
    if outside:
        raise ValueError(f"{outside}/{total} 条记录超出实际立方体几何，停止生成以免遗漏")
    assert sum(sum(g[0].values()) for g in groups.values()) == total
    return dict(radius=radius, length=length, nr=nr, nphi=nphi, nz=nz,
                volume=volume, total=total, material=meta.get("material", ""),
                events=meta.get("events", ""), groups=groups)


def write_html(directory, nr=10, nphi=24, nz=50):
    data = aggregate(directory, nr, nphi, nz)
    template = Path(__file__).with_name("cube_viewer.html").read_text(encoding="utf-8")
    # 将数据嵌入同一 HTML；转义 < 避免字符串结束 script 标签。
    payload = json.dumps(data, separators=(",", ":"), ensure_ascii=True).replace("<", "\\u003c")
    output = directory / "cube_density.html"
    output.write_text(template.replace("/*DATA*/null", payload), encoding="utf-8")
    print(f"Created {output}: {data['total']} nuclei, voxel volume {data['volume']:.6g} m^3")
    return output


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory",type=Path)
    parser.add_argument("--nx",type=int,default=20)
    parser.add_argument("--ny",type=int,default=20)
    parser.add_argument("--nz",type=int,default=20)
    a=parser.parse_args();write_html(a.directory,a.nx,a.ny,a.nz)
