# gl4es-Openmw — applied patches / optimizations

This tree is a patched copy of `gl4es-Openmw` tuned for running OpenMW
(especially the shadow shader pipeline) on Android via GLES.

## What was already in the source snapshot

These upstream patches were already applied before I touched it:

- `disable-shader-hacks.patch` — `ShaderHacks()` is a no-op. gl4es's
  auto-rewrites broke OpenMW's `#extension` directives and
  `sampler2DShadow` in `shadows_fragment.glsl`.
- `highp-fog.patch` — forces `highp` for fog, MVP/MV matrices, and
  `gl_FogParameters`. This is the single biggest win for OpenMW's shadow
  quality: shadow projection math is done in world space and visibly
  bands/z-fights on `mediump`.
- `psa.patch` (partial) — `init.c` side: shader cache written to
  `$OPENMW_USER_FILE_STORAGE/.gl4es.psa-{highp,mediump}` instead of
  `$HOME/.gl4es.psa`, plus the `GL_OES_get_program_binary` typo fix was
  already there via a fallback `S()` line.
- `shared-library.patch` (partial) — already used
  `BUILD_SHARED_LIBRARY` instead of static.
- `gamma.patch` (old 1.4 form) — `state->gamma` field in `fpe_state_t`,
  `fpe_shader.c` pow() emit, and the `glLightModelfv(0x4242)` handler.

## What I changed in this pass

### 1. `src/gl/init.c` — unguard `LIBGL_GAMMA`

The `GetEnvVarFloat("LIBGL_GAMMA", ...)` block was wrapped in
`#ifdef PANDORA`, so the env var was silently ignored on Android.
Removed the `#ifdef` / `#endif`.

### 2. `src/gl/shaderconv.c` — gamma for custom (OpenMW) shaders

The in-tree gamma path only touches the fixed-function pipeline via
`fpe_FragmentShader`. OpenMW doesn't use FPE — it ships its own GLSL.
Added a late-stage rewrite that replaces `applyShadowDebugOverlay();`
with the same call plus `gl_FragData[0].xyz = pow(...)`. This is the
standard gamma hook point used by OpenMW's fragment shaders, so the
pow() lands after all lighting/tonemap but before fragment write.

Trigger: `LIBGL_GAMMA=1.8` (or whatever) in the OpenMW launcher env.

### 3. `src/gl/hint.c` — PSA write-on-demand

Added `if (pname == 41231) { fpe_writePSA(); return; }` at the top of
`gl4es_glHint`. OpenMW calls `glHint(41231, 0)` when it wants gl4es to
flush the compiled-shader cache to disk. Without this, the ~200+ OpenMW
shaders get recompiled every cold start. Also added
`#include "fpe_cache.h"` for the prototype.

### 4. `Android.mk` — shared-lib flags + GLES3 context

New `CFLAGS`:

- `-DNO_INIT_CONSTRUCTOR` — required for dlopen() from OpenMW's JNI glue
- `-DNOEGL` — Android uses its own EGL path, not gl4es's
- `-DNO_LOADER` — symbols are resolved via dlsym at Java side
- `-DDEFAULT_ES=3` — requests a GLES3 context from the driver. This
  unlocks `GL_EXT_shadow_samplers`, float depth textures, and the
  driver-side fast paths that OpenMW's shadow cascade relies on
- `-Dasm=__asm__ -Dvolatile=__volatile__` — Bionic strips plain `asm`
- `-include include/gl4esinit.h` — `gl4es_*` symbol prefixing

## Caveats

### GLES3 shader emission
`shaderconv.c` still hard-codes `#version 100` output (the higher-GLSL
block is wrapped in `#if 0` by upstream). `-DDEFAULT_ES=3` only gives you
a **GLES3 context**, not GLES3-source shaders. For OpenMW this is the
right tradeoff — the shaders target GLES2 + `GL_EXT_shadow_samplers`
anyway, and enabling full 300-es emission would break unrelated
translations.

### PSA cache location
OpenMW must set `OPENMW_USER_FILE_STORAGE` in its env before loading
libGL.so, otherwise the PSA path is empty and no cache is used. The
OpenMW-Android launcher already does this.

### First-run cost
On first run, the PSA cache doesn't exist yet and every shader compiles
cold. Expect a long loading screen. Subsequent runs are fast.

### If rendering goes wrong
- Back out `-DDEFAULT_ES=3` first (drop to `-DDEFAULT_ES=2`). Some
  drivers misreport GLES3 capability.
- Set `LIBGL_NOPSA=1` in env to rule out a corrupt cache.
- Set `LIBGL_GAMMA=0` to bypass the pow() path.

## Env vars reference

| Var                        | Effect                                       |
|----------------------------|----------------------------------------------|
| `LIBGL_GAMMA=1.8`          | Apply 1/1.8 gamma in shader                  |
| `LIBGL_NOPSA=1`            | Disable shader binary cache                  |
| `OPENMW_USER_FILE_STORAGE` | Dir for PSA cache (set by OpenMW launcher)   |
| `LIBGL_ES=2`               | Force GLES2 context instead of 3             |
