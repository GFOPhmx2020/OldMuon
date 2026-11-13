#!/usr/bin/env bash
set -eo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"
source ./env.sh
# Release 加速本项目的代码；不会重新优化已安装的 Geant4/PYTHIA 软件库。
args=(-S "$GLASHOW_ROOT" -B "$GLASHOW_BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
      "-DPYTHIA8_ROOT=$PYTHIA8_ROOT")
[[ -z "${Geant4_DIR:-}" ]] || args+=("-DGeant4_DIR=$Geant4_DIR")
cmake "${args[@]}"
cmake --build "$GLASHOW_BUILD_DIR" --config Release -j "${JOBS:-8}"
