#!/usr/bin/env bash
# 从任何工作目录启动均可；只构建和测试，不运行十万事件模拟。
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="$script_dir/build-wsl"
for tool in cmake g++; do
  command -v "$tool" >/dev/null || { echo "缺少 $tool，请先安装或加载环境" >&2; exit 1; }
done
jobs="${BUILD_JOBS:-8}"
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo 'BUILD_JOBS 必须为正整数' >&2; exit 1; }
cmake -S "$script_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$build_dir" --parallel "$jobs"
python3 "$script_dir/test_beam.py"
echo "构建和几何统计测试完成。运行：bash '$script_dir/run.sh'"
