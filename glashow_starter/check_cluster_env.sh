#!/usr/bin/env bash
# 在目标节点运行：只报告环境，不安装软件、不更改配置、不遍历大型数据盘。
# bash check_cluster_env.sh [额外软件安装目录] | tee cluster_env_report.txt
set -o pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
section() { printf '\n===== %s =====\n' "$1"; }
section 'Host / compiler / build tools'
hostname
uname -sm
for cmd in c++ g++ cmake make python3 geant4-config pythia8-config; do
    if command -v "$cmd" >/dev/null 2>&1; then
        command -v "$cmd"
        "$cmd" --version 2>&1 | head -n 2
    else
        printf 'NOT ON PATH: %s\n' "$cmd"
    fi
done
printf 'CXX=%s\n' "${CXX:-not set}"
compiler="${CXX:-c++}"
# 不生成目标文件：验证 C++17 语法及 zlib 头文件是否可用。
if command -v "$compiler" >/dev/null 2>&1; then
    printf '#include <filesystem>\n#include <optional>\nint main(){std::optional<int> x=1; auto p=std::filesystem::path(".");}\n' |
        "$compiler" -std=c++17 -x c++ -fsyntax-only - && echo 'C++17 headers/syntax: OK'
    printf '#include <zlib.h>\n' | "$compiler" -x c++ -fsyntax-only - && echo 'zlib header: OK'
    printf 'zlib linker candidate: '
    "$compiler" -print-file-name=libz.so
fi
section 'Loaded Geant4 and dataset paths'
printf 'Geant4_DIR=%s\nGEANT4_SH=%s\nCMAKE_PREFIX_PATH=%s\n' "${Geant4_DIR:-}" "${GEANT4_SH:-}" "${CMAKE_PREFIX_PATH:-}"
if command -v geant4-config >/dev/null 2>&1; then
    geant4-config --prefix
    geant4-config --datasets
fi
found=0
while IFS= read -r name; do
    case "$name" in
        G4*DATA|G4NEUTRONHPDATA|GEANT4_DATA_DIR)
            value="${!name}"
            found=1
            if [[ -d "$value" ]]; then
                printf 'EXISTS %s=%s\n' "$name" "$value"
            else
                printf 'MISSING %s=%s\n' "$name" "$value"
            fi ;;
    esac
done < <(compgen -e)
[[ "$found" == 1 ]] || echo 'No exported dataset paths found; check geant4-config --datasets and source geant4.sh.'
section 'Software candidates (bounded search; not proof of completeness)'
# 仅搜索常用软件位置；额外位置可作为参数传入。避免扫描 /data 或整个 /cvmfs。
for base in /opt /usr/local "$HOME/software" "$HOME/local" "$root/vendor" "$@"; do
    [[ -d "$base" ]] || continue
    printf '\nSearch: %s\n' "$base"
    if command -v timeout >/dev/null 2>&1; then
        timeout 30 find "$base" -maxdepth 7 \( -name geant4.sh -o -name Geant4Config.cmake \
            -o -name Pythia.h -o -name libpythia8.so -o -name libpythia8.a \
            -o -name Index.xml -o -name libz.so -o -name zlib.h \) -print 2>/dev/null
        status=$?
        [[ "$status" == 0 ]] || printf 'Search incomplete (status %s: timeout or access restriction).\n' "$status"
    else
        echo 'timeout unavailable; skipped bounded search. Use a specific installation path.'
    fi
done
section 'Configured / bundled PYTHIA candidates'
for base in "${PYTHIA8_ROOT:-}" "$root/vendor/pythia8317"; do
    [[ -n "$base" ]] || continue
    printf '\nPYTHIA root: %s\n' "$base"
    for rel in include/Pythia8/Pythia.h share/Pythia8/xmldoc/Index.xml; do
        [[ -f "$base/$rel" ]] && echo "OK $rel" || echo "MISSING $rel"
    done
    for lib in "$base"/lib/libpythia8.* "$base"/lib64/libpythia8.*; do
        [[ -f "$lib" ]] || continue
        file "$lib"
        # 仅诊断用户项目/指定安装中的可信库；动态依赖 not found 表示仍不能运行。
        [[ "$lib" == *.so ]] && ldd "$lib"
    done
done
section 'Python analysis packages'
if command -v python3 >/dev/null 2>&1; then
    python3 - <<'PY'
import sys, importlib
print('Python executable:', sys.executable)
for name in ('numpy', 'matplotlib'):
    try:
        module = importlib.import_module(name)
        print(name, module.__version__, module.__file__)
    except Exception as exc:
        print('UNAVAILABLE:', name, str(exc))
PY
fi
section 'Na/Cl range tables for analysis'
for name in DEDX_Z11_A23.txt DEDX_Z17_A35.txt DEDX_Z17_A37.txt; do
    path="$root/../Data/Geant4_data/Halite/Range/$name"
    [[ -r "$path" ]] && echo "OK $path" || echo "MISSING $path"
done
section 'Scheduler commands / disk space'
for cmd in sbatch srun squeue qsub qstat condor_submit bsub; do
    command -v "$cmd" 2>/dev/null || true
done
type module 2>/dev/null || true
df -h "$root" "$HOME"
section 'Conclusion'
echo 'Discovery only: directory presence does not verify dataset completeness or binary compatibility.'
echo 'Use a fresh build directory; CMake configure/link and a small transport run are the final checks.'
