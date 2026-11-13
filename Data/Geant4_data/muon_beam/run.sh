#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
[[ -x "$script_dir/build-wsl/muon_beam" ]] || { echo '请先 bash build.sh' >&2; exit 1; }
python3 -c 'import yaml' || { echo '请在当前 Python 环境安装：python3 -m pip install PyYAML' >&2; exit 1; }
exec python3 "$script_dir/run_beam.py" "$@"
