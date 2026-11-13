"""验证旧 TSV、新 gzip TSV 与中断压缩文件的读取，不运行 Geant4。"""
import gzip
import tempfile
import unittest
import zlib
from pathlib import Path
from data_io import open_data, partial_lines


class DataIOChecks(unittest.TestCase):
    def test_plain_and_gzip_preserve_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = b'energy_mev\n0.00010000000000000001\n'
            (root / 'a.tsv').write_bytes(payload)
            with open_data(root, 'a.tsv', 'rb') as f:
                self.assertEqual(f.read(), payload)
            (root / 'a.tsv').unlink()
            (root / 'a.tsv.gz').write_bytes(gzip.compress(payload))
            with open_data(root, 'a.tsv', 'rb') as f:
                self.assertEqual(f.read(), payload)

    def test_ambiguous_files_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / 'a.tsv').write_text('x')
            (root / 'a.tsv.gz').write_bytes(gzip.compress(b'x'))
            with self.assertRaises(ValueError):
                open_data(root, 'a.tsv')

    def test_interrupted_gzip_reports_missing_footer(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = b'header\ncomplete row\nincomplete'
            (root / 'a.tsv.gz').write_bytes(gzip.compress(payload)[:-8])
            issues = []
            self.assertEqual(list(partial_lines(root, 'a.tsv', issues)),
                             ['header\n', 'complete row\n', 'incomplete'])
            self.assertTrue(issues)

    def test_corrupt_gzip_not_silently_accepted(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            payload = bytearray(gzip.compress(b'header\nrow\n'))
            payload[-8] ^= 0xff  # 修改 CRC；这不是正常的手工中断。
            (root / 'a.tsv.gz').write_bytes(payload)
            with self.assertRaises(zlib.error):
                list(partial_lines(root, 'a.tsv', []))


if __name__ == '__main__':
    unittest.main()
