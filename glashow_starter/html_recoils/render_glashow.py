"""读取 GlashowRun 输出，生成离线出生点及 CSDA 直线射程 HTML；不改变模拟。"""
import argparse
import base64
import bisect
import csv
import gzip
import json
import math
import random
import struct
from pathlib import Path


def finite(value):
    number = float(value)
    if not math.isfinite(number):
        raise ValueError('发现非有限数值')
    return number


def read_rows(directory):
    """流式读入，兼容压缩表；不容忍截断 gzip 或缺失字段。"""
    paths = [p for p in (directory / 'recoils.tsv', directory / 'recoils.tsv.gz') if p.exists()]
    if len(paths) != 1:
        raise ValueError('必须且只能存在一个 recoils.tsv 或 recoils.tsv.gz')
    opener = gzip.open if paths[0].suffix == '.gz' else open
    required = {'event_id', 'track_id', 'z', 'a', 'energy_mev', 'x_mm', 'y_mm', 'z_mm',
                'ux', 'uy', 'uz', 'creator', 'parent_a'}
    with opener(paths[0], 'rt', encoding='utf-8', newline='') as stream:
        reader = csv.DictReader(stream, delimiter='\t')
        if not required.issubset(reader.fieldnames or []):
            raise ValueError('核出生表缺少必要字段')
        for row in reader:
            if None in row or any(row.get(k) is None for k in required):
                raise ValueError('核出生表存在不完整行')
            yield row


def range_m(directory, z, a, energy, cache):
    """按同位素查原项目 Halite 表，双对数插值；微米转米，不外推。"""
    key = (z, a)
    if key not in cache:
        path = directory / ('DEDX_Z%d_A%d.txt' % key)
        if not path.exists():
            cache[key] = None
        else:
            pairs = []
            for line in path.read_text().splitlines():
                if not line.strip() or line.lstrip().startswith('#'):
                    continue
                values = [finite(v) for v in line.split()]
                if len(values) != 4 or values[0] <= 0 or values[3] <= 0:
                    raise ValueError('无效射程表: ' + str(path))
                pairs.append((values[0], values[3]))
            if len(pairs) < 2 or any(y[0] <= x[0] or y[1] < x[1] for x, y in zip(pairs, pairs[1:])):
                raise ValueError('射程表必须严格递增: ' + str(path))
            # 某些轻离子表高能端被固定成常数；保守地弃用平台及更高能部分。
            for i in range(1, len(pairs)):
                if pairs[i][1] == pairs[i-1][1]:
                    pairs = pairs[:i-1]
                    break
            if len(pairs) < 2:
                raise ValueError('射程表无足够可信的递增区间: ' + str(path))
            cache[key] = ([p[0] for p in pairs], [p[1] for p in pairs])
    table = cache[key]
    if table is None:
        return None, 'missing_table'
    energies, ranges = table
    if not energies[0] <= energy <= energies[-1]:
        return None, 'outside_table'
    i = min(max(bisect.bisect_right(energies, energy), 1), len(energies) - 1)
    t = math.log(energy / energies[i-1]) / math.log(energies[i] / energies[i-1])
    return math.exp(math.log(ranges[i-1]) + t * math.log(ranges[i] / ranges[i-1])) * 1e-6, None


def write_html(template, output, metadata, blob):
    text = Path(__file__).with_name(template).read_text(encoding='utf-8')
    # 转义 <，避免目录名称意外结束 script 标签。
    meta = json.dumps(metadata, ensure_ascii=True).replace('<', '\\u003c')
    text = text.replace('/*META*/null', meta).replace('/*BINARY*/', base64.b64encode(blob).decode('ascii'))
    output.write_text(text, encoding='utf-8')


def generate(args):
    directory = args.directory.resolve()
    meta = json.loads((directory / 'run.json').read_text(encoding='utf-8'))
    if meta.get('complete') is not True:
        raise ValueError('只接受已结束且 complete=true 的模拟；不要读取仍在写入的数据')
    length, radius = finite(meta['length_m']), finite(meta['radius_m'])
    if min(length, radius) <= 0 or args.max_points < 0 or args.min_mev < 0:
        raise ValueError('几何尺寸必须为正，筛选参数不得为负')
    if not args.range_dir.is_dir():
        raise ValueError('射程目录不存在，请用 --range-dir 指定 Halite/Range')
    selected = []
    rng = random.Random(12345)
    raw = accepted = 0
    for row in read_rows(directory):
        raw += 1
        energy = finite(row['energy_mev'])
        if energy < 0:
            raise ValueError('负出生动能')
        if args.event is not None and int(row['event_id']) != args.event:
            continue
        if energy < args.min_mev:
            continue
        if args.analysis_selection and ('decay' in row['creator'].lower() or int(row['parent_a']) > 4):
            continue
        # 保存最小字段；mm 转 m，元素编号 z 与空间坐标 z_mm 完全不同。
        xyz = tuple(finite(row[k]) * .001 for k in ('x_mm', 'y_mm', 'z_mm'))
        direction = tuple(finite(row[k]) for k in ('ux', 'uy', 'uz'))
        item = (xyz, direction, int(row['z']), int(row['a']), energy, int(row['creator'] == 'hadElastic'))
        accepted += 1
        # 均匀蓄水池抽样避免大数据占满内存。0 表示保存全部。
        if args.max_points == 0 or len(selected) < args.max_points:
            selected.append(item)
        else:
            i = rng.randrange(accepted)
            if i < args.max_points:
                selected[i] = item
    output = directory / 'html_recoils'
    output.mkdir(exist_ok=True)
    summary = dict(raw_rows=raw, eligible_rows=accepted, displayed_births=len(selected),
                   sampled=len(selected) < accepted, sampling_seed=12345,
                   event=args.event, min_mev=args.min_mev, analysis_selection=args.analysis_selection,
                   range_directory=str(args.range_dir.resolve()),
                   range_statistics_scope='displayed births only',
                   missing_table=0, outside_table=0, invalid_direction=0)
    label = ('均匀抽样 %d / %d' if summary['sampled'] else '显示 %d / %d') % (len(selected), accepted)
    common = dict(geometry='cylinder', length=length, width=radius*2, scale=max(length, radius*2)*.65,
                  name=directory.name + ' | ' + label,
                  dimensions='直径 %g m，长度 %g m；入射方向 +z' % (radius*2, length))
    points, lines = bytearray(), bytearray()
    point_counts, line_counts, cache = {}, {}, {}
    for xyz, direction, z, a, energy, elastic in selected:
        points.extend(struct.pack('<5f', *xyz, z, elastic))
        c = point_counts.setdefault(z, [0, 0]); c[0] += 1; c[1] += elastic
        distance, reason = range_m(args.range_dir, z, a, energy, cache)
        if reason:
            summary[reason] += 1
            continue
        norm = math.sqrt(sum(v*v for v in direction))
        if abs(norm-1) > 1e-4:
            summary['invalid_direction'] += 1
            continue
        delta = [v / norm * distance for v in direction]
        # 位移单独存储，减轻米级坐标与微小径迹相加时的 Float32 精度损失。
        lines.extend(struct.pack('<8f', *xyz, 0, 0, 0, z, elastic))
        lines.extend(struct.pack('<8f', *xyz, *delta, z, elastic))
        c = line_counts.setdefault(z, [0, 0]); c[0] += 1; c[1] += elastic
    nlines = len(lines) // 64
    summary['displayed_range_proxies'] = nlines
    write_html('birth_points_viewer.html', output / 'birth_points.html',
               dict(common, total=len(selected), counts=point_counts), points)
    write_html('recoil_tracks_viewer.html', output / 'recoil_tracks.html',
               dict(common, total=nlines, counts=line_counts, skipped=len(selected)-nlines), lines)
    (output / 'summary.json').write_text(json.dumps(summary, indent=2, ensure_ascii=False), encoding='utf-8')
    print(json.dumps(summary, indent=2, ensure_ascii=False))
    print('HTML output: ' + str(output))


if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=Path, help='含 run.json 和 recoils.tsv[.gz] 的模拟目录')
    p.add_argument('--range-dir', type=Path, default=Path(__file__).resolve().parents[2] / 'Data/Geant4_data/Halite/Range')
    p.add_argument('--max-points', type=int, default=200000, help='最多显示核数；0 为全部，可能占大量内存')
    p.add_argument('--event', type=int, help='只画某个 event_id；默认叠加全部事件')
    p.add_argument('--min-mev', type=finite, default=0., help='仅影响绘图的最低出生动能 MeV')
    p.add_argument('--analysis-selection', action='store_true', help='与 analyze.py 相同：排除 decay 或 parent_a>4')
    generate(p.parse_args())
