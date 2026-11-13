"""完整事件的分析入口：反冲能谱 + 原射程表近似；不含通量/保存效率。

支持旧 .tsv 和新 .tsv.gz，不需要手工解压。中断事件请用 audit_partial.py。
启用出生动能截断时，本程序只能分析保留下来的反冲，无法恢复被删除的贡献。
"""
import argparse
import csv
import json
from pathlib import Path
import numpy as np
from data_io import open_data


def range_proxy_nm(energy_mev, table_path):
    """表的能量列为 MeV、射程列为微米；在对数坐标插值后转换为 nm。"""
    data = np.loadtxt(table_path, comments='#', ndmin=2)
    if data.shape[1] != 4 or not np.isfinite(data).all():
        raise ValueError(f'Invalid range table: {table_path}')
    e, _, _, r = data.T
    if np.any(e <= 0) or np.any(r <= 0) or np.any(np.diff(e) <= 0) or np.any(np.diff(r) <= 0):
        raise ValueError(f'Nonpositive/nonmonotone range table: {table_path}')
    energy = np.asarray(energy_mev, float)
    if not np.isfinite(energy).all() or np.any(energy < e[0]) or np.any(energy > e[-1]):
        raise ValueError(f'Energy outside range table: {table_path}')
    return np.exp(np.interp(np.log(energy), np.log(e), np.log(r))) * 1000.


def spectrum_per_event(values, edges, completed_events):
    """计数除以完成事件数，不除 bin 宽度；越界单独报告，不能偷偷归一化。"""
    if completed_events <= 0 or not np.isfinite(completed_events):
        raise ValueError('Completed W event count must be positive')
    values, edges = np.asarray(values, float), np.asarray(edges, float)
    if (values.ndim != 1 or edges.ndim != 1 or len(edges) < 2 or
            not np.isfinite(values).all() or not np.isfinite(edges).all() or
            np.any(np.diff(edges) <= 0)):
        raise ValueError('Invalid values or bin edges')
    counts, _ = np.histogram(values, edges)
    return counts / completed_events, int(len(values) - counts.sum())


def analyze(run_dir, range_dir):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt

    run_dir, range_dir = Path(run_dir), Path(range_dir)
    meta = json.loads((run_dir / 'run.json').read_text())
    if not meta.get('complete'):
        raise ValueError('Run is incomplete; do not normalize partial events')
    n = int(meta['completed_events'])
    # 完成账本是唯一归一化分母；不以“有反冲的事件数”代替。
    with open_data(run_dir, 'events.tsv') as f:
        events = list(csv.DictReader(f, delimiter='\t'))
    ids = {int(r['event_id']) for r in events}
    if n <= 0 or len(events) != n or len(ids) != n or n != meta['requested_events']:
        raise ValueError('Event ledger disagrees with metadata')

    groups = {'Na23': [], 'Cl35': [], 'Cl37': [], 'other_heavy': [], 'light_ions': []}
    nuclei = {(11, 23): 'Na23', (17, 35): 'Cl35', (17, 37): 'Cl37'}
    seen = set()
    skipped = {'decay_or_ion_descendant': 0}
    # 当前保留编号去重与能量数组，大样本仍需内存；gzip 只减少磁盘占用。
    with open_data(run_dir, 'recoils.tsv') as f:
        for row in csv.DictReader(f, delimiter='\t'):
            key = (int(row['event_id']), int(row['track_id']))
            if key in seen or key[0] not in ids:
                raise ValueError('Duplicate recoil or recoil outside completed ledger')
            seen.add(key)
            # 限定诊断样本，排除衰变和重核后代；raw 表仍完整保留。
            if 'decay' in row['creator'].lower() or int(row['parent_a']) > 4:
                skipped['decay_or_ion_descendant'] += 1
                continue
            z, a, energy = int(row['z']), int(row['a']), float(row['energy_mev'])
            if not np.isfinite(energy) or energy < 0:
                raise ValueError('Invalid recoil energy')
            name = nuclei.get((z, a), 'light_ions' if z <= 2 else 'other_heavy')
            groups[name].append(energy)
    if sum(int(r['n_recorded_nuclei']) for r in events) != len(seen):
        raise ValueError('Recoil row count disagrees with event ledger')

    out = run_dir / 'analysis'
    out.mkdir(exist_ok=True)
    energy_edges = np.geomspace(1e-4, 1e10, 141)
    length_edges = np.geomspace(1e-1, 1e9, 151)
    fig, axes = plt.subplots(1, 2, figsize=(12, 4.5))
    summary = {'completed_W_events': n, 'raw_recorded_nuclei': len(seen),
               'excluded': skipped, 'selection': 'non-decay births with parent_A <= 4',
               'units': 'count per completed hadronic W event per bin', 'species': {}}
    outputs = {'energy_edges_mev': energy_edges, 'length_edges_nm': length_edges,
               'n_completed_events': np.array(n), 'energy_gev': np.array(meta['energy_gev'])}
    for name, values in groups.items():
        values = np.asarray(values, float)
        spectrum, overflow = spectrum_per_event(values, energy_edges, n)
        outputs['energy_yield_' + name] = spectrum
        axes[0].stairs(spectrum, energy_edges, label=name)
        summary['species'][name] = {'selected_recoils': len(values),
                                   'energy_hist_outside': overflow}
        if name not in ('Na23', 'Cl35', 'Cl37'):
            continue
        z, a = next(k for k, v in nuclei.items() if v == name)
        table_path = range_dir / f'DEDX_Z{z}_A{a}.txt'
        if not table_path.exists():
            raise FileNotFoundError(f'Missing existing halite range table: {table_path}')
        table = np.loadtxt(table_path, comments='#', ndmin=2)
        # 不能把超出射程表的核用端点值代替，也不做未经验证的外推。
        covered = (values >= table[0, 0]) & (values <= table[-1, 0])
        lengths = range_proxy_nm(values[covered], table_path)
        y, outside = spectrum_per_event(lengths, length_edges, n)
        outputs['range_proxy_yield_' + name] = y
        axes[1].stairs(y, length_edges, label=name)
        summary['species'][name].update(range_table_outside=int((~covered).sum()),
                                        length_hist_outside=outside)
    for ax in axes:
        ax.set_xscale('log')
        ax.set_yscale('symlog', linthresh=0.1/max(n, 1))
        ax.set_ylabel('Count / hadronic W event / bin')
        ax.legend()
    axes[0].set_xlabel('Recoil birth kinetic energy [MeV]')
    axes[1].set_xlabel('Existing-table range proxy [nm]')
    axes[0].set_title('Selected nuclear births')
    axes[1].set_title('Na/Cl range proxy; NOT visible damage')
    fig.suptitle(f"E_nu={meta['energy_gev']/1e6:.3f} PeV; {n} completed W events")
    fig.tight_layout()
    fig.savefig(out / 'spectra.png', dpi=180)
    plt.close(fig)
    np.savez(out / 'response.npz', **outputs)
    (out / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary, indent=2))
    print(f'Wrote: {out / "spectra.png"}')
    print('This is a conditional response, not tracks/mg or a visible-track prediction.')


if __name__ == '__main__':
    # 默认相对源码寻找原项目数据；迁移设备后可用 --range-dir 指定新位置。
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_directory')
    parser.add_argument('--range-dir', default=str(Path(__file__).resolve().parent.parent /
                                                  'Data/Geant4_data/Halite/Range'))
    args = parser.parse_args()
    analyze(args.run_directory, args.range_dir)
