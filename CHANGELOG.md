# Changelog

## v1.0.0 — 2026-09-19

Initial public source release.

- True analog Vita right-stick camera for third-person gameplay.
- Dynamic `Camera_Update` discovery.
- Dynamic direct-callsite discovery.
- Dynamic camera-structure/X/Y target discovery.
- Shared user-memory mailbox.
- Pure MIPS shim with dynamic `jalr` to the active game camera routine.
- Automatic recovery across tested USA level transitions.
- USA UCUS-98633 supported.
- EU UCES-00420 explicitly documented as tested and currently unsupported.
- Bilingual technical comments and reverse-engineering documentation.
- AI/authorship disclosure added for the public repository.

## Internal development history

### v0.13

Made the camera function and its direct caller dynamic, fixing route changes observed when travelling between layouts such as Pokitaru/Ryllus and Kalidon.

### v0.12

Made the active camera structure and X/Y target addresses dynamic between levels that kept the same camera code layout.

### v0.11

Established the stable architecture: Vita input thread + shared user-memory mailbox + pure MIPS shim, avoiding direct calls from emulated game code into plugin/kernel C.
