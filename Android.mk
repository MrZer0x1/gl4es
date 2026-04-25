LOCAL_PATH := $(call my-dir)

###########################
#
# GL shared library
#
###########################

include $(CLEAR_VARS)

LOCAL_MODULE := GL

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES) -DBCMHOST

LOCAL_SRC_FILES := \
	src/gl/arbconverter.c \
	src/gl/arbgenerator.c \
	src/gl/arbhelper.c \
	src/gl/arbparser.c \
	src/gl/array.c \
	src/gl/blend.c \
	src/gl/blit.c \
	src/gl/buffers.c \
	src/gl/build_info.c \
	src/gl/debug.c \
	src/gl/decompress.c \
	src/gl/depth.c \
	src/gl/directstate.c \
	src/gl/drawing.c \
	src/gl/enable.c \
	src/gl/envvars.c \
	src/gl/eval.c \
	src/gl/face.c \
	src/gl/fog.c \
	src/gl/fpe.c \
	src/gl/fpe_cache.c \
	src/gl/fpe_shader.c \
	src/gl/framebuffers.c \
	src/gl/gl_lookup.c \
	src/gl/getter.c \
	src/gl/gl4es.c \
	src/gl/glstate.c \
	src/gl/hint.c \
	src/gl/init.c \
	src/gl/light.c \
	src/gl/line.c \
	src/gl/list.c \
	src/gl/listdraw.c \
	src/gl/listrl.c \
	src/gl/loader.c \
	src/gl/logs.c \
	src/gl/matrix.c \
	src/gl/matvec.c \
	src/gl/oldprogram.c \
	src/gl/pixel.c \
	src/gl/planes.c \
	src/gl/pointsprite.c \
	src/gl/preproc.c \
	src/gl/program.c \
	src/gl/queries.c \
	src/gl/raster.c \
	src/gl/render.c \
	src/gl/samplers.c \
	src/gl/shader.c \
	src/gl/shaderconv.c \
	src/gl/shader_hacks.c \
	src/gl/stack.c \
	src/gl/stencil.c \
	src/gl/string_utils.c \
	src/gl/stubs.c \
	src/gl/texenv.c \
	src/gl/texgen.c \
	src/gl/texture.c \
	src/gl/texture_compressed.c \
	src/gl/texture_params.c \
	src/gl/texture_read.c \
	src/gl/texture_3d.c \
	src/gl/uniform.c \
	src/gl/vertexattrib.c \
	src/gl/wrap/gl4eswraps.c \
	src/gl/wrap/gles.c \
	src/gl/wrap/glstub.c \
	src/gl/math/matheval.c \
	src/glx/hardext.c \
	src/glx/glx.c \
	src/glx/lookup.c \
	src/glx/gbm.c \
	src/glx/streaming.c \
	src/gl/libtxc_dxtn/txc_compress_dxtn.c \

LOCAL_CFLAGS += -g -std=gnu99 -funwind-tables -O3 -fvisibility=hidden -include include/android_debug.h
LOCAL_CFLAGS += -DNOX11
LOCAL_CFLAGS += -DNO_GBM
LOCAL_CFLAGS += -DUSE_ANDROID_LOG=1

# Do NOT define NO_INIT_CONSTRUCTOR on this fork.  In the upstream gl4es
# layout, libGL.so depends on initialize_gl4es() being called on dlopen
# via __attribute__((constructor)).  Without it, the global glstate stays
# NULL and the first glGetString() from OSG's GraphicsThread crashes
# with SIGSEGV at glGetString+28.  The original crash that motivated
# adding this flag was a NULL-pointer strcpy() inside initialize_gl4es()
# when OPENMW_USER_FILE_STORAGE was unset; that is now fixed at the
# source level by a NULL-guard in src/gl/init.c.

# Request a GLES3 context.  The shader emitter still targets GLSL ES 1.00
# (#version 100), but the GLES3 context unlocks GL_EXT_shadow_samplers
# and float depth textures that OpenMW's shadow cascade uses.
LOCAL_CFLAGS += -DDEFAULT_ES=3

# Suppress harmless warnings on this code base.
LOCAL_CFLAGS += -Wno-typedef-redefinition -Wno-dangling-else

LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)
