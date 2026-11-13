"""Integrate a fixed irradiation scenario over time, binned by birth depth and CSDA range."""
import argparse
import bisect
import csv
import html
import json
import math
import shlex
from pathlib import Path


def analyze(card):
    cfg = {}
    required = {'input', 'output', 'years', 'depth_m', 'depth_bins', 'range_bins', 'elements'}
    for line in card.read_text().splitlines():
        parts = shlex.split(line, comments=True)
        if parts:
            key = parts[0]
            if key not in required | {"range_max_um"} or key in cfg:
                raise ValueError(f'Unknown or duplicate setting: {key}')
            cfg[key] = parts[1:]
    if not required.issubset(cfg):
        raise ValueError(f'Missing settings: {required-set(cfg)}')
    def one(key):
        value, = cfg[key]
        return value
    source = card.parent / one('input')
    out = card.parent / one('output')
    years = float(one('years'))
    lo, hi = map(float, cfg['depth_m'])
    nd, nr = int(one('depth_bins')), int(one('range_bins'))
    if cfg['elements'] != ['all']:
        raise ValueError('Three-series plots require elements all')
    meta = dict(line.split('=', 1) for line in (source/'metadata.txt').read_text().splitlines() if '=' in line)
    if not (0 <= lo < hi <= float(meta['length_m']) and 0 < years < math.inf and 0 < nd*nr <= 100000 and min(nd,nr)>0):
        raise ValueError('Invalid exposure or bin geometry')
    table = Path(__file__).with_name('materials.tsv')
    densities = {p[0]: float(p[1]) for line in table.read_text().splitlines()
                 if (p := line.split()) and not p[0].startswith('#')}
    rho = densities[meta['material']]
    dz = (hi-lo)/nd
    # 1 g/cm³ = 10^9 mg/m³. Each layer spans the full target cross section.
    mass = rho*1e9*math.pi*float(meta['radius_m'])**2*dz
    factor = years/float(meta['equivalent_top_exposure_years'])/mass
    # 最后一列保存大于绘图上限的记录；恰在上限的记录归入最后一个可见档。
    xmax = float(cfg.get('range_max_um', ['25'])[0])
    if not 0 < xmax < math.inf:
        raise ValueError('range_max_um must be positive')
    counts = [{g:[0]*(nr+1) for g in ('all','Na','Cl')} for _ in range(nd)]
    skipped = 0
    with (source/'nuclei.csv').open() as stream:
        for row in csv.DictReader(stream):
            d = float(row['depth_m'])
            if not lo <= d <= hi:
                continue
            r = float(row['range_nm']) / 1000
            if row['range_status'] != 'ok' or not 0 < r < math.inf:
                skipped += 1
                continue
            layer = counts[min(nd-1, int((d-lo)/dz))]
            j = nr if r>xmax else min(nr-1,int(r/xmax*nr))
            layer['all'][j] += 1
            group = {'11':'Na','17':'Cl'}.get(row['Z'])
            if group:
                layer[group][j] += 1
    out.mkdir(parents=True,exist_ok=True)
    with (out/'depth_tracks.csv').open('w',newline='') as stream:
        writer = csv.writer(stream)
        writer.writerow(['depth_start_m','depth_end_m','group','bin_type','range_start_um','range_end_um','raw_count','layer_mass_mg','years','tracks_per_mg'])
        for i,layer in enumerate(counts):
            for group,bins in layer.items():
                for j,n in enumerate(bins):
                    writer.writerow([lo+i*dz,lo+(i+1)*dz,group,'overflow' if j==nr else 'visible',j*xmax/nr,(j+1)*xmax/nr if j<nr else '',n,mass,years,n*factor])
    panels=[]
    peak=max(max(layer['all'][:-1]) for layer in counts)
    bottom=math.floor(math.log10(factor))
    top=max(bottom+1,math.ceil(math.log10(max(1,peak)*factor)))
    styles=[('all','所有核','#222',''),('Na','Na 核','#1676af','7 3'),('Cl','Cl 核','#b24d13','8 3 2 3')]
    for i,layer in enumerate(counts):
        plot=[]
        for exponent in range(bottom,top+1):
            y=202-170*(exponent-bottom)/(top-bottom)
            plot.append(f'<path d="M72 {y}h330" stroke="#d9dfe4"/><text x="65" y="{y+4}" text-anchor="end">10<tspan dy="-4" font-size="8">{exponent}</tspan></text>')
        for tick in range(6):
            x=72+66*tick
            plot.append(f'<text x="{x}" y="220" text-anchor="middle">{tick*xmax/5:g}</text>')
        for group,label,color,dash in styles:
            segments=[]
            for j,n in enumerate(layer[group][:-1]):
                if n==0: continue
                x=72+330*j/nr
                y=202-170*(math.log10(n*factor)-bottom)/(top-bottom)
                join='V' if j and layer[group][j-1] else None
                segments.append((f'V{y}' if join else f'M{x} {y}')+f'H{72+330*(j+1)/nr}')
            plot.append(f'<path d="{" ".join(segments)}" fill="none" stroke="{color}" stroke-width="2" stroke-dasharray="{dash}"/>')
        for j in range(nr):
            tip='；'.join(f'{label} {layer[g][j]*factor:.4g} mg⁻¹（{layer[g][j]} 条）' for g,label,_,_ in styles)
            plot.append(f'<rect x="{72+330*j/nr}" y="32" width="{330/nr}" height="170" fill="transparent"><title>{j*xmax/nr:g}–{(j+1)*xmax/nr:g} µm：{tip}</title></rect>')
        panels.append(f'<section><h2>{lo+i*dz:g}–{lo+(i+1)*dz:g} m</h2><svg viewBox="0 0 430 250"><path d="M72 32V202H402" stroke="#555" fill="none"/>'+''.join(plot)+'<text x="235" y="242" text-anchor="middle">CSDA 射程（µm）</text><text transform="translate(15 120) rotate(-90)" text-anchor="middle">累计径迹数（mg⁻¹ / 档）</text></svg>'+f'<footer>全射程原始数 {sum(layer["all"])}；超过 {xmax:g} µm：所有核 {layer["all"][-1]}，Na {layer["Na"][-1]}，Cl {layer["Cl"][-1]}。</footer></section>')
    legend=' '.join(f'<span style="color:{c}">{mark} {label}</span>' for (_,label,c,_),mark in zip(styles,['━━','┄┄','┅·']))
    page=f'''<!doctype html><html lang="zh"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>分层累计径迹长度谱</title><style>body{{max-width:1150px;margin:30px auto;padding:0 18px;font:16px system-ui;color:#243441;background:#f5f6f7}}main{{display:grid;grid-template-columns:repeat(2,1fr);gap:16px}}section{{background:white;padding:15px;border:1px solid #dde2e5}}svg{{width:100%;font-size:12px}}h2{{margin:0}}footer{{font-size:13px}}span{{margin-right:18px}}@media(max-width:650px){{main{{grid-template-columns:1fr}}}}</style><h1>{lo:g}–{hi:g} m 的累计径迹长度谱</h1><p>{html.escape(source.name)} · {html.escape(meta['material'])} · {years:,.0f} 年 · 每档 {xmax/nr:g} µm</p><p>{legend}</p><main>{''.join(panels)}</main><p>所有核包含 Na 和 Cl。各图共用对数纵轴，零计数档留空。悬停可读原始数。有效射程以外的记录 {skipped} 条。</p><p>各层质量 {mass:.6g} mg，模拟曝光 {meta['equivalent_top_exposure_s']} s。按固定通量与几何外推；每条有效产生核记录贡献一条 CSDA 示意径迹。</p></html>'''
    (out/'depth_tracks.html').write_text(page,encoding='utf-8')
    print([(sum(layer['all']), layer['all'][-1]) for layer in counts])
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('card',nargs='?',type=Path,default=Path(__file__).with_name('depth_tracks.mac'))
    analyze(parser.parse_args().card.resolve())
