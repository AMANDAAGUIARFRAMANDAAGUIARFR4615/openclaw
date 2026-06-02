#!/usr/bin/env python3
"""Release：混淆 Qt MOC 元对象字符串（类名、信号、槽、参数等），并可选修补二进制残留。"""
from __future__ import annotations

import hashlib
import pathlib
import re
import sys

MIN_META_STRING_LEN = 2

# 二进制全局替换时仅处理类名，避免误改 QSettings 等源码中的同名明文（如 isLanMode）
EXTRA_BINARY_CLASS_NAMES = frozenset(
    {
        "AesCrypto",
        "DeviceInfo",
        "EventHub",
        "NetworkUtils",
        "FileTransfer",
    }
)


def obf_token(name: str) -> str:
    pool = hashlib.md5(f"RemotePro:{name}".encode()).hexdigest()
    while len(pool) < len(name):
        pool += hashlib.md5(pool.encode()).hexdigest()
    return pool[: len(name)]


def _decode_c_escape(s: str) -> str:
    try:
        return bytes(s, "utf-8").decode("unicode_escape")
    except UnicodeDecodeError:
        return s


def _encode_c_escape(s: str) -> str:
    out: list[str] = []
    for ch in s:
        o = ord(ch)
        if ch == "\\":
            out.append("\\\\")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\n":
            out.append("\\n")
        elif ch == "\r":
            out.append("\\r")
        elif ch == "\t":
            out.append("\\t")
        elif o < 32 or o == 127:
            out.append(f"\\{o:03o}")
        else:
            out.append(ch)
    return "".join(out)


def collect_class_names(text: str, header_class: str | None = None) -> set[str]:
    names: set[str] = set()
    if header_class:
        names.add(header_class)
    for match in re.finditer(r"metaObjectData<([A-Za-z_][A-Za-z0-9_]*)", text):
        names.add(match.group(1))
    for match in re.finditer(
        r"([A-Za-z_][A-Za-z0-9_]*)::qt_create_metaobjectdata", text
    ):
        names.add(match.group(1))
    for match in re.finditer(r"qt_meta_stringdata_([A-Za-z_][A-Za-z0-9_]*)_t", text):
        names.add(match.group(1))
    return names


def collect_meta_strings(text: str) -> set[str]:
    """从 MOC 生成代码收集元对象字符串表中的全部条目。"""
    names: set[str] = set()

    block = re.search(
        r"QtMocHelpers::StringRefStorage\s+qt_stringData\s*\{(.*?)\n\s*\};",
        text,
        re.DOTALL,
    )
    if block:
        for match in re.finditer(r'"((?:[^"\\]|\\.)*)"', block.group(1)):
            names.add(_decode_c_escape(match.group(1)))

    for match in re.finditer(
        r"QT_MOC_LITERAL\([^)]+\)\s*,\s*//\s*\"([^\"]*)\"", text
    ):
        names.add(match.group(1))

    legacy = re.search(
        r"qt_meta_stringdata_\w+\s*=\s*\{[^}]*\},\s*\n\s*\"((?:[^\"\\]|\\.)*)\"",
        text,
        re.DOTALL,
    )
    if legacy:
        blob = _decode_c_escape(legacy.group(1))
        for part in blob.split("\0"):
            if part:
                names.add(part)

    names.discard("")
    return {n for n in names if len(n) >= MIN_META_STRING_LEN}


def _is_ident_byte(b: int) -> bool:
    return (48 <= b <= 57) or (65 <= b <= 90) or (97 <= b <= 122) or b == 95


def _replace_isolated(data: bytearray, old: bytes, new: bytes) -> bool:
    """仅替换独立标识符，避免 TcpServer 误改 QTcpServer 或 Itanium 符号名。"""
    changed = False
    start = 0
    while True:
        idx = data.find(old, start)
        if idx < 0:
            break
        prev_ok = idx == 0 or not _is_ident_byte(data[idx - 1])
        next_idx = idx + len(old)
        next_ok = next_idx >= len(data) or not _is_ident_byte(data[next_idx])
        if prev_ok and next_ok:
            data[idx:next_idx] = new
            changed = True
            start = next_idx
        else:
            start = idx + 1
    return changed


def collect_all_from_autogen(autogen_dir: pathlib.Path) -> tuple[set[str], set[str]]:
    meta: set[str] = set()
    classes: set[str] = set()
    if not autogen_dir.is_dir():
        return meta, classes
    for moc, header_class in _iter_moc_sources(autogen_dir):
        text = moc.read_text(encoding="utf-8", errors="ignore")
        meta |= collect_meta_strings(text)
        classes |= collect_class_names(text, header_class)
    meta |= classes
    classes |= EXTRA_BINARY_CLASS_NAMES
    return meta, classes


def obfuscate_moc_text(text: str, header_class: str | None = None) -> tuple[str, bool]:
    names = collect_meta_strings(text) | collect_class_names(text, header_class)
    if not names:
        return text, False

    changed = False
    for name in sorted(names, key=len, reverse=True):
        if len(name) < MIN_META_STRING_LEN:
            continue
        token = obf_token(name)
        if token == name:
            continue

        new_text = text
        new_text = new_text.replace(f'"{name}"', f'"{token}"')
        new_text = new_text.replace(f'"{name}",', f'"{token}",')

        esc = _encode_c_escape(name)
        tok_esc = _encode_c_escape(token)
        if esc != name:
            new_text = new_text.replace(f'"{esc}"', f'"{tok_esc}"')
            new_text = new_text.replace(f'"{esc}",', f'"{tok_esc}",')

        for sep in (r"\0", r"\\0"):
            new_text = new_text.replace(f"{sep}{esc}{sep}", f"{sep}{tok_esc}{sep}")
            new_text = new_text.replace(f'"{esc}{sep}', f'"{tok_esc}{sep}')
            new_text = new_text.replace(f"{sep}{esc}\"", f"{sep}{tok_esc}\"")

        if new_text != text:
            text = new_text
            changed = True
    return text, changed


def _iter_moc_sources(autogen_dir: pathlib.Path):
    for path in autogen_dir.rglob("moc_*.cpp"):
        yield path, path.stem[4:] if path.stem.startswith("moc_") else None
    for path in autogen_dir.rglob("*.moc"):
        yield path, None


def obfuscate_autogen_dir(autogen_dir: pathlib.Path) -> int:
    if not autogen_dir.is_dir():
        return 0
    touched = 0
    for moc, header_class in _iter_moc_sources(autogen_dir):
        text = moc.read_text(encoding="utf-8", errors="ignore")
        new_text, changed = obfuscate_moc_text(text, header_class)
        if changed:
            moc.write_text(new_text, encoding="utf-8")
            touched += 1
    if touched:
        print(f"obfuscated Qt meta strings in {touched} moc file(s) under {autogen_dir}")
    return 0


def obfuscate_binary(binary: pathlib.Path, autogen_dir: pathlib.Path) -> int:
    if not binary.is_file():
        print(f"skip missing binary: {binary}", file=sys.stderr)
        return 0

    _, classes = collect_all_from_autogen(autogen_dir)
    data = bytearray(binary.read_bytes())
    changed = False

    for name in sorted(classes, key=len, reverse=True):
        old = name.encode("utf-8")
        if len(old) < MIN_META_STRING_LEN:
            continue
        new = obf_token(name).encode("utf-8")
        if old == new:
            continue
        if _replace_isolated(data, old, new):
            changed = True

    if changed:
        binary.write_bytes(data)
        print(f"obfuscated class name strings in {binary}")
    return 0


def main() -> int:
    if len(sys.argv) < 2:
        print(
            "usage: qt-meta-obfuscate.py moc <autogen_dir>\n"
            "       qt-meta-obfuscate.py binary <binary> [autogen_dir]",
            file=sys.stderr,
        )
        return 1

    mode = sys.argv[1]
    if mode == "moc":
        if len(sys.argv) < 3:
            print("usage: qt-meta-obfuscate.py moc <autogen_dir>", file=sys.stderr)
            return 1
        return obfuscate_autogen_dir(pathlib.Path(sys.argv[2]))
    if mode == "binary":
        if len(sys.argv) < 3:
            print(
                "usage: qt-meta-obfuscate.py binary <binary> [autogen_dir]",
                file=sys.stderr,
            )
            return 1
        binary = pathlib.Path(sys.argv[2])
        autogen = pathlib.Path(sys.argv[3]) if len(sys.argv) > 3 else pathlib.Path()
        return obfuscate_binary(binary, autogen)
    print(f"unknown mode: {mode}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
