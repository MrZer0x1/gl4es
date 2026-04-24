// gl4es Android auto-init shim
//
// WHY THIS FILE EXISTS
// --------------------
// gl4es on Android is built with -DNO_INIT_CONSTRUCTOR -DNOEGL -DNO_LOADER
// (see Android.mk). That combo means:
//
//   1. initialize_gl4es() does NOT run at dlopen("libGL.so") time, so the
//      global `glstate` pointer stays NULL and `globals4es` stays zeroed.
//   2. gl4es has no built-in way to resolve GLES symbols — it needs the
//      host to call set_getprocaddress(&eglGetProcAddress) before any GL.
//   3. proc_address() falls through to `return NULL;` (loader.c:210) if
//      nobody wired in a getter.
//
// The OpenMW launcher never calls set_getprocaddress / initialize_gl4es,
// so the first time OSG's graphics thread calls glGetString(GL_EXTENSIONS)
// from osg::State::initializeExtensionProcs(), gl4es dereferences
// glstate == NULL and crashes in glGetString+28 at fault addr ~0x1828.
//
// THIS FILE
// ---------
// A library constructor that runs at libGL.so load and wires up the
// getProcAddress callback via dlsym(). Actual initialize_gl4es() is
// deferred to the first glGetString() call (see getter.c patch) because
// initialize_gl4es needs a *live current* GL context for the hardext
// probe (see hardext.c:134 "Hardware test on current Context") — and at
// .so-load time SDL has not yet created the EGL context.
//
// The hook in getter.c lazily calls initialize_gl4es() the first time
// anyone asks for GL_VERSION / GL_EXTENSIONS / GL_VENDOR / etc, which is
// guaranteed to happen from a thread where EGL is current (because OSG
// does it as its very first GL call on the graphics thread).

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

#include "init.h"
#include "../../include/gl4esinit.h"
#include "logs.h"

// Flag observable from getter.c's glGetString wrapper.
// Volatile because it's read/written from multiple threads (SDLMain and
// OSG graphics thread).
volatile int gl4es_android_inited = 0;

// Fallback FB size getter. gl4es calls this for glXQueryDrawable-style
// queries when NOEGL is defined. OpenMW never relies on these values for
// anything load-bearing (it uses SDL_GL_GetDrawableSize directly), so
// returning a sane default is fine — the values are only used by a few
// legacy-GL paths that OpenMW does not exercise.
static void gl4es_android_get_fb_size(int *w, int *h) {
    if (w) *w = 1280;
    if (h) *h = 720;
}

// dlsym-based getProcAddress. With NOEGL set, gl4es can't link against
// libEGL directly, so we resolve eglGetProcAddress at runtime.
static void *(*s_eglGetProcAddress)(const char *name) = NULL;

static void *gl4es_android_proc_address(const char *name) {
    if (!s_eglGetProcAddress) return NULL;
    return s_eglGetProcAddress(name);
}

// Runs at dlopen("libGL.so") time. This one constructor is NOT blocked
// by NO_INIT_CONSTRUCTOR — that macro only guards initialize_gl4es()
// and close_gl4es() in init.c. Our constructor is an independent symbol
// in a separate file, so it always runs.
__attribute__((constructor))
static void gl4es_android_bootstrap(void) {
    // Resolve eglGetProcAddress from the already-loaded libEGL.so.
    // SDL2 links libEGL, so by the time libGL (gl4es) is dlopened by the
    // Java launcher, libEGL is already in the process. RTLD_DEFAULT
    // searches the full process namespace.
    void *sym = dlsym(RTLD_DEFAULT, "eglGetProcAddress");
    if (!sym) {
        // Second try: open libEGL explicitly. On a few Android builds
        // dlsym(RTLD_DEFAULT, ...) won't see libEGL symbols if it was
        // dlopened with RTLD_LOCAL by the framework.
        void *lib = dlopen("libEGL.so", RTLD_NOW | RTLD_GLOBAL);
        if (lib) sym = dlsym(lib, "eglGetProcAddress");
    }

    if (!sym) {
        SHUT_LOGE("gl4es-android: cannot resolve eglGetProcAddress — "
                  "GL calls will fail\n");
        return;
    }

    s_eglGetProcAddress = (void *(*)(const char *))sym;

    // Wire up gl4es. Neither of these touches the GL context, they just
    // stash callback pointers in init.c file-static state, so it's safe
    // to do at .so-load time.
    set_getprocaddress(gl4es_android_proc_address);
    set_getmainfbsize(gl4es_android_get_fb_size);

    SHUT_LOGD("gl4es-android: getProcAddress wired, deferring "
              "initialize_gl4es() until first glGetString()\n");
}

// Called lazily from gl4es_glGetString the first time anyone asks for
// a string. By then SDL has made the EGL context current on the calling
// thread, so GetHardwareExtensions()'s "current context" probe works.
void gl4es_android_ensure_inited(void) {
    // Fast path — common case after first init.
    if (gl4es_android_inited) return;

    // Extra safety: if the bootstrap constructor somehow didn't run or
    // didn't find libEGL, don't proceed — initialize_gl4es would call
    // through a NULL function pointer in GetHardwareExtensions.
    if (!s_eglGetProcAddress) {
        SHUT_LOGE("gl4es-android: ensure_inited called but "
                  "eglGetProcAddress still NULL; skipping\n");
        return;
    }

    // initialize_gl4es has its own `if (inited++) return;` idempotency
    // (init.c:93), so racing threads that both enter here are fine —
    // the second one is a no-op inside gl4es. The only extra work we
    // do is set our own flag below.
    initialize_gl4es();
    gl4es_android_inited = 1;
}
