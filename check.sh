#!/bin/sh
# 项目检查入口：build/test、格式检查、分层检查、审计。
# 用法：./check.sh [all|ci|style|layer|audit]
#
# ci/style 统一走 cxx_ci 的跨平台入口 check.py（主程序与 5 个测试工程由
# tests/run_all.sh 驱动，配置见 ci.toml）；layer 与 audit 保持项目自己的检查。
set -u

ROOT=$(cd "$(dirname "$0")" && pwd)
cd "$ROOT"
# 并行度自适应本机核数；显式给 JOBS=... 可覆盖
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}
STAGE=${1:-all}
STATUS=0

run() {
    echo "[check] $*"
    "$@"
    rc=$?
    if [ "$rc" -eq 130 ]; then
        exit 130
    fi
    if [ "$rc" -ne 0 ]; then
        # 配置/工具错误(2)比普通失败(1)更需要保留，不能被后续步骤覆盖。
        if [ "$rc" -eq 2 ] || [ "$STATUS" -eq 2 ]; then
            STATUS=2
        else
            STATUS=1
        fi
    fi
}

case "$STAGE" in
    all)
        run python3 ../cxx_ci/check.py all --root "$ROOT" --jobs "$JOBS"
        run python3 tools/check_layering.py
        run env PYTHONPATH=../cxx_audit/src python3 -m cpp_audit -c audit.toml --jobs "$JOBS" all
        ;;
    ci)
        run python3 ../cxx_ci/check.py ci --root "$ROOT" --jobs "$JOBS"
        ;;
    style)
        run python3 ../cxx_ci/check.py style --root "$ROOT" --jobs "$JOBS"
        ;;
    layer)
        run python3 tools/check_layering.py
        ;;
    audit)
        run env PYTHONPATH=../cxx_audit/src python3 -m cpp_audit -c audit.toml --jobs "$JOBS" all
        ;;
    *)
        echo "usage: $0 [all|ci|style|layer|audit]" >&2
        exit 2
        ;;
esac

exit "$STATUS"
