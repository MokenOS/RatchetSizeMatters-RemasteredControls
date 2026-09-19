# Reverse-Engineering History

This document summarizes how the right-stick camera mod was discovered and stabilized. It intentionally avoids distributing copyrighted game binaries or large disassembly dumps.

## 1. PPSSPP as the investigation environment

PPSSPP's debugger was used as the primary research environment because it allowed repeatable RAM reads, disassembly and breakpoints while running the USA build UCUS-98633.

The first useful input finding was that the Vita/PSP control structure exposed the right stick through the bytes corresponding to `Rsrv[0]` and `Rsrv[1]`.

## 2. Finding the third-person camera path

Debugger experiments isolated the third-person camera update logic and then the writable targets used for camera motion.

The horizontal target was identified first. The relevant camera structure store corresponded to offset `+0x274`.

The vertical path was then traced. Normal third-person gameplay often wrote zero through that path, but injecting an analog value at the corresponding `+0x278` target produced real vertical camera movement.

That established the core idea: right-stick input could control the internal camera directly rather than emulate L/R button presses.

## 3. Early native plugin attempts

Several native plugin prototypes were built after the PPSSPP proof of concept.

A major hardware-only failure emerged: designs where the emulated game-side hook called plugin/kernel C or a syscall-style helper froze or crashed Adrenaline on a real Vita.

That evidence forced an architectural change rather than another address tweak.

## 4. v0.11 — stable shared-memory MIPS hook

v0.11 established the crash-free boundary that remains in the current project:

```text
Vita input thread -> shared PSP user RAM -> pure MIPS shim -> original game camera routine
```

The game executes only user-space MIPS and never calls plugin/kernel C directly.

This version provided working true analog third-person camera control on hardware.

## 5. v0.12 — dynamic camera structures

Testing across planets showed that the camera structure address could change even when the camera function and callsite stayed at the same addresses.

For example, two tested levels used the same camera code route but different camera structure bases.

v0.12 therefore made the X/Y target pointers dynamic and updated them from the active camera structure.

This fixed the first cross-planet failure, but not every one.

## 6. Kalidon exposed another relocation

A later scan in Kalidon showed a more important change: `Camera_Update` itself and its direct caller had moved.

The internal structure of the function was still recognizable, including the same X/Y camera stores, but the route was no longer located at the earlier addresses.

A particularly useful hardware observation followed: if Adrenaline was restarted while already in Kalidon, the plugin worked there; travelling back to earlier planets then broke it, and returning to Kalidon made it work again. That behavior demonstrated that the plugin was binding itself to whichever camera route was active when it initialized.

## 7. v0.13 — dynamic camera routes

v0.13 removed that startup binding.

The monitor now treats all of these as dynamic level data:

- `Camera_Update`;
- its direct JAL callsite;
- the camera structure;
- X/Y target pointers.

The permanent MIPS shim also stopped embedding a direct call to one camera function. It reads the active function pointer from the mailbox and calls it through `jalr`.

Hardware testing then succeeded across repeated planet changes, including the Staff Off easter-egg area.

## 8. Regional compatibility test

The European build UCES-00420 was tested after the USA implementation stabilized. The USA profile did not work there.

That result is why the public v1.0.0 release claims support only for UCUS-98633. The architecture is suitable for regional profiles, but the EU camera signature/path must be reverse engineered separately before support is claimed.

## 9. Why this history matters

The final code contains several choices that can look unnecessarily defensive if the failed prototypes are ignored. In particular:

- avoiding game-to-kernel C calls;
- keeping the shim in PSP user memory;
- rediscovering camera routes dynamically;
- validating the expected callsite instruction before patching;
- protecting MIPS layout with static regression tests.

Those choices are not theoretical style preferences. They came directly from debugger evidence and real PS Vita hardware behavior.
