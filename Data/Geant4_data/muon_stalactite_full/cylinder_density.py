#!/usr/bin/env python3
"""从 nuclei.csv 生成无需网络、无需第三方 Python 包的交互圆柱体素密度 HTML。"""
import argparse
import csv
import json
import math
import shlex
from pathlib import Path


def aggregate(directory, nr=10, nphi=24, nz=50):
    # 几何取实际运行元数据，不读取可能已被修改的当前 config.mac。
    meta = dict(line.split("=", 1) for line in (directory / "metadata.txt").read_text().splitlines() if "=" in line)
    radius, length = float(meta["radius_m"]), float(meta["length_m"])
    if radius <= 0 or length <= 0 or min(nr, nphi, nz) <= 0:
        raise ValueError("几何尺寸和分箱数必须大于零")
    if nr * nphi * nz > 100000:
        raise ValueError("交互图分箱总数不得超过 100000")
    # 等体积径向分箱 r_i=R sqrt(i/nr)，每个角扇区与 z 层体积一致。
    # 体积 = (r_out²-r_in²)/2 * delta_phi * delta_z；不把外包矩形当体积。
    volume = math.pi * radius**2 * length / (nr * nphi * nz)
    groups, outside, total = {}, 0, 0
    with (directory / "nuclei.csv").open(newline="") as stream:
        for row in csv.DictReader(stream):
            x, y, z = (float(row[k]) for k in ("x_m", "y_m", "z_m"))
            if not all(math.isfinite(v) for v in (x, y, z)):
                raise ValueError("核坐标包含非有限数值")
            r2 = x*x+y*y
            total += 1
            if r2 > radius**2*(1+1e-9) or abs(z) > length/2+1e-8:
                outside += 1
                continue
            ir = min(nr-1, int(r2/radius**2*nr))
            ip = min(nphi-1, int((math.atan2(y,x) % (2*math.pi))/(2*math.pi)*nphi))
            iz = min(nz-1, max(0, int((z+length/2)/length*nz)))
            index = (iz*nphi+ip)*nr+ir
            # 保留全部产生核与 hadElastic 弹性过程两种口径，后者不代表只由 μ 直接产生。
            key = row["Z"]
            group = groups.setdefault(key, [{}, {}])
            group[0][index] = group[0].get(index, 0)+1
            if row["process"] == "hadElastic":
                group[1][index] = group[1].get(index, 0)+1
    if outside:
        raise ValueError(f"{outside}/{total} 条记录超出实际圆柱几何，停止生成以免遗漏")
    assert sum(sum(g[0].values()) for g in groups.values()) == total
    return dict(radius=radius, length=length, nr=nr, nphi=nphi, nz=nz,
                volume=volume, total=total, material=meta.get("material", ""),
                events=meta.get("events", ""), groups=groups)


def write_html(directory, nr=10, nphi=24, nz=50):
    data = aggregate(directory, nr, nphi, nz)
    template = Path(__file__).with_name("cylinder_viewer.html").read_text(encoding="utf-8")
    # 将数据嵌入同一 HTML；转义 < 避免字符串结束 script 标签。
    payload = json.dumps(data, separators=(",", ":"), ensure_ascii=True).replace("<", "\\u003c")
    output = directory / "cylinder_density.html"
    output.write_text(template.replace("/*DATA*/null", payload), encoding="utf-8")
    print(f"Created {output}: {data['total']} nuclei, voxel volume {data['volume']:.6g} m^3")
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", nargs="?", type=Path, help="含 nuclei.csv 和 metadata.txt 的运行目录")
    parser.add_argument("--nr", type=int, default=10)
    parser.add_argument("--nphi", type=int, default=24)
    parser.add_argument("--nz", type=int, default=50)
    parser.add_argument("--card", type=Path, help="运行配置卡；从卡读取输出目录和 HTML 分箱")
    args = parser.parse_args()
    if args.card:
        params = {}
        for line in args.card.read_text(encoding="utf-8-sig").splitlines():
            fields = shlex.split(line, comments=True)
            if fields:
                params[fields[0]] = fields[1]
        args.directory = (args.card.resolve().parent / params["/stalactite/output"]).resolve()
        # 使用完成运行保存的卡片快照，避免运行期间更改卡片影响绘图分箱。
        saved = args.directory / "config_used.mac"
        if saved.exists():
            params = {}
            for line in saved.read_text(encoding="utf-8-sig").splitlines():
                fields = shlex.split(line, comments=True)
                if fields:
                    params[fields[0]] = fields[1]
        args.nr = int(params.get("/stalactite/htmlRadialBins", 10))
        args.nphi = int(params.get("/stalactite/htmlPhiBins", 24))
        args.nz = int(params.get("/stalactite/htmlZBins", 50))
    if args.directory is None:
        parser.error("提供输出目录或 --card 配置卡")
    write_html(args.directory, args.nr, args.nphi, args.nz)
    # 正式运行入口自动生成径迹；手动重绘旧密度图不要求历史 CSV 有方向。
    if args.card:
        import subprocess,sys
        subprocess.run([sys.executable,str(Path(__file__).resolve().parent.parent/"render_recoil_tracks.py"),str(args.directory)],check=True)
