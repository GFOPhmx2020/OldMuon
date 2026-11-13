"""比较同参数的完整输运结果；逐块解压校验，不把大表装入内存。"""
import argparse
import hashlib
import json
from pathlib import Path
from data_io import open_data


def check(before, after):
    before, after = Path(before), Path(after)
    old = json.loads((before / 'run.json').read_text())
    new = json.loads((after / 'run.json').read_text())
    for meta in (old, new):
        if not meta['complete'] or meta['completed_events'] != meta['requested_events']:
            raise ValueError('Both runs must have completed material transport')
    # 先核对物理输入；存储格式不同是允许的，不影响逐字节内容比较。
    for key in ('energy_gev', 'seed', 'requested_events', 'radius_m', 'length_m',
                'record_threshold_keV', 'geant4', 'pythia_version'):
        if old[key] != new[key]:
            raise ValueError(f'Inputs differ: {key}')
    for name in ('primaries.tsv', 'events.tsv', 'recoils.tsv', 'recoil_endings.tsv'):
        digests = []
        for directory in (before, after):
            h = hashlib.sha256()
            # 对解压后的文本取摘要，旧 TSV 与新 TSV.GZ 可以直接比较。
            with open_data(directory, name, 'rb') as f:
                for chunk in iter(lambda: f.read(1024 * 1024), b''):
                    h.update(chunk)
            digests.append(h.digest())
        if digests[0] != digests[1]:
            raise ValueError(f'Outputs differ: {name}')
        print(f'IDENTICAL: {name}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('before')
    parser.add_argument('after')
    args = parser.parse_args()
    check(args.before, args.after)
