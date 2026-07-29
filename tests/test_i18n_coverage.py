#!/usr/bin/env python3

import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CHINESE = re.compile(r"[\u3400-\u9fff]")
STRING = re.compile(r'"((?:[^"\\]|\\.)*)"')
TRANSLATION = re.compile(r'TEXT\("((?:[^"\\]|\\.)*)"')


def main() -> int:
    i18n_source = (ROOT / "source/i18n.c").read_text(encoding="utf-8")
    translated = set(TRANSLATION.findall(i18n_source))
    missing: list[str] = []

    for relative in ("source/main.c", "source/ui.c"):
        source = (ROOT / relative).read_text(encoding="utf-8")
        for match in STRING.finditer(source):
            value = match.group(1)
            if CHINESE.search(value) and value not in translated:
                line = source.count("\n", 0, match.start()) + 1
                missing.append(f"{relative}:{line}: {value}")

    assert not missing, "untranslated user-facing strings:\n" + "\n".join(
        missing
    )
    print("i18n coverage tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
