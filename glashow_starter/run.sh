#!/usr/bin/env bash
set -eo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
source ./env.sh
# 检查数据路径，避免把“路径配置错误”误认为生成器物理问题。
[[ -d "$PYTHIA8DATA" ]] || { echo "Missing PYTHIA8DATA: $PYTHIA8DATA" >&2; exit 1; }
if [[ ! -x "$GLASHOW_BUILD_DIR/GlashowRun" ]]; then
    echo 'Executable missing. Run: bash build.sh' >&2
    exit 1
fi
# exec 保留真实退出码；用户 Ctrl+C 会直接传给模拟进程。
exec "$GLASHOW_BUILD_DIR/GlashowRun" "$@"
