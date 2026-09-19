# Technical Notes

## Scope

This document describes the v1.0.0 / v0.13 architecture for the supported USA build, UCUS-98633, running through Adrenaline/Epinephrine on PS Vita.

The important design goal is not merely to map the right stick to PSP buttons. The plugin feeds true analog values into the game's own third-person camera path.

## Data flow

```text
Vita right stick
      ↓
kernel/input monitor thread
      ↓
shared mailbox in PSP user RAM
      ↓
pure user-space Allegrex/MIPS shim
      ↓
jalr to current Camera_Update
      ↓
write analog camera X/Y targets
      ↓
return to original game caller
```

The monitor thread samples `SceCtrlData.Rsrv[0]` and `SceCtrlData.Rsrv[1]` as the Vita right-stick axes, applies a deadzone and response curve, and publishes the resulting values to the mailbox.

## Why a mailbox exists

Earlier prototypes attempted to call plugin/kernel C directly from a game-side hook. That boundary repeatedly crashed or froze Adrenaline on real Vita hardware even though the hook itself was reached.

The stable design keeps the game side entirely in user-memory MIPS. The plugin thread writes data into ordinary PSP user RAM, and the shim reads only that memory. No game-to-kernel C helper or syscall trampoline is needed.

## Mailbox

The mailbox stores:

- activation flags;
- analog X value;
- analog Y value;
- raw right-stick bytes for diagnostics;
- active camera X target pointer;
- active camera Y target pointer;
- active `Camera_Update` function pointer.

The camera function pointer is important because a level package can move code as well as data.

## Camera targets

For UCUS-98633, the reverse-engineered third-person camera routine writes its targets at:

```text
cameraStruct + 0x274  -> X
cameraStruct + 0x278  -> Y
```

The scanner also verifies the corresponding `swc1` stores inside the known `Camera_Update` pattern.

The current analog amplitudes are defined by:

```c
X_FULL_TARGET_RAD
Y_FULL_TARGET_RAD
STICK_DEADZONE
```

Changing these constants is the safest starting point for sensitivity/deadzone experiments. Any such change should still be tested on real hardware.

## Dynamic Camera_Update discovery

The initial working levels used one `Camera_Update` address, but Kalidon demonstrated that the function and its direct caller can relocate with another level package.

The plugin therefore scans the active `rcp1` text region for the known UCUS-98633 camera signature rather than assuming one permanent address.

Once the function is found, the plugin:

1. derives the active camera structure from instructions inside `Camera_Update`;
2. finds the direct JAL that calls this function;
3. publishes the current function and target pointers to the mailbox;
4. patches only the expected direct callsite to jump to the shim.

## Dynamic jalr

The MIPS shim is allocated once. It does not embed a level-specific `jal Camera_Update` instruction.

Instead it loads the current camera function pointer from the mailbox and uses:

```text
jalr t9
```

This allows the same shim to survive level transitions where `Camera_Update` moves.

## Safe repatching

The plugin only modifies a discovered callsite when the instruction is one of two expected values:

- the exact original JAL to the current `Camera_Update`;
- the exact JAL to this plugin's own shim.

If an unknown instruction appears at the location, the plugin does not blindly overwrite it. It performs route rediscovery instead.

## Level/route changes

Two different cases were observed during development:

- Same camera code, different camera data structure.
- Different camera code/callsite plus a different camera data structure.

`maintainCameraRoute()` handles the common fast path. `rescanCameraRoute()` handles full rediscovery when the active route is no longer valid.

## Cache coherency

Whenever executable MIPS code is patched, data-cache writeback and instruction-cache invalidation are required. The plugin uses PSP kernel cache functions after publishing executable changes so the emulated PSP CPU sees the updated instructions.

## MIPS shim invariants

The regression tests intentionally protect several details:

- the shim remains pure user-memory MIPS;
- the incoming game stack pointer is not changed before the original camera routine executes;
- the current camera routine is called through `jalr`;
- the shim layout and branch offsets remain consistent;
- no direct helper call back into plugin/kernel C is introduced.

These constraints are based on real-hardware failures seen during earlier prototypes.

## Debugging

Runtime diagnostics are written to:

```text
ms0:/seplugins/ratchet_debug.txt
```

The log can show:

- `rcp1` text-region changes;
- camera function and callsite changes;
- camera structure and X/Y target changes;
- shim call counts;
- raw right-stick values and activation flags during the startup diagnostic window.

File I/O is intentionally kept out of the hot MIPS shim.

## Regional ports

A new region should not be ported by simply copying USA addresses. The preferred process is:

1. run the target region in PPSSPP with the debugger enabled;
2. identify the equivalent `Camera_Update` routine;
3. confirm the X/Y camera stores and structure offsets;
4. compare the regional function shape with the USA signature;
5. add a separate signature/profile if needed;
6. retain the mailbox/shim boundary unless hardware evidence shows a better safe design;
7. validate on a real Vita before claiming support.
