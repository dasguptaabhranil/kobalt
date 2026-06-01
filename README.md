# Kobalt

Kobalt is a kernel written from scratch (not totally, but very mostly) for x86_64. No libc, no Linux under the hood, just C and ASM. It boots, for now in QEMU, has a working scheduler, drivers, filesystems, and a security subsystem that would refuse to load its own drivers if you messed with the binary.

---

## What's in here

**Core kernel**
- Memory model, interrupt handling, SMP support
- EEVDF scheduler
- Intel AMX (matrix extension) support — tile config, BF16/INT8 ops, per-thread state

**TYKID** (Tell Your Kernel that I'm Driver)  
The driver security gate. Before any driver loads, TYKID:
1. Checks a kernel identity constant baked at compile time
2. Enumerates PCI/USB hardware and computes a topology hash
3. Runs the driver binary through a five-stage threat pipeline (ELF check → entropy → HMAC → SHA-256 → cert/pattern scan)
4. Enforces an IOMMU domain per driver (DMA-capable drivers can't run without IOMMU)
5. Keeps a background watchdog that re-HMACs every loaded driver every ~30 s; escalates to safe-mode or emergency shutdown on repeated failures

All TYKID source lives under `src/security/tykid/`.

**Subsystems**
- FlatFS and KangarooFS filesystems
- POSIX layer (`kposixz`)
- USB stack
- lwIP networking
- Intel HDA audio (codec enumeration, DMA ring buffer, PCM streams, volume/EQ)
- VESA/GOP graphics

**Syscall interface** — 105+ entries with a documented ABI.

---

## Key concepts

**`KOBALT_KERNEL_IDENT`** — a 64-bit compile-time constant that seeds every cryptographic operation in TYKID. Change the build, change the identity.

**TYKID gate seal** — a 64-bit value checked on every TYKID API entry. If the gate context is corrupted, calls are rejected immediately.

**Essential mask** — TYKID only loads drivers for hardware actually present. No IOMMU → NIC and USB host drivers are blocked entirely.

**HDA audio** — `hda_init()` is TYKID-gated. If TYKID blocks the driver binary, the sound subsystem stays uninitialised and all `hda_open_output()` calls return null.

---

## Build notes

- Compiler: **clang**, target `x86_64-pc-elf`, `-O2`
- `-ffreestanding -nostdinc -mno-red-zone -mno-sse -mno-sse2`
- `-fno-pic -fno-pie -mcmodel=large` — matches kernel addressing
- `KOBALT_KERNEL_IDENT` default is `0xA3F7C219DE40B851ULL`; regenerate per image in production

Fuzz harness (TYKID):
```sh
clang -DTYKID_FUZZ -fsanitize=fuzzer,address -O1 \
      src/security/tykid/src/tykid_fuzz.c [other tykid sources] \
      -o tykid_fuzz
./tykid_fuzz corpus/
```

---

## **Website? Docs? Still in development**

---

## Contributing, testing, breaking things

If you want to poke around:

- Build with `NDEBUG` undefined first. The debug paths (`hda_debug_dump()`, `hda_codec_debug_dump()`, `hda_mixer_debug_dump()`) print to UART and the framebuffer console via `kprintf`. Good first stop when something's wrong.
- TYKID has a libFuzzer harness (`tykid_fuzz.c`). Throw a corpus at it, see what breaks.
- To test TYKID rejection: modify a driver binary after it's been signed. TYKID's HMAC check (stage 3 of the threat pipeline) will catch it and block the load.
- The watchdog re-HMACs every active driver every ~30 s. If you want to test escalation, you can trigger repeated failures and watch it enter safe-mode, then shutdown.

If you're sending a patch:

- New verb, error code, or format? Add it to `hda.h` / the relevant enum — not buried in a `.c` file.
- New codec submodule? Wire `g_hda` fields through accessor functions. Don't touch the struct directly.
- New TYKID feature? Make sure `hda_debug_dump()` still reflects the state accurately.
- Always do a clean release build after testing debug — the debug stubs compile away entirely under `NDEBUG`.

---
