#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""构建并运行全部测试 —— 每个测试是独立 qmake 工程，各自 qmake + make。

跨平台入口：用标准库实现构建与运行步骤，不依赖 POSIX sh、``make`` 与
shell 的单词拆分，Linux/macOS/Windows 共用同一份实现。

    python3 tests/run_all.py            # 全部
    python3 tests/run_all.py domain     # 只跑名字含 domain 的

环境变量：``QMAKE``（默认 qmake6）、``MAKE``（默认按平台选）、``JOBS``
（默认 4）、``QMAKE_ARGS``（额外 qmake 参数，按 shell 规则分词）。

退出码：0 全部通过；1 有构建/运行失败；2 没有测试匹配过滤条件。
"""

import os
import shlex
import shutil
import subprocess
import sys


# 界面相关用例需要离屏平台插件（CI 上没有显示器）。
CHILD_ENV_OVERRIDE = {"QT_QPA_PLATFORM": "offscreen"}


def default_make():
    """返回本平台默认的 make 工具名。

    Returns:
        str: 工具名；Windows 优先 mingw32-make，其次 make/jom/nmake。
    """
    if os.name == "nt":
        candidates = ("mingw32-make", "make", "jom", "nmake")
    else:
        candidates = ("make",)
    for name in candidates:
        if shutil.which(name) is not None:
            return name
    return candidates[0]


def executable_suffix():
    """返回本平台可执行文件后缀。

    Returns:
        str: Windows 为 ``.exe``，其它平台为空串。
    """
    return ".exe" if os.name == "nt" else ""


def run(command, cwd, env):
    """运行一个命令并返回退出码。

    Args:
        command (list[str]): 命令与参数。
        cwd (str): 工作目录。
        env (dict): 环境变量。

    Returns:
        int: 子进程退出码；命令无法启动时按 127 处理。
    """
    try:
        return subprocess.call(command, cwd=cwd, env=env)
    except OSError as exc:
        print("无法执行 {0}: {1}".format(command[0], exc))
        return 127


def test_projects(tests_dir):
    """返回 ``tests/`` 下的测试工程列表。

    Args:
        tests_dir (str): 测试目录。

    Returns:
        list[str]: 排序后的 ``.pro`` 文件绝对路径。
    """
    return sorted(
        os.path.join(tests_dir, name) for name in os.listdir(tests_dir)
        if name.endswith("_tests.pro"))


def build_and_run(root, pro, out_dir, qmake, make, jobs, qmake_args, env):
    """构建并运行一个测试工程。

    Args:
        root (str): 仓库根目录。
        pro (str): ``.pro`` 文件路径。
        out_dir (str): 测试产物根目录（``build/tests``）。
        qmake (str): qmake 命令。
        make (str): make 命令。
        jobs (str): 并行度。
        qmake_args (list[str]): 额外 qmake 参数。
        env (dict): 子进程环境变量。

    Returns:
        bool: 构建并运行都成功返回 True。
    """
    name = os.path.basename(pro)[:-len(".pro")]
    print("=== {0} ===".format(name))
    build_dir = os.path.join(out_dir, name)
    try:
        os.makedirs(build_dir)
    except OSError:
        if not os.path.isdir(build_dir):
            raise
    if run([qmake, pro] + qmake_args, build_dir, env) != 0:
        print("构建失败: {0}".format(name))
        return False
    if run([make, "-j{0}".format(jobs)], build_dir, env) != 0:
        print("构建失败: {0}".format(name))
        return False
    binary = os.path.join(build_dir, "bin", name + executable_suffix())
    if run([binary], root, env) != 0:
        print("测试失败: {0}".format(name))
        return False
    return True


def main(argv):
    """命令行入口。

    Args:
        argv (list[str]): 命令行参数（``argv[0]`` 是脚本名，可带一个过滤词）。

    Returns:
        int: 0 全部通过，1 有失败，2 没有测试匹配过滤条件。
    """
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    out_dir = os.path.join(root, "build", "tests")
    qmake = os.environ.get("QMAKE") or "qmake6"
    make = os.environ.get("MAKE") or default_make()
    jobs = os.environ.get("JOBS") or "4"
    qmake_args = shlex.split(os.environ.get("QMAKE_ARGS") or "")
    filter_text = argv[1] if len(argv) > 1 else ""
    env = dict(os.environ)
    env.update(CHILD_ENV_OVERRIDE)

    matched = 0
    status = 0
    for pro in test_projects(here):
        name = os.path.basename(pro)[:-len(".pro")]
        if filter_text not in name:
            continue
        matched += 1
        if not build_and_run(
                root, pro, out_dir, qmake, make, jobs, qmake_args, env):
            status = 1
    if matched == 0:
        sys.stderr.write("没有测试匹配过滤条件: {0}\n".format(
            filter_text or "<无>"))
        return 2

    if not filter_text:
        # 分层检查：与 check.sh layer 相同的两个检查。
        for tool in ("check_layering.py", "layering_selftest.py"):
            print("=== {0} ===".format(tool))
            code = run(
                [sys.executable, os.path.join(root, "tools", tool), root],
                root, env)
            if code != 0:
                print("失败: {0}".format(tool))
                status = 1
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv))
