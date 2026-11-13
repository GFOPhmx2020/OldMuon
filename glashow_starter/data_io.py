"""统一读取 .tsv / .tsv.gz；只在中断数据审计时允许缺少 gzip 尾部。"""
import gzip
from pathlib import Path
import zlib


def data_path(directory, name):
    """同时存在两种格式容易读到旧数据，因此拒绝猜测应读哪份。"""
    plain = Path(directory) / name
    zipped = Path(str(plain) + '.gz')
    found = [p for p in (plain, zipped) if p.is_file()]
    if len(found) != 1:
        raise ValueError(f'Expected exactly one of {plain} and {zipped}; found {len(found)}')
    return found[0]


def open_data(directory, name, mode='rt'):
    """完整文件使用标准 gzip 读取器，保留 CRC/完整性检查。"""
    path = data_path(directory, name)
    kwargs = {} if 'b' in mode else {'encoding': 'utf-8', 'newline': ''}
    return gzip.open(path, mode, **kwargs) if path.suffix == '.gz' else path.open(mode, **kwargs)


def partial_lines(directory, name, issues):
    """逐行挽救已写出的前缀；缺少尾部会记录告警，损坏的 CRC 仍报错。

    新写入器每次运行生成单个 gzip member；不把未正常关闭的文件当成完整样本。
    使用固定块读取，内存不随总行数增长。最后无换行的文本由审计器判为截断行。
    """
    path = data_path(directory, name)
    if path.suffix != '.gz':
        with path.open(encoding='utf-8') as f:
            yield from f
        return
    decoder = zlib.decompressobj(16 + zlib.MAX_WBITS)
    pending = b''
    with path.open('rb') as f:
        for block in iter(lambda: f.read(65536), b''):
            pending += decoder.decompress(block)
            lines = pending.split(b'\n')
            pending = lines.pop()
            for line in lines:
                yield (line + b'\n').decode('utf-8')
            if decoder.unused_data:
                raise ValueError('Unexpected bytes or multiple gzip members')
    if not decoder.eof:
        issues.append('gzip stream interrupted: missing footer; only recoverable prefix counted')
    if pending:
        yield pending.decode('utf-8')
