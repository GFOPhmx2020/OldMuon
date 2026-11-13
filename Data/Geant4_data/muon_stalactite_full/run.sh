#!/usr/bin/env bash
# 相对卡片路径按调用者目录解释；默认使用脚本旁的 config.mac。
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exe="$script_dir/build-wsl/muon_stalactite_full"
[[ -x "$exe" ]] || { echo "请先运行 bash '$script_dir/build.sh'" >&2; exit 1; }
if (( $# > 1 )); then echo '用法：bash run.sh [配置卡路径]' >&2; exit 1; fi
command -v python3 >/dev/null || { echo '交互 HTML 生成需要 python3' >&2; exit 1; }
card="${1:-$script_dir/config.mac}"
"$exe" "$card"
# 仅在输运成功完成后读取核记录，生成离线可旋转的圆柱密度图。
python3 "$script_dir/cylinder_density.py" --card "$card"
