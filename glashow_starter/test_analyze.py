"""分析计算的单元测试：验证单位、分母、越界；不验证物理模型准确性。"""
import unittest
from pathlib import Path
import tempfile
import numpy as np
from analyze import range_proxy_nm, spectrum_per_event


class AnalysisChecks(unittest.TestCase):
    def test_zero_recoil_events_stay_in_denominator(self):
        edges = np.array([0., 10., 20.])
        # 两个事件都完成，只有一个产生这三条记录，分母仍必须是 2。
        y, outside = spectrum_per_event(np.array([2., 3., 12.]), edges, 2)
        np.testing.assert_allclose(y, [1., 0.5])
        self.assertEqual(outside, 0)

    def test_range_units_and_no_silent_extrapolation(self):
        # 人工构造射程表，检查微米到纳米的因子 1000。
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / 'range.txt'
            np.savetxt(p, [[.001, 1., 2., .01], [.01, 1., 2., .1]])
            np.testing.assert_allclose(range_proxy_nm([.001, .01], p), [10., 100.])
            with self.assertRaises(ValueError):
                range_proxy_nm([.1], p)

    def test_overflow_reported_not_renormalized(self):
        # 第二条记录在直方图之外，但不能因此把第一条的权重抬高。
        y, outside = spectrum_per_event([2., 200.], [0., 10.], 2)
        np.testing.assert_allclose(y, [.5])
        self.assertEqual(outside, 1)

    def test_invalid_denominator_rejected(self):
        with self.assertRaises(ValueError):
            spectrum_per_event([2.], [0., 10.], 0)


if __name__ == '__main__':
    unittest.main()
