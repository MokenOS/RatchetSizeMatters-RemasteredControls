from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
c = (ROOT / 'src' / 'main.c').read_text(encoding='utf-8')
mk = (ROOT / 'Makefile').read_text(encoding='utf-8')

assert 'RatchetRemastered v0.13 dynamic camera routes' in c
assert '#include <pspctrl.h>' in c
assert 'sceCtrlPeekBufferPositive' in c
assert 'pad.Rsrv[0]' in c and 'pad.Rsrv[1]' in c
assert 'X_FULL_TARGET_RAD' in c and 'Y_FULL_TARGET_RAD' in c

# Crash-free architecture remains: user shim never calls plugin/kernel C or a
# syscall helper. It only calls the current game Camera_Update via user MIPS.
fn_start = c.index('static u32 makeSharedMemoryShim')
fn_end = c.index('static int signatureMatches')
fn = c[fn_start:fn_end]
assert 'applyRightStick' not in c
assert 'makeSyscallStub' not in c
assert 'makeJal(' not in fn
assert 'jalr  t9' in fn
assert 'lw    t9,0(t0)' in fn

# Incoming stack pointer remains untouched before Camera_Update.
pos_call = fn.index('jalr  t9')
assert '27BD' not in fn[:pos_call], 'SP must not change before Camera_Update'

# Dynamic route + targets all live in the mailbox.
assert '#define MAILBOX_CAMERA_FUNC_OFF 0x18u' in c
assert 'sharedCameraFuncAddr' in c
assert 'sharedTargetXAddr' in c and 'sharedTargetYAddr' in c
assert 'rescanCameraRoute();' in c
assert 'maintainCameraRoute();' in c

# One monitor/installer only; no module-start-handler race like v0.8.
assert 'sctrlHENSetStartModuleHandler' not in c

# Known-good PRX link settings retained.
assert '-nostartfiles' in mk
assert 'USE_KERNEL_LIBS = 1' in mk
assert 'PSP_FW_VERSION = 660' in mk

print('static regression checks: PASS')
