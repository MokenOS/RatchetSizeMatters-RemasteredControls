from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def test_public_tree_exists():
    assert (ROOT / "src" / "main.c").is_file()
    assert (ROOT / "src" / "exports.exp").is_file()
    assert (ROOT / "Makefile").is_file()


def test_no_old_build_archives_in_public_tree():
    names = {p.name for p in ROOT.rglob("*") if p.is_file()}
    assert not any("v0_11" in n or "v0_12" in n for n in names)
