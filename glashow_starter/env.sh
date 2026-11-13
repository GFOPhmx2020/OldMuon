#!/usr/bin/env bash
# 供 build.sh/run.sh source：集中配置依赖，换设备通常只修改 local_env.sh。
GLASHOW_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [[ -f "$GLASHOW_ROOT/local_env.sh" ]]; then
    source "$GLASHOW_ROOT/local_env.sh"
fi

# 优先使用明确指定的安装，再尝试 PATH 和本机常见的 /usr/local 安装。
if [[ -n "${GEANT4_SH:-}" ]]; then
    [[ -f "$GEANT4_SH" ]] || { echo "GEANT4_SH not found: $GEANT4_SH" >&2; return 1; }
elif [[ -n "${GEANT4_PREFIX:-}" ]]; then
    GEANT4_SH="$GEANT4_PREFIX/bin/geant4.sh"
    [[ -f "$GEANT4_SH" ]] || { echo "Missing $GEANT4_SH; configure local_env.sh" >&2; return 1; }
elif [[ -n "${Geant4_DIR:-}" ]]; then
    # 用户明确选了另一套 CMake 配置时，不擅自加载 /usr/local 的另一版本。
    :
elif command -v geant4.sh >/dev/null 2>&1; then
    GEANT4_SH="$(command -v geant4.sh)"
elif [[ -f /usr/local/bin/geant4.sh ]]; then
    GEANT4_SH=/usr/local/bin/geant4.sh
fi
if [[ -n "${GEANT4_SH:-}" ]]; then
    source "$GEANT4_SH"
    GEANT4_PREFIX="${GEANT4_PREFIX:-$(cd "$(dirname "$GEANT4_SH")/.." && pwd)}"
elif [[ -z "${Geant4_DIR:-}" ]]; then
    echo 'Set GEANT4_SH or Geant4_DIR in local_env.sh; see README.' >&2
    return 1
fi

# 若 Geant4_DIR 单独指定，调用者须事先加载 Geant4 的运行时数据环境。
if [[ -n "${GEANT4_PREFIX:-}" ]]; then
    export CMAKE_PREFIX_PATH="$GEANT4_PREFIX${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
fi
export PYTHIA8_ROOT="${PYTHIA8_ROOT:-$GLASHOW_ROOT/vendor/pythia8317}"
export PYTHIA8DATA="${PYTHIA8DATA:-$PYTHIA8_ROOT/share/Pythia8/xmldoc}"
export LD_LIBRARY_PATH="$PYTHIA8_ROOT/lib:$PYTHIA8_ROOT/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# BUILD_DIR 可设为新目录，避免复制来的 CMake 缓存含旧设备的绝对路径。
GLASHOW_BUILD_DIR="${GLASHOW_BUILD_DIR:-$GLASHOW_ROOT/build}"
[[ "$GLASHOW_BUILD_DIR" = /* ]] || GLASHOW_BUILD_DIR="$GLASHOW_ROOT/$GLASHOW_BUILD_DIR"
