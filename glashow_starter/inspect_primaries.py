"""只读检查 PYTHIA 实验室系初级粒子表；仅依赖 Python 标准库。

用法：python inspect_primaries.py results/运行目录 [--all]
退出码：0=输入表检查通过；1=检查失败或文件不完整。
这不证明 Geant4 的输运或物理模型正确。
"""
import argparse
import csv
import gzip
import json
import math
from collections import Counter
from pathlib import Path

NAMES = {22: "gamma", 11: "e-", -11: "e+", 13: "mu-", -13: "mu+",
         211: "pi+", -211: "pi-", 111: "pi0", 321: "K+", -321: "K-",
         130: "K_L", 310: "K_S", 2212: "proton", -2212: "antiproton",
         2112: "neutron", -2112: "antineutron", 3122: "Lambda",
         12: "nu_e", -12: "anti_nu_e", 14: "nu_mu", -14: "anti_nu_mu"}


def inspect(run, show_all=False):
    """每个事件只保留其初级粒子，避免加载庞大的反冲核输出。"""
    meta = json.loads((run / "run.json").read_text(encoding="utf-8"))
    paths = [p for p in (run / "primaries.tsv", run / "primaries.tsv.gz") if p.exists()]
    if len(paths) != 1:
        raise ValueError("必须且只能存在一个 primaries.tsv 或 primaries.tsv.gz")
    expected = float(meta["energy_gev"])
    me = float(meta["electron_mass_gev"])
    if not (math.isfinite(expected) and expected > 0 and math.isfinite(me) and me > 0):
        raise ValueError("run.json 中能量或电子质量无效")
    print(f"输入：{paths[0]}\n入射能量：{expected:g} GeV = {expected/1e6:g} PeV")
    print(f"运行 complete={meta.get('complete')}，completed_events={meta.get('completed_events')}")
    print("注意：这里检查保存的传入四动量，不是 Geant4 实际追踪日志。")
    failures = 0
    events = 0
    current = None
    rows = []

    def report(event, particles):
        nonlocal failures, events
        events += 1
        energy, px, py, pz = [math.fsum(r[k] for r in particles) for k in (2, 3, 4, 5)]
        tol = 1e-6 * (expected + me)
        good = (abs(energy-expected-me) <= tol and abs(pz-expected) <= tol
                and abs(px) <= 1e-5 and abs(py) <= 1e-5)
        failures += not good
        counts = Counter(r[1] for r in particles)
        print(f"\n事件 {event}：{len(particles)} 个初级粒子；四动量检查 {'PASS' if good else 'FAIL'}")
        print(f"  ΣE={energy:.12g} GeV，Σp=({px:.6g}, {py:.6g}, {pz:.12g}) GeV/c")
        print(f"  ΔE={energy-expected-me:.6g} GeV，Δpz={pz-expected:.6g} GeV/c")
        print("  粒子种类：" + ", ".join(f"{NAMES.get(p, 'PDG='+str(p))}:{n}" for p, n in counts.items()))
        for threshold in (1, 1e3, 1e5, 1e6):
            selected = [r for r in particles if r[2] >= threshold]
            print(f"  总能量 ≥ {threshold:g} GeV：{len(selected)} 个，占总能量 {sum(r[2] for r in selected)/energy:.2%}")
        print("  index  PDG       名称          E[GeV]        px          py          pz[GeV/c]  theta[deg]")
        ordered = sorted(particles, key=lambda r: r[2], reverse=True)
        for idx, pdg, e, x, y, z in (ordered if show_all else ordered[:10]):
            theta = math.degrees(math.atan2(math.hypot(x, y), z))
            print(f"  {idx:5d} {pdg:9d} {NAMES.get(pdg, 'unknown'):12s} {e:12.6g} {x:11.5g} {y:11.5g} {z:12.6g} {theta:10.5g}")

    opener = gzip.open if paths[0].suffix == ".gz" else open
    with opener(paths[0], "rt", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream, delimiter="\t")
        if reader.fieldnames != ["event_id", "index", "pdg", "px_gev", "py_gev", "pz_gev", "energy_gev"]:
            raise ValueError("初级粒子表头不匹配")
        for row in reader:
            event, idx, pdg = (int(row[k]) for k in ("event_id", "index", "pdg"))
            e, x, y, z = (float(row[k]) for k in ("energy_gev", "px_gev", "py_gev", "pz_gev"))
            if not all(map(math.isfinite, (e, x, y, z))) or e <= 0:
                raise ValueError(f"事件 {event} 存在无效四动量")
            # 用 E 与 |p| 比较，避免在 PeV 区直接相减平方造成质量重建消减误差。
            if math.hypot(x, y, z) > e + max(1e-8, 1e-9*e):
                raise ValueError(f"事件 {event} 粒子 {idx} 出现 |p| > E")
            if event != current:
                if current is not None:
                    report(current, rows)
                    if event <= current:
                        raise ValueError("事件编号重复或未按递增顺序排列")
                current, rows = event, []
            if idx != len(rows):
                raise ValueError(f"事件 {event} 的 index 不连续或重复")
            rows.append((idx, pdg, e, x, y, z))
    if rows:
        report(current, rows)
    if not events:
        raise ValueError("没有初级粒子记录")
    if "generated_events" in meta and events != int(meta["generated_events"]):
        print("FAIL：表中事件数量与 generated_events 不一致，可能运行中或文件不完整。")
        failures += 1
    print(f"\n合计：{events} 个已保存事件，{failures} 项失败。")
    print("总能量包含静质量；未从表中猜测粒子质量、动能或电荷。未知 PDG 仍纳入检查。")
    return 1 if failures else 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path, help="含 run.json 和 primaries.tsv[.gz] 的结果目录")
    parser.add_argument("--all", action="store_true", help="列出全部粒子，默认每事件能量最高的10个")
    args = parser.parse_args()
    try:
        raise SystemExit(inspect(args.run, args.all))
    except (OSError, EOFError, ValueError, KeyError, TypeError, csv.Error) as exc:
        print(f"检查失败：{exc}")
        raise SystemExit(1)
