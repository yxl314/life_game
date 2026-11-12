#!/bin/sh
# 项目检查入口：build/test、格式检查、审计。
# 用法：./check.sh [all|ci|style|audit]
set -u

ROOT=$(cd "$(dirname "$0")" && pwd)
cd "$ROOT"
JOBS=${JOBS:-8}
STAGE=${1:-all}
STATUS=0

run() {
    echo "[check] $*"
    "$@" || STATUS=1
}

case "$STAGE" in
    all)
        run python3 ../cxx_ci/ci/ci.py -c ci.toml --jobs "$JOBS"
        run python3 ../cxx_ci/standardize/standardize.py --check --jobs "$JOBS"
        run env PYTHONPATH=../cxx_audit/src python3 -m cpp_audit -c audit.toml --jobs "$JOBS" all
        ;;
    ci)
        run python3 ../cxx_ci/ci/ci.py -c ci.toml --jobs "$JOBS"
        ;;
    style)
        run python3 ../cxx_ci/standardize/standardize.py --check --jobs "$JOBS"
        ;;
    audit)
        run env PYTHONPATH=../cxx_audit/src python3 -m cpp_audit -c audit.toml --jobs "$JOBS" all
        ;;
    *)
        echo "usage: $0 [all|ci|style|audit]" >&2
        exit 2
        ;;
esac

exit "$STATUS"
