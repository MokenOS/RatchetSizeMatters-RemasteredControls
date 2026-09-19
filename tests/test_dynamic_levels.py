from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
c = (ROOT / 'src' / 'main.c').read_text(encoding='utf-8')

# v0.13 keeps the v0.12 fix for levels that retain the same Camera_Update
# code but move the camera structure (planet 1 -> planet 2).
assert 'RatchetRemastered v0.13 dynamic camera routes' in c
assert '#define MAILBOX_TARGET_X_OFF 0x10u' in c
assert '#define MAILBOX_TARGET_Y_OFF 0x14u' in c
assert 'sharedTargetXAddr' in c and 'sharedTargetYAddr' in c

maint_start = c.index('static int maintainCameraRoute(void)')
maint_end = c.index('static int setupSharedRuntime', maint_start)
maint = c[maint_start:maint_end]
assert 'deriveCameraStruct(cameraFunc)' in maint
assert 'if (st != cameraStruct)' in maint
assert 'write32(sharedFlagsAddr, 0);' in maint
assert 'write32(sharedTargetXAddr, st + 0x274u);' in maint
assert 'write32(sharedTargetYAddr, st + 0x278u);' in maint
assert maint.index('write32(sharedFlagsAddr, 0);') < maint.index('write32(sharedTargetXAddr, st + 0x274u);')

# If the current level reloads the exact same route, re-apply only when the
# original expected JAL is present. Unknown code forces a full rediscovery.
assert 'if (cur == expectedOriginalCall)' in maint
assert 'write32(callsite, expectedPatchedCall);' in maint
assert 'return rescanCameraRoute();' in maint

print('dynamic-level regression checks: PASS')
