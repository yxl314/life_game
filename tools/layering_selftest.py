#!/usr/bin/env python3
"""Negative fixtures for tools/check_layering.py.

The architecture gate must fail on bypasses, not only pass on the happy path.
"""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path


def run_checker(root: Path) -> subprocess.CompletedProcess[str]:
    checker = root / "tools" / "check_layering.py"
    return subprocess.run(
        [sys.executable, str(checker), str(root)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def write_tree(files: dict[str, str]) -> Path:
    root = Path(tempfile.mkdtemp(prefix="layering-selftest-"))
    (root / "tools").mkdir(parents=True, exist_ok=True)
    (root / "tools" / "check_layering.py").write_text(
        (Path(__file__).resolve().parent / "check_layering.py").read_text(encoding="utf-8"),
        encoding="utf-8",
    )
    source_paths = []
    for rel, content in files.items():
        path = root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content, encoding="utf-8")
        if rel.startswith("src/"):
            source_paths.append(rel)
    # 清单的唯一事实源是 life_game.pri（构建已从 CMake 换成 qmake）
    (root / "life_game.pri").write_text(
        "LIFEGAME_SELFTEST_SOURCES = \\\n"
        + " \\\n".join("    " + rel for rel in source_paths)
        + "\n",
        encoding="utf-8",
    )
    return root


def expect(name: str, files: dict[str, str], should_pass: bool) -> bool:
    root = write_tree(files)
    result = run_checker(root)
    ok = (result.returncode == 0) == should_pass
    print(f"{'PASS' if ok else 'FAIL'} {name}: exit={result.returncode}")
    if not ok:
        print(result.stdout)
    return ok


def main() -> int:
    ok = True
    ok &= expect(
        "valid domain",
        {"src/domain/grid.h": "#pragma once\nnamespace lifegame::domain {}\n"},
        True,
    )
    ok &= expect(
        "view traversal",
        {
            "src/view/view.h": '#include "model/../controller/x.h"\n',
            "src/controller/x.h": "#pragma once\n",
        },
        False,
    )
    ok &= expect(
        "view src-prefix",
        {
            "src/view/view.h": '#include "src/controller/x.h"\n',
            "src/controller/x.h": "#pragma once\n",
        },
        False,
    )
    ok &= expect(
        "view model dependency",
        {
            "src/view/view.h": '#include "model/life_stats.h"\n',
            "src/model/life_stats.h": "#pragma once\n",
        },
        False,
    )
    ok &= expect(
        "service model dependency",
        {
            "src/service/service.h": '#include "model/life_stats.h"\n',
            "src/model/life_stats.h": "#pragma once\n",
        },
        False,
    )
    ok &= expect(
        "shared Qt dependency",
        {"src/shared/value.h": "#include <QString>\n"},
        False,
    )
    ok &= expect(
        "model QFile",
        {"src/model/model.h": "#include <QFile>\n"},
        False,
    )
    ok &= expect(
        "model Chinese literal",
        {"src/model/model.h": 'inline const char* kMsg = "中文说明";\n'},
        False,
    )
    ok &= expect(
        "model Chinese comment",
        {"src/model/model.h": "// 中文注释是文档，不是显示文本\n"},
        True,
    )
    ok &= expect(
        "all sources listed",
        {"src/domain/a.h": "#pragma once\n", "src/domain/b.h": "#pragma once\n"},
        True,
    )
    # The last fixture deliberately lists both files; build a truly unlisted one.
    root = write_tree({"src/domain/listed.h": "#pragma once\n"})
    (root / "src" / "domain" / "unlisted.h").write_text("#pragma once\n", encoding="utf-8")
    result = run_checker(root)
    unlisted_ok = result.returncode == 1
    print(f"{'PASS' if unlisted_ok else 'FAIL'} truly unlisted source: exit={result.returncode}")
    ok &= unlisted_ok

    root = write_tree({
        "src/domain/listed.h": "#pragma once\n",
        "src/model/wrong_group.h": "#pragma once\n",
    })
    (root / "life_game.pri").write_text(
        "LIFEGAME_DOMAIN_HEADERS = \\\n"
        "    $$LIFEGAME_ROOT/src/model/wrong_group.h \\\n"
        "    $$LIFEGAME_ROOT/src/domain/listed.h\n",
        encoding="utf-8",
    )
    result = run_checker(root)
    misclassified_ok = result.returncode == 1
    print(f"{'PASS' if misclassified_ok else 'FAIL'} manifest group mismatch: exit={result.returncode}")
    ok &= misclassified_ok

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
