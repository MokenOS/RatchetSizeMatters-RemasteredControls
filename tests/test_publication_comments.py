from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEXT = (ROOT / "src" / "main.c").read_text(encoding="utf-8")

REQUIRED = [
    "EN: Right stick",
    "ES: Stick derecho",
    "EN: Shared mailbox",
    "ES: Buzon compartido",
    "EN: Pure MIPS shim",
    "ES: Shim MIPS puro",
    "EN: Dynamic Camera_Update",
    "ES: Camera_Update dinamico",
    "EN: Safe repatching",
    "ES: Reaplicacion segura",
    "EN: Cache",
    "ES: Cache",
]


def test_key_sections_have_bilingual_comments():
    for marker in REQUIRED:
        assert marker in TEXT


def test_ai_disclosure_header_present():
    assert "ChatGPT" in TEXT
    assert "OpenAI" in TEXT
    assert "Rafitalocotron" in TEXT
