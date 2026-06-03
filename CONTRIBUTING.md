# Contributing to Kobalt

Thanks for taking the time to look at this. Kobalt is a hobby kernel project and it moves at its own pace, so contributions are welcome as long as they're focused and easy to follow.

## Who Are You?

Pick whichever fits closest. These aren't rigid buckets — they just point you at the parts of this doc that are most relevant to you.

---

**New Kernel Developer**
Kobalt is not a gentle introduction to kernel development. It's freestanding, has no standard library, and doesn't follow Linux conventions. If you're just starting out: get comfortable with x86_64 fundamentals first (interrupts, paging, privilege levels — the OSDev wiki is fine for this). Then read through `hda.c` or `tykid_core.c` to get a feel for the code before touching anything. Start small, and open an issue before committing to a larger change.

**Researcher**
The most documented subsystems are TYKID and Intel HDA — both have detailed write-ups in the docs site covering architecture, data structures, and design rationale. If you're studying something specific (the EEVDF scheduler, IOMMU integration, the HDA codec enumeration model) and the docs don't cover it well enough, opening an issue to ask is fine. Better docs are a valid contribution.

**Security Expert**
TYKID is the main thing here. It's a driver-gate subsystem with a multi-layer chain of trust: identity binding via `KOBALT_KERNEL_IDENT`, hardware topology sealing, binary integrity checks, and a runtime watchdog. The extended hyper-seal mixes CPU microcode revision, SMBIOS UUID, platform security version, and a BLAKE2s kernel image hash through a Threefish-inspired ARX network. If you find something wrong, open an issue with a clear description of the problem — responsible disclosure is appreciated. If you want to harden something, discuss it first; security changes without clear threat model reasoning won't land.

**Backport / Maintenance Engineer**
Kobalt doesn't have a stable branch or an LTS model right now. If that changes, this section will be updated. For now, if you're maintaining a fork and need to backport something, the subsystem-specific checklists in this document (especially the HDA one) are the most useful reference for what changes need to stay in sync.

**System Administrator**
Kobalt is not a production OS "yet". It's a kernel, If you're configuring or troubleshooting a Kobalt instance, the docs site is the right starting point — particularly the Booting Kobalt page and the TYKID section, since TYKID gates driver loading at boot and is the most common source of "why isn't my hardware working" problems.

**Maintainer**
If you're reviewing patches or leading a subsystem, the bar for what lands is: focused, buildable in debug and release, doesn't silently break unrelated subsystems, and has a clear commit message that explains *why* not just *what*. Security-touching changes (anything in `src/security/`) need extra scrutiny — don't approve them on trust alone. Use the subsystem checklists in this file as a review aid.

**AI Coding Assistant**
A few things worth knowing before generating patches for this codebase: Kobalt is freestanding — no standard library, no libc, no POSIX. Don't emit `#include <string.h>`, `malloc()`, `printf()`, or anything that assumes a hosted environment. MMIO accesses must use `volatile`-qualified pointer casts; the compiler will silently elide them otherwise. The compiler is clang, not GCC — don't emit GCC-specific attributes or extensions. Security-critical files (anything under `src/security/tykid/`) have invariants that are non-obvious from local context; generating patches for those without understanding the full subsystem is likely to produce something subtly wrong. When in doubt, generate a minimal diff and leave a comment explaining the reasoning.

---

## Before You Start

Search the existing issues and pull requests first. If something is already being worked on, a comment is more useful than a duplicate PR. If you're planning something non-trivial, open an issue first so we can talk about it before you put in the work.

## How to Contribute

1. Fork the repo and create a branch from `main`.
2. Make your changes. Keep them focused — one thing per PR.
3. Test your changes. At minimum, verify a clean build in both debug and release.
4. Open a pull request with a clear description of what you changed and why.

There's no formal PR template. Just be clear about what the patch does.

## Code Style

Kobalt is written in C, freestanding, targeting x86_64. A few things to keep in mind:

- Compiler is **clang**. Don't assume GCC-specific extensions work.
- No standard library. Don't reach for `<string.h>` or friends — look at what's already in the relevant internal headers.
- No FP/SIMD instructions unless you're explicitly in a context that permits them. The kernel stack doesn't tolerate it.
- Keep `volatile` on all MMIO accesses. The compiler will silently elide them otherwise.
- Match the style of the surrounding code. This isn't a rigid styleguide project, just use common sense.

## Subsystem-Specific Notes

### TYKID

TYKID is the driver-gate security subsystem. It's security-critical, so changes here get extra scrutiny.

- `KOBALT_KERNEL_IDENT` is the root of trust. Don't change its value without understanding what it affects — it flows into every key derivation, every seal, and every mixing constant.
- If you add a new public API function, it belongs in `tykid.h`. Internal helpers stay in `tykid_internal.h`.
- The watchdog re-verifies HMACs and seals on every sweep. If you change what gets signed or sealed, update the watchdog recheck logic too.
- Optimisation is `-O2`, not `-O3`. Dead store elimination at `-O3` can silently remove zeroing of sensitive buffers. Don't bump it.

### Intel HDA

Before submitting a patch to the HDA subsystem, run through this checklist:

- **New codec submodule?** Declare any `g_hda` fields it needs via new accessor functions in `hda.c`/`hda.h`. Don't add fields to the anonymous `g_hda` struct without also updating `hda_debug_dump()`.
- **New verb?** The `HDA_VERB_*` or `HDA_PARAM_*` constant goes in `hda.h`, not in the C file.
- **Format change?** Update `hda_calc_fmt()` in `hda.c` and extend the sample rate table in the documentation.
- **New error code?** Add it to the `hda_status_t` enum and to the `hda_strerror()` switch in `hda.c`. Both, not just one.
- **Stream count change?** Update `HDA_MAX_STREAMS`, `HDA_MAX_OUTPUT_STREAMS`, and `HDA_MAX_INPUT_STREAMS` in `hda.h` together. The static `g_dma_buf` and `g_bdl` arrays are sized from `HDA_MAX_STREAMS`, so they need to stay in sync.
- **EQ band count change?** Update `HDA_MAX_EQ_BANDS` and verify the coefficient slot layout in `hda_apply_eq_to_codec()` still fits within the processing widget's coefficient address space.

Always build with `NDEBUG` undefined first to exercise the debug paths, then confirm a clean release build.

### Other Subsystems

If you're touching FlatFS, the POSIX layer, the USB stack, or lwIP networking, read the relevant documentation in the docs site before submitting. These subsystems have non-obvious invariants that aren't always visible from the code alone.

## What Makes a Good PR

- It does one thing.
- The commit message explains *why*, not just *what*.
- It doesn't break the build in either debug or release mode.
- It doesn't silently change behaviour in an unrelated subsystem.

## What Will Probably Be Rejected

- Large refactors without prior discussion.
- Anything that introduces a standard library dependency in a freestanding context.
- Security-critical changes to TYKID without a clear explanation of the threat model reasoning.
- Code that only works with GCC.

## Questions

Open an issue. That's the right place for design questions, build problems, or anything else that doesn't fit neatly into a PR.
