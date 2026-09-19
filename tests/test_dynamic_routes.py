from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
c = (ROOT / 'src' / 'main.c').read_text(encoding='utf-8')

# Regression for the Kalidon -> Pokitaru/Ryllus -> Kalidon failure:
# v0.13 must treat Camera_Update AND its direct caller as dynamic level data.
assert 'RatchetRemastered v0.13 dynamic camera routes' in c

# Mailbox now carries the current Camera_Update function pointer as well as
# target X/Y pointers, so the shim itself is not tied to the level present
# when the PRX starts.
assert '#define MAILBOX_CAMERA_FUNC_OFF 0x18u' in c
assert 'sharedCameraFuncAddr' in c

fn_start = c.index('static u32 makeSharedMemoryShim')
fn_end = c.index('static int signatureMatches')
fn = c[fn_start:fn_end]
assert 'u32 cameraFuncSlot' in fn
assert 'jalr  t9' in fn
assert 'lw    t9,0(t0)' in fn
assert 'makeJal(original)' not in fn
assert 'u32 original' not in fn

# The active route must be re-discovered throughout gameplay, not only during
# initial installation. This is what allows BD50/AE14 <-> BD90/AE44 swaps.
assert 'static int rescanCameraRoute(void)' in c
rescan_start = c.index('static int rescanCameraRoute(void)')
rescan_end = c.index('static int maintainCameraRoute(void)', rescan_start)
rescan = c[rescan_start:rescan_end]
assert 'findCameraFunction(moduleTextAddr, moduleTextSize)' in rescan
assert 'findDirectJalRef(moduleTextAddr, moduleTextSize, func)' in rescan
assert 'write32(sharedCameraFuncAddr, func);' in rescan
assert 'write32(sharedTargetXAddr, st + 0x274u);' in rescan
assert 'write32(sharedTargetYAddr, st + 0x278u);' in rescan
assert 'write32(cs, expectedPatchedCall);' in rescan

# The permanent monitor loop must call the route maintainer before sampling
# the right stick.
loop_anchor = c.index('while (1) {')
loop = c[loop_anchor:]
assert 'maintainCameraRoute();' in loop
assert loop.index('maintainCameraRoute();') < loop.index('sceCtrlPeekBufferPositive')

# A changed/invalid camera signature must cause a rescan; a restored original
# JAL for the SAME route may be cheaply re-patched without a full scan.
maint_start = c.index('static int maintainCameraRoute(void)')
maint_end = c.index('static int patchText', maint_start)
maint = c[maint_start:maint_end]
assert 'signatureMatches(cameraFunc' in maint
assert 'if (cur == expectedOriginalCall)' in maint
assert 'return rescanCameraRoute();' in maint

# The v0.12 one-shot installer guard must not block later route discovery.
assert 'if (callsite) return 1;' not in c

print('dynamic-route regression checks: PASS')
