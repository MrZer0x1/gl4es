# gl4es launch-fix + FPE optimization

Drop-in replacements for **4 files** in your gl4es tree.
This is on top of the tree you sent (with shadow support, half-float
probe, dummy color FBO, NULL guard, and gamma already applied).

## What's fixed here

### 1. Android.mk — actual reason for the launch crash

Three bugs in your current `Android.mk`:

| Line | Original | Fix |
|------|----------|-----|
| 97 | `#LOCAL_CFLAGS += -DNO_INIT_CONSTRUCTOR` (commented out) | uncommented |
| 98 | `-DDEFAULT_ES=2` | `-DDEFAULT_ES=3` |
| 99 | `//TODO: maybe temporary?` (C-style comment) | `# TODO: maybe temporary?` |

**The crash you keep seeing** (`SIGSEGV in __strcpy_chk` while
`libGL.so` is being constructed by the dynamic linker) happens because
`NO_INIT_CONSTRUCTOR` is **not** defined. Without it, `init.c` line 82
keeps `__attribute__((constructor(101)))` on `initialize_gl4es()`, and
the function runs before `getenv("OPENMW_USER_FILE_STORAGE")` can
resolve. Even though there's now a NULL-guard, the deeper assumption
that gl4es is ready before JNI calls into it is broken — OpenMW's JNI
glue calls `dlopen` itself and explicitly initializes gl4es later.
`NO_INIT_CONSTRUCTOR` defers the initialization until OpenMW asks for
it, which is the design.

The `//TODO` line is also a real bug — `make` doesn't recognize `//`
as a comment, only `#`. Depending on the make version, that line is
either silently ignored or treated as a target name.

### 2. FPE input-cache fast path (the optimization you asked for)

In `fpe_program()` we now skip `fpe_ReleventState()` and the khash
lookup when the unfiltered `fpe_state` is byte-identical to the last
fully-resolved call. Saves ~250-byte memcpy + ~50 conditional
bit-ops + a hash lookup on every draw. Estimated benefit on OpenMW
at ~3000 draws/frame: **0.3–0.8 ms/frame** of CPU on ARM.

Touched files:
- `src/gl/glstate.h` — adds `fpe_input_cache` field
- `src/gl/glstate.c` — allocates it (two init sites)
- `src/gl/fpe.c` — fast-path check at top of `fpe_program`, snapshot
  of state at the bottom

## How to apply

    cp Android.mk /path/to/gl4es/Android.mk
    cp -r src/* /path/to/gl4es/src/

Then rebuild `libGL.so` and replace it in the APK.

## Verifying it worked

After rebuild, the new `libGL.so` will have a different BuildId.
You can check with:

    aarch64-linux-android-readelf -n /path/to/libGL.so | grep "Build ID"

Old (crashing) BuildId was `209dc0ebe98fa7931af050c487ad7c25c9a3e496`.
If the new BuildId differs, you have actually replaced the binary.

In logcat at game start you should see:

    LIBGL: GLES Version: 3.0
    Probed: half-float color FBO complete, enabling halffloatfbo

If `OPENMW_USER_FILE_STORAGE` is set, also:

    LIBGL: PSA path: /sdcard/.../.gl4es.psa-highp

If `OPENMW_USER_FILE_STORAGE` is NOT set:

    OPENMW_USER_FILE_STORAGE not set, PSA disabled

The game should now start, shadows should appear (with
`enable shadows = true` in OpenMW settings.cfg), and walking into
shallow water should produce visible ripples.
