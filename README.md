# Ratchet & Clank: Size Matters — Remastered Controls

True analog right-stick camera controls for **Ratchet & Clank: Size Matters** on PS Vita through Adrenaline/Epinephrine.

This plugin does **not** simply remap the PSP L/R camera buttons to the Vita right stick. It injects analog X/Y camera values into the game's own third-person camera update path, while keeping the original left stick and L/R controls intact.

## Features

- True analog right-stick camera in third person.
- Horizontal and vertical camera control.
- Original left-stick movement remains untouched.
- Original L/R camera buttons remain available.
- Deadzone and analog response curve for the Vita right stick.
- Dynamic camera-structure discovery across levels.
- Dynamic `Camera_Update` and callsite rediscovery when level code moves.
- Pure user-space MIPS shim with a shared-memory mailbox.
- Automatic recovery after level/planet transitions on the supported USA build.
- Debug logging to help with ports and troubleshooting.

## Compatibility

| Region | Title ID | Status | Notes |
|---|---|---|---|
| USA | **UCUS-98633** | **Tested / Supported** | Hardware-tested on PS Vita with Adrenaline/Epinephrine |
| Europe | **UCES-00420** | **Tested / currently unsupported** | Current USA camera signature/profile does not activate correctly |
| Japan | UCJS-10052 | Untested | No compatibility claim |
| Korea | UCKS-45048 | Untested | No compatibility claim |

The current public source targets **UCUS-98633**. Do not assume another region is compatible just because the game title is the same.

See [COMPATIBILITY.md](COMPATIBILITY.md) for details.

## Installation

Requirements:

- PS Vita
- Adrenaline or Epinephrine
- Ratchet & Clank: Size Matters **USA / UCUS-98633**

Copy the compiled plugin to:

```text
ux0:/pspemu/seplugins/ratchet_remastered.prx
```

Then enable it in the Adrenaline PSP plugin configuration. A typical `game.txt` entry is:

```text
ms0:/seplugins/ratchet_remastered.prx 1
```

Fully restart Adrenaline/Epinephrine after replacing or updating the PRX.

No game files, ISO images, save files, keys, firmware files or copyrighted assets are included in this repository.

## Build from source

A PSP development environment with **PSPSDK** is required.

```sh
python -m pytest tests -q
make clean
make
```

Expected output when PSPSDK is configured correctly:

```text
ratchet_remastered.prx
```

The repository tests validate the known MIPS shim layout, camera-route discovery assumptions and publication structure. They do not replace real-hardware testing.

## How it works

The final architecture is intentionally split across the Vita-side plugin and PSP user memory:

```text
Vita right stick
      ↓
monitor/input thread
      ↓
shared mailbox in PSP user RAM
      ↓
pure user-space MIPS shim
      ↓
indirect jalr to the current Camera_Update
      ↓
analog X/Y written to the active camera structure
      ↓
return to the game
```

The supported USA build stores the analog camera targets at offsets `+0x274` and `+0x278` inside the active camera structure. The plugin discovers the current camera route instead of relying on one permanent address for the entire game.

More details: [docs/TECHNICAL.md](docs/TECHNICAL.md)

## How this was made

The project was developed iteratively using PPSSPP's debugger together with repeated testing on a real PS Vita.

The investigation progressed roughly like this:

1. Identify the Vita right-stick bytes exposed to the PSP control structure.
2. Locate the third-person camera update routine.
3. Identify the horizontal camera target.
4. Identify and validate the vertical camera target.
5. Build an initial native Vita/Adrenaline plugin.
6. Discover that calls from emulated game code into plugin/kernel C caused crashes on real hardware.
7. Replace that boundary with a pure user-space MIPS shim and shared-memory mailbox.
8. Discover that camera data structures move between planets.
9. Discover in Kalidon that `Camera_Update` and its direct caller can also move.
10. Make the camera function, callsite and camera structure dynamic in v0.13.

The USA build has been tested through multiple planet transitions, including the Staff Off easter-egg area.

More history: [docs/REVERSE_ENGINEERING.md](docs/REVERSE_ENGINEERING.md)

## AI / authorship disclosure

The source implementation in this repository was generated with **ChatGPT by OpenAI** during an iterative reverse-engineering and testing process.

**Rafitalocotron** did not personally write the C/MIPS implementation line by line. His contribution was to conceive and direct the project, run the reverse-engineering experiments, test builds on real PS Vita hardware, provide logs and debugger results, identify regressions, and decide the direction of each iteration.

This project is also a starting point for learning and making small contributions to the PS Vita/PSP scene. The intention is to progressively build more directly while using AI as a tool to accelerate research, implementation, debugging and idea generation when useful.

### Declaración de IA / autoría

La implementación del código fuente de este repositorio fue generada con **ChatGPT de OpenAI** durante un proceso iterativo de ingeniería inversa y pruebas.

**Rafitalocotron** no escribió personalmente línea por línea la implementación C/MIPS. Su aportación fue idear y dirigir el proyecto, realizar las pruebas de ingeniería inversa, probar las builds en una PS Vita real, aportar logs y resultados del debugger, detectar regresiones y decidir la dirección de cada iteración.

Este proyecto también es un punto de entrada para aprender y hacer pequeñas aportaciones a la scene de PS Vita/PSP. La intención es ir desarrollando cada vez más de forma directa y utilizar la IA como herramienta para acelerar investigación, implementación, depuración e ideas cuando aporte valor.

## Debug log

The plugin writes diagnostic information to:

```text
ms0:/seplugins/ratchet_debug.txt
```

Useful entries include camera route changes, camera structure changes, current camera function/callsite and right-stick sampling diagnostics.

## Known limitations

- Third-person camera only.
- First-person camera is currently unsupported.
- USA **UCUS-98633** is the only supported game build in v1.0.0.
- Europe **UCES-00420** has been tested and is currently unsupported.
- Other regional releases have not been tested.
- This project does not modify rendering resolution, frame rate, FOV or game content.

## Porting to another region

The v0.13 architecture is already designed to avoid fixed per-level addresses, but it still relies on a camera signature/profile proven against the USA executable and level modules.

A regional port should begin by using PPSSPP's debugger to identify the equivalent `Camera_Update` routine, confirm the camera X/Y stores, then adapt the scanner/profile while keeping the shared-mailbox and MIPS-shim architecture unchanged where possible.

## Contributing

Contributions are welcome, especially:

- Regional ports.
- Additional hardware validation.
- Better documentation of the High Impact Games PSP engine.
- Carefully tested camera options such as sensitivity or inversion.

Please avoid changing the hook architecture casually. The current boundary exists because earlier designs that called plugin/kernel C directly from emulated game code crashed Adrenaline on real hardware.

## Legal

This is an unofficial fan-made homebrew plugin. It is not affiliated with or endorsed by Sony Interactive Entertainment, Insomniac Games, High Impact Games or the rights holders of Ratchet & Clank.

This repository contains only original mod/plugin source code and documentation. It does not distribute the game, copyrighted game data or proprietary SDK files.

## License

MIT — see [LICENSE](LICENSE).
