#!/usr/bin/env python3
"""Enforce the layer contract described in docs/architecture.md.

Project includes must use an explicit layer prefix, e.g.

    #include "domain/grid.h"
    #include "model/life_model.h"

The checker rejects:
  * includes that point at a forbidden layer,
  * path traversal, absolute paths and the non-canonical ``src/`` prefix,
  * Qt/UI headers in layers that must stay UI-free,
  * Chinese display text in domain/model,
  * sources that are not listed in life_game.pri,
  * manifest groups that list a file under the wrong layer.

Run directly or from tests/run_all.sh.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path, PurePosixPath

LAYERS = ("domain", "shared", "model", "service", "view", "controller", "app")

ALLOWED = {
    "domain": {"domain"},
    "shared": {"shared"},
    "model": {"domain", "shared", "model"},
    "service": {"domain", "shared", "service"},
    "view": {"domain", "shared", "view"},
    "controller": {"domain", "shared", "model", "service", "view", "controller"},
    "app": set(LAYERS),
}

# UI/framework headers that must not leak into pure/model/service layers.
QT_FORBIDDEN = {
    "model": {
        "QFile", "QSettings", "QFileDialog", "QMessageBox", "QWidget", "QDialog",
        "QMainWindow", "QProgressDialog", "QApplication", "QStatusBar", "QMenu",
        "QMenuBar", "QToolBar", "QImage",
    },
    "service": {
        "QWidget", "QDialog", "QMessageBox", "QFileDialog", "QMainWindow",
        "QApplication", "QProgressDialog", "QMenu", "QToolBar",
    },
}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]')
QT_INCLUDE_RE = re.compile(r'^\s*#\s*include\s*[<"]Q[A-Za-z0-9_/]*[>"]')
CJK_RE = re.compile(r'[\u4e00-\u9fff]')
MANIFEST_GROUP_RE = re.compile(
    r'^(LIFEGAME_(?:DOMAIN|SHARED|MODEL|SERVICE|VIEW|CONTROLLER|APP|MAIN))'
    r'_(?:SOURCES|HEADERS)\s*=\s*(.*)$')
MANIFEST_GROUP_LAYER = {
    "LIFEGAME_DOMAIN": "domain",
    "LIFEGAME_SHARED": "shared",
    "LIFEGAME_MODEL": "model",
    "LIFEGAME_SERVICE": "service",
    "LIFEGAME_VIEW": "view",
    "LIFEGAME_CONTROLLER": "controller",
    "LIFEGAME_APP": "app",
    "LIFEGAME_MAIN": "app",
}


def layer_of(path: Path, root: Path) -> str | None:
    try:
        rel = path.relative_to(root)
    except ValueError:
        return None
    if len(rel.parts) < 2:
        return None
    layer = rel.parts[0]
    return layer if layer in LAYERS else None


def normalize_include(target: str) -> str | None:
    """Return the canonical layer-prefixed include, or None for non-project headers."""
    target = target.replace("\\", "/")
    if target.startswith("/"):
        return ""
    if target.startswith("src/"):
        return ""
    parts = PurePosixPath(target).parts
    if ".." in parts:
        return ""
    if not parts or parts[0] not in LAYERS:
        return None
    return target


def logical_lines(text: str):
    """Yield manifest lines with backslash continuations joined."""
    parts: list[str] = []
    for raw in text.splitlines():
        line = raw.split('#', 1)[0].rstrip()
        if not line.strip():
            continue
        if line.endswith('\\'):
            parts.append(line[:-1].strip())
        else:
            parts.append(line.strip())
            yield ' '.join(parts)
            parts = []
    if parts:
        yield ' '.join(parts)


def check_manifest_groups(root: Path) -> list[str]:
    """Ensure each LIFEGAME_* group only lists files from its own layer."""
    manifest = root / "life_game.pri"
    if not manifest.is_file():
        return []
    text = manifest.read_text(encoding="utf-8", errors="replace")
    problems: list[str] = []
    listed: dict[str, list[str]] = {}

    for line in logical_lines(text):
        match = MANIFEST_GROUP_RE.match(line)
        if not match:
            continue
        group, values = match.group(1), match.group(2)
        expected = MANIFEST_GROUP_LAYER[group]
        for token in values.split():
            if not token.startswith("$$LIFEGAME_ROOT/"):
                continue
            rel = token[len("$$LIFEGAME_ROOT/"):]
            if not rel.startswith("src/"):
                continue
            parts = rel.split("/")
            if len(parts) < 3:
                continue
            actual = parts[1]
            if actual != expected:
                problems.append(
                    f"{manifest}: {group}_* lists {rel}, expected layer {expected}")
            listed.setdefault(rel, []).append(group)
            if not (root / rel).is_file():
                problems.append(f"{manifest}: listed file does not exist: {rel}")

    for rel, groups in listed.items():
        unique = sorted(set(groups))
        if len(unique) > 1:
            problems.append(
                f"{manifest}: {rel} is listed in multiple layer groups: {', '.join(unique)}")
    return problems


def check_manifest(root: Path, sources: list[Path]) -> list[str]:
    """源码清单是否完整。

    构建从 CMake 换成了 qmake 之后，清单的唯一事实源是 life_game.pri 里的
    LIFEGAME_*_SOURCES / *_HEADERS。它同时被主程序和每个测试 include，所以
    "某个 .cpp 忘了登记"在任何一个工程里都编不进去——但仍要在这里点出来：
    那种情况下编译能过（只是少了那个文件的符号），问题要留到链接或者运行时才露。
    """
    problems: list[str] = []
    manifest = root / "life_game.pri"
    if not manifest.is_file():
        return ["life_game.pri is missing"]
    text = manifest.read_text(encoding="utf-8", errors="replace")

    for source in sources:
        rel = source.relative_to(root).as_posix()
        if rel not in text:
            problems.append(f"{source}: source file is not listed in life_game.pri")

    return problems

def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    src = root / "src"
    if not src.is_dir():
        print(f"layering: source directory not found: {src}")
        return 2

    problems: list[str] = []
    checked = 0
    sources: list[Path] = []

    for path in sorted(src.rglob("*")):
        if path.suffix not in {".h", ".hpp", ".cpp", ".cc", ".cxx"}:
            continue
        layer = layer_of(path, src)
        if layer is None:
            problems.append(f"{path}: file is not inside a known layer directory")
            continue
        checked += 1
        sources.append(path)
        text = path.read_text(encoding="utf-8", errors="replace")

        if layer in {"domain", "shared", "model"} and CJK_RE.search(text):
            problems.append(f"{path}: Chinese display text is forbidden in {layer}/")

        for lineno, line in enumerate(text.splitlines(), 1):
            if layer in {"domain", "shared"} and QT_INCLUDE_RE.match(line):
                problems.append(f"{path}:{lineno}: {layer} must not include Qt headers")
                continue
            match = INCLUDE_RE.match(line)
            if not match:
                continue
            raw_target = match.group(2)
            canonical = normalize_include(raw_target)
            if canonical == "":
                problems.append(
                    f"{path}:{lineno}: non-canonical or escaping include is forbidden: {raw_target}"
                )
                continue
            if canonical is None:
                # Qt/system header; check the UI-free lists below.
                header = raw_target.split("/")[0]
                header = header.split(".")[0]
                if layer in QT_FORBIDDEN and header in QT_FORBIDDEN[layer]:
                    problems.append(f"{path}:{lineno}: {layer} may not include {raw_target}")
                continue
            target_layer = canonical.split("/", 1)[0]
            if target_layer not in ALLOWED[layer]:
                problems.append(
                    f"{path}:{lineno}: {layer} may not include {target_layer} ({canonical})"
                )

    problems.extend(check_manifest(root, sources))
    problems.extend(check_manifest_groups(root))

    if problems:
        print("Architecture violations:")
        for problem in problems:
            print(f"  - {problem}")
        print(f"\n{len(problems)} violation(s) in {checked} files.")
        return 1

    print(f"Layering OK: {checked} files checked.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
