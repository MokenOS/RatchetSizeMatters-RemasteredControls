from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
s = (ROOT / 'src' / 'main.c').read_text(encoding='utf-8')
fn = s[s.index('static u32 makeSharedMemoryShim'):s.index('static int signatureMatches')]

m = re.search(r'const u32 words = (\d+);', fn)
assert m and int(m.group(1)) == 41
writes = [int(x, 16) for x in re.findall(r'write32\(s \+ 0x([0-9A-Fa-f]+),', fn)]
assert writes == list(range(0, 0xA4, 4)), writes

# jalr at 0x18 returns to 0x20 after its delay slot, where caller RA is restored.
assert 'write32(s + 0x18, 0x0320F809u);' in fn
assert 'write32(s + 0x20, 0x3C010000u | rhi);' in fn

# The game callsite can still reach a high user-RAM shim with a normal JAL in
# every observed address range (same 256 MiB pseudo-direct jump region).
for addr in (0x0913AE14, 0x0913AE44, 0x0BBFFD00):
    assert (addr >> 28) == 0

print('MIPS layout checks: PASS')
