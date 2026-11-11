#!/bin/sh
# 构建并运行全部测试 —— 每个测试是独立工程，各自 qmake + make，互不依赖。
#
#     ./tests/run_all.sh            # 全部
#     ./tests/run_all.sh domain     # 只跑名字含 domain 的
#
# 为什么不用 TEMPLATE=subdirs 汇总：qmake 的 subdirs 会按子目录名拼 .pro 路径，
# 而本仓库的路径里有非 ASCII 目录（桌面），生成的 Makefile 会把路径写坏成 ??。
# 逐个构建没有这个问题，也正好对上"每个测试都是独立项目"的要求。
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
QMAKE=${QMAKE:-qmake6}
# 额外 qmake 参数，例如 ASan CI：
#   QMAKE_ARGS='QMAKE_CXXFLAGS+=-fsanitize=address QMAKE_LFLAGS+=-fsanitize=address'
QMAKE_ARGS=${QMAKE_ARGS:-}
JOBS=${JOBS:-6}
FILTER=${1:-}
OUT="$ROOT/build/tests"
STATUS=0

# 界面相关用例需要离屏平台插件：CI 上没有显示器
export QT_QPA_PLATFORM=offscreen

for pro in "$ROOT"/tests/*_tests.pro; do
    name=$(basename "$pro" .pro)
    case "$name" in
        *"$FILTER"*) ;;
        *) continue ;;
    esac
    dir="$OUT/$name"
    printf '=== %s ===\n' "$name"
    mkdir -p "$dir" || exit 1
    # QMAKE_ARGS is intentionally word-split so callers can pass several flags.
    if ! (cd "$dir" && "$QMAKE" "$pro" $QMAKE_ARGS && make -j"$JOBS"); then
        printf '构建失败: %s\n' "$name"
        STATUS=1
        continue
    fi
    if ! "$dir/bin/$name"; then
        printf '测试失败: %s\n' "$name"
        STATUS=1
    fi
done

# 分层检查：CMake 里那两个 add_test，这里同样跑
if [ -z "$FILTER" ]; then
    for tool in check_layering.py layering_selftest.py; do
        printf '=== %s ===\n' "$tool"
        if ! python3 "$ROOT/tools/$tool" "$ROOT"; then
            printf '失败: %s\n' "$tool"
            STATUS=1
        fi
    done
fi

exit $STATUS
