from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def test_readme_public_facts():
    text = read("README.md")
    assert "UCUS-98633" in text
    assert "UCES-00420" in text
    assert "currently unsupported" in text.lower()
    assert "true analog" in text.lower()
    assert "ChatGPT" in text
    assert "OpenAI" in text
    assert "Rafitalocotron" in text


def test_repo_docs_do_not_contain_distribution_plans():
    combined = "\n".join(
        p.read_text(encoding="utf-8")
        for p in ROOT.rglob("*.md")
        if ".git" not in p.parts
    ).lower()
    assert "r/vitahacks" not in combined
    assert "r/vitapiracy" not in combined
    assert "r/psp" not in combined
    assert "autoplugin" not in combined


def test_compatibility_table_is_explicit():
    text = read("COMPATIBILITY.md")
    assert "UCUS-98633" in text and "Tested / Supported" in text
    assert "UCES-00420" in text and "Tested / Unsupported" in text


def test_technical_docs_cover_hook_architecture():
    tech = read("docs/TECHNICAL.md")
    for term in ["0x274", "0x278", "mailbox", "jalr", "Camera_Update", "MIPS", "deadzone", "cache"]:
        assert term.lower() in tech.lower()


def test_reverse_engineering_history_is_documented():
    history = read("docs/REVERSE_ENGINEERING.md")
    for term in ["PPSSPP", "v0.11", "v0.12", "v0.13", "Kalidon"]:
        assert term in history
