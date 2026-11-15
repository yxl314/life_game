#!/bin/sh
# 兼容入口：真正的跨平台实现在 run_all.py（Windows 上没有 sh 也能用
# `python3 tests/run_all.py`）。参数与环境变量语义见 run_all.py。
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
exec python3 "$DIR/run_all.py" "$@"
