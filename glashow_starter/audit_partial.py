"""中断数据的逐行描述统计：只统计已写出的前缀，不做每事件归一化。

支持 TSV 和压缩 TSV；遇到 gzip 尾部缺失会在结果里标记，而不是声称完成。
"""
import bisect
import collections
import json
import math
from pathlib import Path
import sys
from contextlib import closing
from data_io import partial_lines

run = Path(sys.argv[1])
out = Path(sys.argv[2])
meta = json.loads((run / 'run.json').read_text())
# 单位 MeV，固定分箱只用少量内存，不保留上千万条原始记录。
edges = [0, .0001, .001, .01, .1, 1, 10, 100, 1000, 1e4, 1e5, 1e6, 1e7, 1e10]
groups = {}
processes = collections.Counter()
parents = collections.Counter()
species = collections.Counter()
bad = 0
total = 0
excluded = 0
max_track = 0
max_energy_record = None
zmin, zmax, rmax = math.inf, -math.inf, 0
events = collections.Counter()
read_warnings = []
with closing(partial_lines(run, 'recoils.tsv', read_warnings)) as f:
    header = next(f).rstrip('\n').split('\t')
    if len(header) != 18:
        raise ValueError('Unexpected header')
    for line in f:
        a = line.rstrip('\n').split('\t')
        # 手工终止可切断最后一行；不尝试补齐缺失物理数据。
        if len(a) != 18 or not line.endswith('\n'):
            bad += 1
            continue
        try:
            eid, tid, z, mass = int(a[0]), int(a[1]), int(a[4]), int(a[5])
            e, x, y, zz = map(float, a[6:10])
            pa = int(a[17])
            if not all(math.isfinite(v) for v in (e,x,y,zz)) or e < 0:
                raise ValueError('nonfinite/negative')
        except ValueError:
            bad += 1
            continue
        total += 1
        events[eid] += 1
        max_track = max(max_track, tid)
        processes[a[14]] += 1
        parents[a[15]] += 1
        species[f'Z{z}_A{mass}'] += 1
        zmin, zmax, rmax = min(zmin,zz), max(zmax,zz), max(rmax, math.hypot(x,y))
        if max_energy_record is None or e > float(max_energy_record['energy_mev']):
            max_energy_record = dict(zip(header,a))
        # 与 analyze.py 相同的诊断筛选；另外始终保留 all_raw 的汇总。
        selected = 'decay' not in a[14].lower() and pa <= 4
        if not selected:
            excluded += 1
        names = ['all_raw']
        if selected:
            names += ['selected', 'NaCl' if (z,mass) in ((11,23),(17,35),(17,37)) else 'other_nuclei']
        for name in names:
            g = groups.setdefault(name, {'n':0,'min_mev':math.inf,'max_mev':0,'hist':[0]*(len(edges)+1)})
            g['n'] += 1
            g['min_mev'] = min(g['min_mev'],e)
            g['max_mev'] = max(g['max_mev'],e)
            g['hist'][bisect.bisect_right(edges,e)] += 1
        if total % 2000000 == 0:
            print('Read complete recoil rows:',total,flush=True)
result = {'source':str(run.resolve()),'complete':meta['complete'],
          'completed_events':meta['completed_events'],'raw_rows':total,'malformed_rows':bad,
          'read_warnings':read_warnings,
          'event_ids':dict(events),'excluded_rows':excluded,'max_track_id':max_track,
          'energy_hist_edges_mev':edges,'hist_convention':'underflow, [edge_i,edge_i+1), overflow',
          'groups':groups,'processes':dict(processes.most_common()),
          'parent_pdg':dict(parents.most_common()),'species':dict(species.most_common()),
          'max_energy_record':max_energy_record,'birth_extent_mm':{'z_min':zmin,'z_max':zmax,'r_max':rmax},
          'warning':'Partial transport history, not an unbiased sample or a completed-event yield. No duplicate-ID audit.'}
# 没有记录时用 null 而不是非标准 JSON 的 Infinity。
if total == 0:
    result['birth_extent_mm'] = None
out.write_text(json.dumps(result,indent=2,allow_nan=False),encoding='utf-8')
print('Saved',out,'rows',total,'malformed',bad,flush=True)
