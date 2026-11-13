"""按靶内出生位置筛选已完成的模拟，使用局部坐标绘制出生点与直线射程。"""
import argparse
import csv
import math
import shlex
import shutil
import sys
from uuid import uuid4
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import render_birth_points
import render_recoil_tracks


def analyze(card):
    fields = {}
    sizes = {'/sample/input': 1, '/sample/output': 1, '/sample/center_m': 3, '/sample/size_m': 3}
    for line in card.read_text(encoding='utf-8-sig').splitlines():
        tokens = shlex.split(line, comments=True)
        if tokens:
            key, *values = tokens
            if key in fields or sizes.get(key) != len(values):
                raise ValueError(f'配置卡参数错误：{line}')
            fields[key] = values
    if fields.keys() != sizes.keys():
        raise ValueError(f'缺少配置项：{sizes.keys()-fields.keys()}')
    source = (card.parent / fields['/sample/input'][0]).resolve()
    output = (card.parent / fields['/sample/output'][0]).resolve()
    center = tuple(map(float, fields['/sample/center_m']))
    size = tuple(map(float, fields['/sample/size_m']))
    if not all(map(math.isfinite, center+size)) or min(size) <= 0:
        raise ValueError('取样中心必须有限，边长必须为正有限数')
    if output.exists():
        raise FileExistsError(f'请在卡片中指定新的输出目录：{output}')
    meta = dict(line.split('=',1) for line in (source/'metadata.txt').read_text().splitlines() if '=' in line)
    # 要求整个取样盒位于圆柱内，检查最外侧角点与顶底面。
    if math.hypot(abs(center[0])+size[0]/2, abs(center[1])+size[1]/2)>float(meta['radius_m']) or abs(center[2])+size[2]/2>float(meta['length_m'])/2:
        raise ValueError('取样盒超出本次模拟的圆柱边界')
    output.parent.mkdir(parents=True,exist_ok=True)
    # 与最终结果同父目录，继承 samples 的 ACL；避免继承 Windows 私有临时目录的 ACL。
    stage=output.parent / ('.'+output.name+'.'+uuid4().hex)
    stage.mkdir()
    try:
        shutil.copyfile(source/'metadata.txt',stage/'metadata.txt')
        shutil.copyfile(card,stage/'sample_used.mac')
        count=0
        with (source/'nuclei.csv').open(newline='') as stream, (stage/'nuclei.csv').open('w',newline='') as dest:
            reader=csv.DictReader(stream)
            coords=('x_m','y_m','z_m')
            if not set(coords).issubset(reader.fieldnames or []):raise ValueError('缺少出生位置列')
            has_direction={'dir_x','dir_y','dir_z'}.issubset(reader.fieldnames)
            writer=csv.DictWriter(dest,fieldnames=reader.fieldnames+['global_'+k for k in coords]);writer.writeheader()
            for row in reader:
                local=[float(row[k])-c for k,c in zip(coords,center)]
                if not all(map(math.isfinite,local)):raise ValueError('发现非有限出生位置')
                if all(abs(x)<=side/2 for x,side in zip(local,size)):
                    for k,x in zip(coords,local):row['global_'+k],row[k]=row[k],x
                    writer.writerow(row);count+=1
        view=dict(geometry='sample',size=size,length=size[2],width=size[0],scale=max(size)*.7,
                  dimensions=f'取样盒 {size} m，靶坐标中心 {center} m；图中使用局部坐标')
        render_birth_points.generate(stage,view=view)
        if has_direction:render_recoil_tracks.generate(stage,view=view)
        else:print('原数据缺少出生方向，仅生成出生点图。')
        stage.rename(output)
    finally:
        if stage.exists():
            shutil.rmtree(stage)
    print(f'{source.name}: 选中 {count} 条，输出 {output}')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('card',type=Path,nargs='?',default=Path(__file__).with_name('sample.mac'))
    analyze(parser.parse_args().card.resolve())
