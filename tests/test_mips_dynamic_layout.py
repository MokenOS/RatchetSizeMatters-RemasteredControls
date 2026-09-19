from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
s = (ROOT / 'src' / 'main.c').read_text(encoding='utf-8')
fn = s[s.index('static u32 makeSharedMemoryShim'):s.index('static int signatureMatches')]

# v0.13 adds three words relative to v0.12: load Camera_Update from mailbox
# and call it via jalr rather than embedding a level-specific JAL.
m = re.search(r'const u32 words = (\d+);', fn)
assert m and int(m.group(1)) == 41
writes = [int(x, 16) for x in re.findall(r'write32\(s \+ 0x([0-9A-Fa-f]+),', fn)]
assert len(writes) == 41, (len(writes), writes)
assert writes == list(range(0, 41 * 4, 4)), writes

# Dynamic function call sequence.
assert '0x8D190000u' in fn   # lw t9,0(t0)
assert '0x0320F809u' in fn   # jalr ra,t9
assert 'makeJal(' not in fn

# X beq @0x50: PC+4=0x54; +8 words -> 0x74 (Y block).
assert fn.count('0x11400008u') == 2
assert 0x54 + 8 * 4 == 0x74
# Y beq @0x78: PC+4=0x7C; +8 words -> 0x9C (return).
assert 0x7C + 8 * 4 == 0x9C

print('dynamic MIPS layout checks: PASS')
