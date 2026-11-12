/*
===========================================================================
Copyright (C) 2026 the OpenMOHAA Plus contributors

This file is part of OpenMOHAA Plus source code.

OpenMOHAA Plus source code is free software: you can redistribute it
and/or modify it under the terms of the GNU Affero General Public License
as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

OpenMOHAA Plus source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License
along with OpenMOHAA Plus source code.  If not, see <https://www.gnu.org/licenses/>.
===========================================================================
*/

// vk_gl.c: Added in OpenMOHAA Plus
//  The OpenGL 1.x calls the GL1 renderer makes (the qgl* pointers), emulated on Vulkan: GL state
//  is tracked here and every draw becomes a Vulkan draw with a pipeline for its blend, depth,
//  cull and stencil state and a per-draw uniform block for the rest (matrices, texture
//  environments, alpha test, fog, clip plane), interpreted by shaders/ffp.frag.
//  Only what code/renderergl1 uses is emulated; unused entry points stay NULL.

#include "vk_local.h"
#include "../renderercommon/tr_common.h"
#include "../renderergl1/qgl.h"

// the qgl* pointers (defined in code/sdl/sdl_glimp.c)
#define GLE(ret, name, ...) extern name##proc *qgl##name;
QGL_1_1_PROCS;
QGL_1_1_FIXED_FUNCTION_PROCS;
QGL_DESKTOP_1_1_PROCS;
QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS;
QGL_1_3_PROCS;
#undef GLE

#ifndef GL_COMBINE4_NV
#    define GL_COMBINE4_NV 0x8503
#endif

#define MAX_UNITS      2
#define MATRIX_DEPTH   32
#define MAX_IMMEDIATE  16384
#define MAX_BATCH      (65536 * 3)

typedef struct {
    const byte *ptr;
    int         size;
    GLenum      type;
    int         stride;
    qboolean    enabled;
} glArray_t;

typedef struct {
    GLenum mode, combineRgb, combineAlpha;
    GLenum srcRgb[4], opRgb[4], srcAlpha[4], opAlpha[4];
} glEnv_t;

typedef enum {
    FMT_RGBA,
    FMT_RGB,
    FMT_LUMINANCE,
    FMT_LUMINANCE_ALPHA,
    FMT_ALPHA,
    FMT_INTENSITY
} glInternal_t;

typedef struct {
    vkTexture_t *tex;
    glInternal_t internal;
    GLenum       minFilter, magFilter, wrapS, wrapT;
    float        anisotropy;
} glTexture_t;

static struct {
    float    matrix[2][MATRIX_DEPTH][16]; // 0 modelview, 1 projection
    int      depth[2];
    int      matrixMode;

    qboolean blend, alphaTest, depthTest, cull, polygonOffset, fog, stencilTest, scissorTest, clipPlane0;
    qboolean texture2D[MAX_UNITS];
    GLenum   blendSrc, blendDst, depthFunc, alphaFunc, cullFace, polygonMode;
    float    alphaRef;
    qboolean depthMask;
    uint8_t  colorMask;
    float    offsetFactor, offsetUnits, lineWidth;
    GLenum   fogMode;
    float    fogStart, fogEnd, fogDensity, fogColor[4];
    float    clipPlane[4]; // eye space
    GLenum   stencilFunc, stencilFail, stencilZFail, stencilZPass;
    int      stencilRef;
    GLuint   stencilMask, stencilWriteMask;
    int      viewport[4], scissor[4];
    float    depthNear, depthFar;
    float    clearColor[4], clearDepth;
    int      clearStencil;

    int      activeUnit, clientUnit;
    GLuint   bound[MAX_UNITS];
    glEnv_t  env[MAX_UNITS];

    glArray_t vertex, color, texcoord[MAX_UNITS];
    float     currentColor[4];
    float     currentTexcoord[MAX_UNITS][2];
    int       packAlignment, unpackAlignment;

    GLenum    beginMode;
    qboolean  inBegin;
    int       numImmediate;

    GLuint    nextName;
} gl;

static glTexture_t *textures;
static int          numTextureSlots;

static vkVertex_t   immediate[MAX_IMMEDIATE];
static vkVertex_t   batchVertices[MAX_BATCH];
static uint32_t     batchIndices[MAX_BATCH * 2];
static byte        *uploadBuffer;
static int          uploadBufferSize;

/*
==============================================================================

Helpers

==============================================================================
*/

static void Warn(const char *what)
{
    static char warned[1024];

    if (!strstr(warned, what) && strlen(warned) + strlen(what) + 2 < sizeof(warned)) {
        Q_strcat(warned, sizeof(warned), what);
        Q_strcat(warned, sizeof(warned), ";");
        ri.Printf(PRINT_DEVELOPER, "Vulkan: %s is not emulated\n", what);
    }
}

static void MatrixMultiply4(const float *a, const float *b, float *out)
{
    // column-major: out = a * b
    int c, r;
    float tmp[16];

    for (c = 0; c < 4; c++) {
        for (r = 0; r < 4; r++) {
            tmp[c * 4 + r] = a[0 * 4 + r] * b[c * 4 + 0] + a[1 * 4 + r] * b[c * 4 + 1] + a[2 * 4 + r] * b[c * 4 + 2]
                           + a[3 * 4 + r] * b[c * 4 + 3];
        }
    }
    Com_Memcpy(out, tmp, sizeof(tmp));
}

static qboolean MatrixInvert4(const float *m, float *out)
{
    float inv[16], det;
    int   i;

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14]
           + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14]
           - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13]
           + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13]
            - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14]
           - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14]
           + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13]
           - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13]
            + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14]
           + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14]
           - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13]
            + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13]
            - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10]
           - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10]
           + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9]
            - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9]
            + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (det == 0) {
        return qfalse;
    }
    det = 1.0f / det;
    for (i = 0; i < 16; i++) {
        out[i] = inv[i] * det;
    }
    return qtrue;
}

static float *CurrentMatrix(void)
{
    return gl.matrix[gl.matrixMode][gl.depth[gl.matrixMode]];
}

static void Identity(float *m)
{
    Com_Memset(m, 0, sizeof(float) * 16);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static glTexture_t *GetTexture(GLuint name, qboolean create)
{
    if ((int)name >= numTextureSlots) {
        int          slots;
        glTexture_t *grown;

        if (!create) {
            return NULL;
        }
        slots = Q_max(4096, (int)name * 2);
        grown = ri.Malloc(slots * sizeof(*grown));
        Com_Memset(grown, 0, slots * sizeof(*grown));
        if (textures) {
            Com_Memcpy(grown, textures, numTextureSlots * sizeof(*grown));
            ri.Free(textures);
        }
        textures        = grown;
        numTextureSlots = slots;
    }
    if (create && !textures[name].minFilter) {
        // GL defaults
        textures[name].minFilter = GL_NEAREST_MIPMAP_LINEAR;
        textures[name].magFilter = GL_LINEAR;
        textures[name].wrapS     = GL_REPEAT;
        textures[name].wrapT     = GL_REPEAT;
    }
    return &textures[name];
}

static byte *UploadBuffer(int size)
{
    if (size > uploadBufferSize) {
        if (uploadBuffer) {
            ri.Free(uploadBuffer);
        }
        uploadBufferSize = size;
        uploadBuffer     = ri.Malloc(size);
    }
    return uploadBuffer;
}

static glInternal_t InternalFormat(GLint internal)
{
    switch (internal) {
    case 1:
    case GL_LUMINANCE:
    case GL_LUMINANCE4:
    case GL_LUMINANCE8:
    case GL_LUMINANCE12:
    case GL_LUMINANCE16:
        return FMT_LUMINANCE;
    case 2:
    case GL_LUMINANCE_ALPHA:
    case GL_LUMINANCE4_ALPHA4:
    case GL_LUMINANCE6_ALPHA2:
    case GL_LUMINANCE8_ALPHA8:
    case GL_LUMINANCE12_ALPHA4:
    case GL_LUMINANCE12_ALPHA12:
    case GL_LUMINANCE16_ALPHA16:
        return FMT_LUMINANCE_ALPHA;
    case GL_ALPHA:
    case GL_ALPHA4:
    case GL_ALPHA8:
    case GL_ALPHA12:
    case GL_ALPHA16:
        return FMT_ALPHA;
    case GL_INTENSITY:
    case GL_INTENSITY4:
    case GL_INTENSITY8:
    case GL_INTENSITY12:
    case GL_INTENSITY16:
        return FMT_INTENSITY;
    case 3:
    case GL_RGB:
    case GL_R3_G3_B2:
    case GL_RGB4:
    case GL_RGB5:
    case GL_RGB8:
    case GL_RGB10:
    case GL_RGB12:
    case GL_RGB16:
    case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        return FMT_RGB;
    }
    return FMT_RGBA;
}

// Convert client pixels to RGBA as the texture's internal format samples them
static const byte *ConvertPixels(int width, int height, GLenum format, GLenum type, const void *pixels, glInternal_t internal)
{
    int         components, rowBytes, x, y;
    byte       *out = UploadBuffer(width * height * 4);
    const byte *in  = pixels;

    if (!pixels) {
        Com_Memset(out, 0, width * height * 4);
        return out;
    }
    if (type != GL_UNSIGNED_BYTE) {
        Warn("non-byte pixel types");
        Com_Memset(out, 255, width * height * 4);
        return out;
    }
    switch (format) {
    case GL_RGBA:
    case GL_BGRA:
        components = 4;
        break;
    case GL_RGB:
    case GL_BGR:
        components = 3;
        break;
    case GL_LUMINANCE_ALPHA:
        components = 2;
        break;
    default:
        components = 1;
        break;
    }
    rowBytes = width * components;
    rowBytes = (rowBytes + gl.unpackAlignment - 1) & ~(gl.unpackAlignment - 1);

    for (y = 0; y < height; y++) {
        const byte *row = in + y * rowBytes;
        byte       *dst = out + y * width * 4;
        for (x = 0; x < width; x++, dst += 4) {
            const byte *p = row + x * components;
            byte        r, g, b, a;
            switch (format) {
            case GL_RGBA:
                r = p[0], g = p[1], b = p[2], a = p[3];
                break;
            case GL_BGRA:
                r = p[2], g = p[1], b = p[0], a = p[3];
                break;
            case GL_RGB:
                r = p[0], g = p[1], b = p[2], a = 255;
                break;
            case GL_BGR:
                r = p[2], g = p[1], b = p[0], a = 255;
                break;
            case GL_LUMINANCE_ALPHA:
                r = g = b = p[0], a = p[1];
                break;
            case GL_ALPHA:
                r = g = b = 0, a = p[0];
                break;
            default:
                r = g = b = p[0], a = 255;
                break;
            }
            switch (internal) {
            case FMT_RGB:
                a = 255;
                break;
            case FMT_LUMINANCE:
                g = b = r, a = 255;
                break;
            case FMT_LUMINANCE_ALPHA:
                g = b = r;
                break;
            case FMT_ALPHA:
                r = g = b = 0;
                break;
            case FMT_INTENSITY:
                g = b = a = r;
                break;
            default:
                break;
            }
            dst[0] = r, dst[1] = g, dst[2] = b, dst[3] = a;
        }
    }
    return out;
}

/*
==============================================================================

Draws

==============================================================================
*/

static VkFilter Filter(GLenum f)
{
    return f == GL_NEAREST || f == GL_NEAREST_MIPMAP_NEAREST || f == GL_NEAREST_MIPMAP_LINEAR ? VK_FILTER_NEAREST
                                                                                                : VK_FILTER_LINEAR;
}

static VkSamplerAddressMode Wrap(GLenum w)
{
    return w == GL_REPEAT ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
}

static VkSampler SamplerFor(const glTexture_t *t)
{
    static const glTexture_t defaults = {NULL, FMT_RGBA, GL_LINEAR, GL_LINEAR, GL_REPEAT, GL_REPEAT, 0};
    qboolean                 mipmaps;
    VkSamplerMipmapMode      mipmapMode;

    if (!t || !t->tex) {
        t = &defaults;
    }
    mipmaps    = t->minFilter != GL_NEAREST && t->minFilter != GL_LINEAR;
    mipmapMode = t->minFilter == GL_NEAREST_MIPMAP_LINEAR || t->minFilter == GL_LINEAR_MIPMAP_LINEAR
                   ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                   : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    return VK_GetSampler(Filter(t->minFilter), Filter(t->magFilter), mipmapMode, mipmaps, Wrap(t->wrapS), Wrap(t->wrapT), t->anisotropy);
}

// topology: 0 triangles, 1 lines, 2 points
static void Draw(int topology, const vkVertex_t *vertices, int numVertices, const uint32_t *indices, int numIndices)
{
    vkPipelineKey_t key;
    vkDrawState_t   state;
    vkViewport_t    vp;
    VkRect2D        scissor;
    int             fbWidth = VK_FramebufferWidth(), fbHeight = VK_FramebufferHeight();
    glTexture_t    *tex[MAX_UNITS];
    int             u;

    if (topology == 0 && gl.cull && gl.cullFace == GL_FRONT_AND_BACK) {
        return;
    }

    Com_Memset(&key, 0, sizeof(key));
    if (gl.blend) {
        key.blendSrc = gl.blendSrc;
        key.blendDst = gl.blendDst;
    }
    key.depthFunc   = gl.depthTest ? gl.depthFunc : 0;
    key.depthWrite  = gl.depthMask;
    key.cullMode    = gl.cull && topology == 0 ? (gl.cullFace == GL_FRONT ? 1 : 2) : 0;
    key.depthBias   = gl.polygonOffset && topology == 0;
    key.colorMask   = gl.colorMask;
    key.topology    = topology;
    key.polygonLine = topology == 0 && gl.polygonMode == GL_LINE;
    if (gl.stencilTest) {
        key.stencilTest  = 1;
        key.stencilFunc  = gl.stencilFunc;
        key.stencilFail  = gl.stencilFail;
        key.stencilZFail = gl.stencilZFail;
        key.stencilZPass = gl.stencilZPass;
    }

    Com_Memset(&state, 0, sizeof(state));
    MatrixMultiply4(gl.matrix[1][gl.depth[1]], gl.matrix[0][gl.depth[0]], state.mvp);
    Com_Memcpy(state.modelView, gl.matrix[0][gl.depth[0]], sizeof(state.modelView));
    Com_Memcpy(state.clipPlane, gl.clipPlane, sizeof(state.clipPlane));
    Com_Memcpy(state.fogColor, gl.fogColor, sizeof(state.fogColor));
    state.fogParams[0]   = gl.fogStart;
    state.fogParams[1]   = gl.fogEnd;
    state.fogParams[2]   = gl.fogDensity;
    state.alphaParams[0] = gl.alphaRef;
    state.misc[0]        = gl.alphaTest ? (int)gl.alphaFunc : 0;
    state.misc[1]        = gl.fog ? (int)gl.fogMode : 0;
    state.misc[2]        = gl.clipPlane0;
    for (u = 0; u < MAX_UNITS; u++) {
        const glEnv_t *e = &gl.env[u];
        int            i;

        tex[u]               = gl.texture2D[u] ? GetTexture(gl.bound[u], qfalse) : NULL;
        state.env[u * 5][0]  = gl.texture2D[u];
        state.env[u * 5][1]  = e->mode;
        state.env[u * 5][2]  = e->combineRgb;
        state.env[u * 5][3]  = e->combineAlpha;
        for (i = 0; i < 4; i++) {
            state.env[u * 5 + 1][i] = e->srcRgb[i];
            state.env[u * 5 + 2][i] = e->opRgb[i];
            state.env[u * 5 + 3][i] = e->srcAlpha[i];
            state.env[u * 5 + 4][i] = e->opAlpha[i];
        }
    }

    // GL windows have their origin at the bottom left
    vp.x        = gl.viewport[0];
    vp.y        = fbHeight - (gl.viewport[1] + gl.viewport[3]);
    vp.w        = gl.viewport[2];
    vp.h        = gl.viewport[3];
    vp.minDepth = gl.depthNear;
    vp.maxDepth = gl.depthFar;
    if (vp.w <= 0 || vp.h <= 0) {
        return;
    }

    if (gl.scissorTest) {
        int x0 = Q_max(0, gl.scissor[0]), x1 = Q_min(fbWidth, gl.scissor[0] + gl.scissor[2]);
        int y1 = Q_min(fbHeight, fbHeight - gl.scissor[1]), y0 = Q_max(0, fbHeight - (gl.scissor[1] + gl.scissor[3]));
        if (x1 <= x0 || y1 <= y0) {
            return;
        }
        scissor.offset.x      = x0;
        scissor.offset.y      = y0;
        scissor.extent.width  = x1 - x0;
        scissor.extent.height = y1 - y0;
    } else {
        scissor.offset.x      = 0;
        scissor.offset.y      = 0;
        scissor.extent.width  = fbWidth;
        scissor.extent.height = fbHeight;
    }

    VK_Draw(
        &key,
        &state,
        &vp,
        &scissor,
        gl.offsetUnits,
        gl.offsetFactor,
        gl.lineWidth,
        gl.stencilRef,
        gl.stencilMask,
        gl.stencilWriteMask,
        tex[0] ? tex[0]->tex : NULL,
        SamplerFor(tex[0]),
        tex[1] ? tex[1]->tex : NULL,
        SamplerFor(tex[1]),
        vertices,
        numVertices,
        indices,
        numIndices
    );
}

// Turn a GL primitive over the element list [0, count) into a list for 'topology'
static int Assemble(GLenum mode, int count, uint32_t *out, int *topology)
{
    int n = 0, i;

    switch (mode) {
    case GL_POINTS:
        *topology = 2;
        for (i = 0; i < count; i++) {
            out[n++] = i;
        }
        break;
    case GL_LINES:
        *topology = 1;
        for (i = 0; i + 1 < count; i += 2) {
            out[n++] = i;
            out[n++] = i + 1;
        }
        break;
    case GL_LINE_STRIP:
    case GL_LINE_LOOP:
        *topology = 1;
        for (i = 1; i < count; i++) {
            out[n++] = i - 1;
            out[n++] = i;
        }
        if (mode == GL_LINE_LOOP && count > 2) {
            out[n++] = count - 1;
            out[n++] = 0;
        }
        break;
    case GL_TRIANGLES:
        *topology = 0;
        for (i = 0; i + 2 < count; i += 3) {
            out[n++] = i;
            out[n++] = i + 1;
            out[n++] = i + 2;
        }
        break;
    case GL_TRIANGLE_STRIP:
        *topology = 0;
        for (i = 2; i < count; i++) {
            if (i & 1) {
                out[n++] = i - 1;
                out[n++] = i - 2;
            } else {
                out[n++] = i - 2;
                out[n++] = i - 1;
            }
            out[n++] = i;
        }
        break;
    case GL_TRIANGLE_FAN:
    case GL_POLYGON:
        *topology = 0;
        for (i = 2; i < count; i++) {
            out[n++] = 0;
            out[n++] = i - 1;
            out[n++] = i;
        }
        break;
    case GL_QUADS:
        *topology = 0;
        for (i = 0; i + 3 < count; i += 4) {
            out[n++] = i;
            out[n++] = i + 1;
            out[n++] = i + 2;
            out[n++] = i;
            out[n++] = i + 2;
            out[n++] = i + 3;
        }
        break;
    case GL_QUAD_STRIP:
        *topology = 0;
        for (i = 0; i + 3 < count; i += 2) {
            out[n++] = i;
            out[n++] = i + 1;
            out[n++] = i + 3;
            out[n++] = i;
            out[n++] = i + 3;
            out[n++] = i + 2;
        }
        break;
    default:
        *topology = 0;
        break;
    }
    return n;
}

static float ArrayFloat(const glArray_t *a, int index, int component)
{
    const byte *p = a->ptr + index * (a->stride ? a->stride : a->size * (a->type == GL_UNSIGNED_BYTE ? 1 : 4));
    if (a->type == GL_UNSIGNED_BYTE) {
        return p[component] / 255.0f;
    }
    return ((const float *)p)[component];
}

static void FetchVertex(int index, vkVertex_t *v)
{
    int u;

    v->xyz[0] = ArrayFloat(&gl.vertex, index, 0);
    v->xyz[1] = ArrayFloat(&gl.vertex, index, 1);
    v->xyz[2] = gl.vertex.size > 2 ? ArrayFloat(&gl.vertex, index, 2) : 0.0f;

    if (gl.color.enabled && gl.color.ptr) {
        if (gl.color.type == GL_UNSIGNED_BYTE) {
            const byte *p = gl.color.ptr + index * (gl.color.stride ? gl.color.stride : gl.color.size);
            v->color[0] = p[0];
            v->color[1] = p[1];
            v->color[2] = p[2];
            v->color[3] = gl.color.size > 3 ? p[3] : 255;
        } else {
            int c;
            for (c = 0; c < 4; c++) {
                float f     = c < gl.color.size ? ArrayFloat(&gl.color, index, c) : 1.0f;
                v->color[c] = (byte)(Com_Clamp(0, 1, f) * 255.0f + 0.5f);
            }
        }
    } else {
        int c;
        for (c = 0; c < 4; c++) {
            v->color[c] = (byte)(Com_Clamp(0, 1, gl.currentColor[c]) * 255.0f + 0.5f);
        }
    }

    for (u = 0; u < MAX_UNITS; u++) {
        float *st = u ? v->st1 : v->st0;
        if (gl.texcoord[u].enabled && gl.texcoord[u].ptr) {
            st[0] = ArrayFloat(&gl.texcoord[u], index, 0);
            st[1] = ArrayFloat(&gl.texcoord[u], index, 1);
        } else {
            st[0] = gl.currentTexcoord[u][0];
            st[1] = gl.currentTexcoord[u][1];
        }
    }
}

static void DrawFromArrays(GLenum mode, int count, const uint32_t *elements)
{
    static uint32_t assembled[MAX_BATCH * 2];
    uint32_t        minIndex = 0xFFFFFFFF, maxIndex = 0;
    int             numIndices, numVertices, topology, i;

    if (!gl.vertex.enabled || !gl.vertex.ptr || count <= 0) {
        return;
    }
    if (count > MAX_BATCH) {
        Warn("draws over 196608 elements");
        return;
    }
    for (i = 0; i < count; i++) {
        minIndex = Q_min(minIndex, elements[i]);
        maxIndex = Q_max(maxIndex, elements[i]);
    }
    numVertices = maxIndex - minIndex + 1;
    if (numVertices > MAX_BATCH) {
        Warn("draws over 196608 vertices");
        return;
    }
    for (i = 0; i < numVertices; i++) {
        FetchVertex(minIndex + i, &batchVertices[i]);
    }
    numIndices = Assemble(mode, count, assembled, &topology);
    for (i = 0; i < numIndices; i++) {
        batchIndices[i] = elements[assembled[i]] - minIndex;
    }
    Draw(topology, batchVertices, numVertices, batchIndices, numIndices);
}

/*
==============================================================================

GL entry points

==============================================================================
*/

static void APIENTRY vkglDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
    static uint32_t elements[MAX_BATCH];
    int             i;

    if (count > MAX_BATCH) {
        Warn("draws over 196608 elements");
        return;
    }
    for (i = 0; i < count; i++) {
        switch (type) {
        case GL_UNSIGNED_BYTE:
            elements[i] = ((const byte *)indices)[i];
            break;
        case GL_UNSIGNED_SHORT:
            elements[i] = ((const unsigned short *)indices)[i];
            break;
        default:
            elements[i] = ((const uint32_t *)indices)[i];
            break;
        }
    }
    DrawFromArrays(mode, count, elements);
}

static void APIENTRY vkglDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    static uint32_t elements[MAX_BATCH];
    int             i;

    if (count > MAX_BATCH) {
        Warn("draws over 196608 elements");
        return;
    }
    for (i = 0; i < count; i++) {
        elements[i] = first + i;
    }
    DrawFromArrays(mode, count, elements);
}

static void APIENTRY vkglBegin(GLenum mode)
{
    gl.beginMode    = mode;
    gl.inBegin      = qtrue;
    gl.numImmediate = 0;
}

static void APIENTRY vkglEnd(void)
{
    int numIndices, topology;

    gl.inBegin = qfalse;
    numIndices = Assemble(gl.beginMode, gl.numImmediate, batchIndices, &topology);
    Draw(topology, immediate, gl.numImmediate, batchIndices, numIndices);
    gl.numImmediate = 0;
}

static void EmitVertex(float x, float y, float z)
{
    vkVertex_t *v;
    int         c;

    if (!gl.inBegin || gl.numImmediate == MAX_IMMEDIATE) {
        return;
    }
    v         = &immediate[gl.numImmediate++];
    v->xyz[0] = x;
    v->xyz[1] = y;
    v->xyz[2] = z;
    for (c = 0; c < 4; c++) {
        v->color[c] = (byte)(Com_Clamp(0, 1, gl.currentColor[c]) * 255.0f + 0.5f);
    }
    v->st0[0] = gl.currentTexcoord[0][0];
    v->st0[1] = gl.currentTexcoord[0][1];
    v->st1[0] = gl.currentTexcoord[1][0];
    v->st1[1] = gl.currentTexcoord[1][1];
}

static void APIENTRY vkglVertex2f(GLfloat x, GLfloat y)
{
    EmitVertex(x, y, 0);
}

static void APIENTRY vkglVertex3f(GLfloat x, GLfloat y, GLfloat z)
{
    EmitVertex(x, y, z);
}

static void APIENTRY vkglVertex3fv(const GLfloat *v)
{
    EmitVertex(v[0], v[1], v[2]);
}

static void APIENTRY vkglArrayElement(GLint i)
{
    if (gl.inBegin && gl.numImmediate < MAX_IMMEDIATE && gl.vertex.ptr) {
        FetchVertex(i, &immediate[gl.numImmediate++]);
    }
}

static void APIENTRY vkglColor3f(GLfloat r, GLfloat g, GLfloat b)
{
    gl.currentColor[0] = r;
    gl.currentColor[1] = g;
    gl.currentColor[2] = b;
    gl.currentColor[3] = 1.0f;
}

static void APIENTRY vkglColor4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    gl.currentColor[0] = r;
    gl.currentColor[1] = g;
    gl.currentColor[2] = b;
    gl.currentColor[3] = a;
}

static void APIENTRY vkglColor4ubv(const GLubyte *v)
{
    vkglColor4f(v[0] / 255.0f, v[1] / 255.0f, v[2] / 255.0f, v[3] / 255.0f);
}

static void APIENTRY vkglTexCoord2f(GLfloat s, GLfloat t)
{
    gl.currentTexcoord[0][0] = s;
    gl.currentTexcoord[0][1] = t;
}

static void APIENTRY vkglTexCoord2fv(const GLfloat *v)
{
    vkglTexCoord2f(v[0], v[1]);
}

static void APIENTRY vkglMultiTexCoord2fARB(GLenum unit, GLfloat s, GLfloat t)
{
    int u = Com_Clamp(0, MAX_UNITS - 1, (int)unit - GL_TEXTURE0_ARB);
    gl.currentTexcoord[u][0] = s;
    gl.currentTexcoord[u][1] = t;
}

static void SetCap(GLenum cap, qboolean on)
{
    switch (cap) {
    case GL_BLEND:
        gl.blend = on;
        break;
    case GL_ALPHA_TEST:
        gl.alphaTest = on;
        break;
    case GL_DEPTH_TEST:
        gl.depthTest = on;
        break;
    case GL_CULL_FACE:
        gl.cull = on;
        break;
    case GL_POLYGON_OFFSET_FILL:
        gl.polygonOffset = on;
        break;
    case GL_FOG:
        gl.fog = on;
        break;
    case GL_STENCIL_TEST:
        gl.stencilTest = on;
        break;
    case GL_SCISSOR_TEST:
        gl.scissorTest = on;
        break;
    case GL_CLIP_PLANE0:
        gl.clipPlane0 = on;
        break;
    case GL_TEXTURE_2D:
        gl.texture2D[gl.activeUnit] = on;
        break;
    default:
        break; // GL_LINE_STIPPLE, GL_DITHER...: no visible effect worth emulating
    }
}

static void APIENTRY vkglEnable(GLenum cap)
{
    SetCap(cap, qtrue);
}

static void APIENTRY vkglDisable(GLenum cap)
{
    SetCap(cap, qfalse);
}

static void SetClientState(GLenum array, qboolean on)
{
    switch (array) {
    case GL_VERTEX_ARRAY:
        gl.vertex.enabled = on;
        break;
    case GL_COLOR_ARRAY:
        gl.color.enabled = on;
        break;
    case GL_TEXTURE_COORD_ARRAY:
        gl.texcoord[gl.clientUnit].enabled = on;
        break;
    default:
        break;
    }
}

static void APIENTRY vkglEnableClientState(GLenum array)
{
    SetClientState(array, qtrue);
}

static void APIENTRY vkglDisableClientState(GLenum array)
{
    SetClientState(array, qfalse);
}

static void SetArray(glArray_t *a, GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
    a->size   = size;
    a->type   = type;
    a->stride = stride;
    a->ptr    = ptr;
}

static void APIENTRY vkglVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
    SetArray(&gl.vertex, size, type, stride, ptr);
}

static void APIENTRY vkglColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
    SetArray(&gl.color, size, type, stride, ptr);
}

static void APIENTRY vkglTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
    SetArray(&gl.texcoord[gl.clientUnit], size, type, stride, ptr);
}

static void APIENTRY vkglLockArraysEXT(GLint first, GLsizei count) {}

static void APIENTRY vkglUnlockArraysEXT(void) {}

static void APIENTRY vkglActiveTextureARB(GLenum unit)
{
    gl.activeUnit = Com_Clamp(0, MAX_UNITS - 1, (int)unit - GL_TEXTURE0_ARB);
}

static void APIENTRY vkglClientActiveTextureARB(GLenum unit)
{
    gl.clientUnit = Com_Clamp(0, MAX_UNITS - 1, (int)unit - GL_TEXTURE0_ARB);
}

static void APIENTRY vkglBindTexture(GLenum target, GLuint name)
{
    gl.bound[gl.activeUnit] = name;
    GetTexture(name, qtrue);
}

static void APIENTRY vkglGenTextures(GLsizei n, GLuint *names)
{
    int i;
    for (i = 0; i < n; i++) {
        names[i] = gl.nextName++;
    }
}

static void APIENTRY vkglDeleteTextures(GLsizei n, const GLuint *names)
{
    int i;
    for (i = 0; i < n; i++) {
        glTexture_t *t = GetTexture(names[i], qfalse);
        if (t) {
            VK_DestroyTexture(t->tex);
            Com_Memset(t, 0, sizeof(*t));
        }
    }
}

static int FullMipChain(int width, int height)
{
    int levels = 1, size = Q_max(width, height);
    while (size > 1) {
        size >>= 1;
        levels++;
    }
    return levels;
}

static glTexture_t *TextureForLevel(GLint level, GLint internal, GLsizei width, GLsizei height)
{
    glTexture_t *t = GetTexture(gl.bound[gl.activeUnit], qtrue);

    if (level == 0) {
        t->internal = InternalFormat(internal);
        if (!t->tex || t->tex->width != width || t->tex->height != height) {
            VK_DestroyTexture(t->tex);
            t->tex = VK_CreateTexture(width, height, FullMipChain(width, height));
        }
    }
    return t->tex ? t : NULL;
}

static void APIENTRY vkglTexImage2D(
    GLenum target, GLint level, GLint internal, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type,
    const GLvoid *pixels
)
{
    glTexture_t *t;

    if (width <= 0 || height <= 0) {
        return;
    }
    t = TextureForLevel(level, internal, width, height);
    if (t) {
        VK_UploadTexture(t->tex, level, 0, 0, width, height, ConvertPixels(width, height, format, type, pixels, t->internal));
    }
}

static void APIENTRY vkglTexSubImage2D(
    GLenum target, GLint level, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type,
    const GLvoid *pixels
)
{
    glTexture_t *t = GetTexture(gl.bound[gl.activeUnit], qfalse);

    if (t && t->tex && width > 0 && height > 0) {
        VK_UploadTexture(t->tex, level, x, y, width, height, ConvertPixels(width, height, format, type, pixels, t->internal));
    }
}

// S3TC (DXT1/3/5) blocks decoded to RGBA on the CPU
static void DecodeColors(const byte *block, byte colors[4][4], qboolean dxt1)
{
    unsigned c0 = block[0] | (block[1] << 8), c1 = block[2] | (block[3] << 8);
    int      i;

    colors[0][0] = ((c0 >> 11) & 31) * 255 / 31;
    colors[0][1] = ((c0 >> 5) & 63) * 255 / 63;
    colors[0][2] = (c0 & 31) * 255 / 31;
    colors[1][0] = ((c1 >> 11) & 31) * 255 / 31;
    colors[1][1] = ((c1 >> 5) & 63) * 255 / 63;
    colors[1][2] = (c1 & 31) * 255 / 31;
    colors[0][3] = colors[1][3] = colors[2][3] = colors[3][3] = 255;
    for (i = 0; i < 3; i++) {
        if (c0 > c1 || !dxt1) {
            colors[2][i] = (2 * colors[0][i] + colors[1][i]) / 3;
            colors[3][i] = (colors[0][i] + 2 * colors[1][i]) / 3;
        } else {
            colors[2][i] = (colors[0][i] + colors[1][i]) / 2;
            colors[3][i] = 0;
        }
    }
    if (dxt1 && c0 <= c1) {
        colors[3][3] = 0;
    }
}

static void DecodeS3TC(GLenum format, int width, int height, const byte *data, byte *out)
{
    int      bx, by, x, y;
    qboolean dxt1 = format == GL_COMPRESSED_RGB_S3TC_DXT1_EXT || format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    int      blockSize = dxt1 ? 8 : 16;

    for (by = 0; by < (height + 3) / 4; by++) {
        for (bx = 0; bx < (width + 3) / 4; bx++, data += blockSize) {
            const byte *colorBlock = dxt1 ? data : data + 8;
            byte        colors[4][4];
            byte        alpha[16];
            unsigned    indices = colorBlock[4] | (colorBlock[5] << 8) | (colorBlock[6] << 16) | ((unsigned)colorBlock[7] << 24);
            int         i;

            DecodeColors(colorBlock, colors, dxt1);
            if (format == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT) {
                for (i = 0; i < 16; i++) {
                    alpha[i] = ((data[i / 2] >> ((i & 1) * 4)) & 15) * 17;
                }
            } else if (format == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
                byte     a[8];
                uint64_t bits = 0;
                a[0]          = data[0];
                a[1]          = data[1];
                for (i = 2; i < 8; i++) {
                    a[i] = a[0] > a[1] ? ((8 - i) * a[0] + (i - 1) * a[1]) / 7
                         : i < 6       ? ((6 - i) * a[0] + (i - 1) * a[1]) / 5
                         : i == 6      ? 0
                                       : 255;
                }
                for (i = 0; i < 6; i++) {
                    bits |= (uint64_t)data[2 + i] << (8 * i);
                }
                for (i = 0; i < 16; i++) {
                    alpha[i] = a[(bits >> (3 * i)) & 7];
                }
            }
            for (y = 0; y < 4; y++) {
                for (x = 0; x < 4; x++) {
                    int   px = bx * 4 + x, py = by * 4 + y, i2 = y * 4 + x;
                    byte *dst;
                    if (px >= width || py >= height) {
                        continue;
                    }
                    dst = out + (py * width + px) * 4;
                    Com_Memcpy(dst, colors[(indices >> (2 * i2)) & 3], 4);
                    if (!dxt1) {
                        dst[3] = alpha[i2];
                    }
                }
            }
        }
    }
}

static void APIENTRY vkglCompressedTexImage2D(
    GLenum target, GLint level, GLenum format, GLsizei width, GLsizei height, GLint border, GLsizei size, const void *data
)
{
    glTexture_t *t;
    byte        *rgba;

    if (width <= 0 || height <= 0 || !data) {
        return;
    }
    if (format != GL_COMPRESSED_RGB_S3TC_DXT1_EXT && format != GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
        && format != GL_COMPRESSED_RGBA_S3TC_DXT3_EXT && format != GL_COMPRESSED_RGBA_S3TC_DXT5_EXT) {
        Warn("compressed formats other than S3TC");
        return;
    }
    t = TextureForLevel(level, format, width, height);
    if (!t) {
        return;
    }
    rgba = UploadBuffer(width * height * 4);
    DecodeS3TC(format, width, height, data, rgba);
    VK_UploadTexture(t->tex, level, 0, 0, width, height, rgba);
}

static void APIENTRY vkglTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
    glTexture_t *t = GetTexture(gl.bound[gl.activeUnit], qtrue);

    switch (pname) {
    case GL_TEXTURE_MIN_FILTER:
        t->minFilter = (GLenum)param;
        break;
    case GL_TEXTURE_MAG_FILTER:
        t->magFilter = (GLenum)param;
        break;
    case GL_TEXTURE_WRAP_S:
        t->wrapS = (GLenum)param;
        break;
    case GL_TEXTURE_WRAP_T:
        t->wrapT = (GLenum)param;
        break;
    case GL_TEXTURE_MAX_ANISOTROPY_EXT:
        t->anisotropy = param;
        break;
    default:
        break;
    }
}

static void APIENTRY vkglTexParameteri(GLenum target, GLenum pname, GLint param)
{
    vkglTexParameterf(target, pname, (GLfloat)param);
}

static void APIENTRY vkglTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
    glEnv_t *e = &gl.env[gl.activeUnit];
    GLenum   v = (GLenum)param;

    if (pname == GL_TEXTURE_ENV_MODE) {
        e->mode = v;
    } else if (pname == GL_COMBINE_RGB) {
        e->combineRgb = v;
    } else if (pname == GL_COMBINE_ALPHA) {
        e->combineAlpha = v;
    } else if (pname >= GL_SOURCE0_RGB && pname <= GL_SOURCE0_RGB + 3) {
        e->srcRgb[pname - GL_SOURCE0_RGB] = v;
    } else if (pname >= GL_SOURCE0_ALPHA && pname <= GL_SOURCE0_ALPHA + 3) {
        e->srcAlpha[pname - GL_SOURCE0_ALPHA] = v;
    } else if (pname >= GL_OPERAND0_RGB && pname <= GL_OPERAND0_RGB + 3) {
        e->opRgb[pname - GL_OPERAND0_RGB] = v;
    } else if (pname >= GL_OPERAND0_ALPHA && pname <= GL_OPERAND0_ALPHA + 3) {
        e->opAlpha[pname - GL_OPERAND0_ALPHA] = v;
    }
}

static void APIENTRY vkglBlendFunc(GLenum src, GLenum dst)
{
    gl.blendSrc = src;
    gl.blendDst = dst;
}

static void APIENTRY vkglDepthFunc(GLenum func)
{
    gl.depthFunc = func;
}

static void APIENTRY vkglDepthMask(GLboolean flag)
{
    gl.depthMask = flag;
}

static void APIENTRY vkglDepthRange(GLclampd n, GLclampd f)
{
    gl.depthNear = n;
    gl.depthFar  = f;
}

static void APIENTRY vkglAlphaFunc(GLenum func, GLclampf ref)
{
    gl.alphaFunc = func;
    gl.alphaRef  = ref;
}

static void APIENTRY vkglCullFace(GLenum mode)
{
    gl.cullFace = mode;
}

static void APIENTRY vkglPolygonMode(GLenum face, GLenum mode)
{
    gl.polygonMode = mode;
}

static void APIENTRY vkglPolygonOffset(GLfloat factor, GLfloat units)
{
    gl.offsetFactor = factor;
    gl.offsetUnits  = units;
}

static void APIENTRY vkglColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a)
{
    gl.colorMask = (r ? 1 : 0) | (g ? 2 : 0) | (b ? 4 : 0) | (a ? 8 : 0);
}

static void APIENTRY vkglLineWidth(GLfloat width)
{
    gl.lineWidth = width;
}

static void APIENTRY vkglStencilFunc(GLenum func, GLint ref, GLuint mask)
{
    gl.stencilFunc = func;
    gl.stencilRef  = ref;
    gl.stencilMask = mask;
}

static void APIENTRY vkglStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
    gl.stencilFail  = fail;
    gl.stencilZFail = zfail;
    gl.stencilZPass = zpass;
}

static void APIENTRY vkglStencilMask(GLuint mask)
{
    gl.stencilWriteMask = mask;
}

static void APIENTRY vkglFogf(GLenum pname, GLfloat param)
{
    switch (pname) {
    case GL_FOG_MODE:
        gl.fogMode = (GLenum)param;
        break;
    case GL_FOG_DENSITY:
        gl.fogDensity = param;
        break;
    case GL_FOG_START:
        gl.fogStart = param;
        break;
    case GL_FOG_END:
        gl.fogEnd = param;
        break;
    default:
        break;
    }
}

static void APIENTRY vkglFogi(GLenum pname, GLint param)
{
    vkglFogf(pname, (GLfloat)param);
}

static void APIENTRY vkglFogfv(GLenum pname, const GLfloat *params)
{
    if (pname == GL_FOG_COLOR) {
        Com_Memcpy(gl.fogColor, params, sizeof(gl.fogColor));
    } else {
        vkglFogf(pname, params[0]);
    }
}

static void APIENTRY vkglClipPlane(GLenum plane, const GLdouble *equation)
{
    float inverse[16];
    float p[4];
    int   i;

    if (plane != GL_CLIP_PLANE0) {
        return;
    }
    for (i = 0; i < 4; i++) {
        p[i] = (float)equation[i];
    }
    // GL stores the plane in eye space: p_eye = p * inverse(modelview)
    if (!MatrixInvert4(gl.matrix[0][gl.depth[0]], inverse)) {
        Identity(inverse);
    }
    for (i = 0; i < 4; i++) {
        gl.clipPlane[i] = p[0] * inverse[i * 4 + 0] + p[1] * inverse[i * 4 + 1] + p[2] * inverse[i * 4 + 2]
                        + p[3] * inverse[i * 4 + 3];
    }
}

static void APIENTRY vkglMatrixMode(GLenum mode)
{
    gl.matrixMode = mode == GL_PROJECTION ? 1 : 0;
    if (mode == GL_TEXTURE) {
        Warn("texture matrices");
    }
}

static void APIENTRY vkglLoadIdentity(void)
{
    Identity(CurrentMatrix());
}

static void APIENTRY vkglLoadMatrixf(const GLfloat *m)
{
    Com_Memcpy(CurrentMatrix(), m, sizeof(float) * 16);
}

static void APIENTRY vkglPushMatrix(void)
{
    int mode = gl.matrixMode;
    if (gl.depth[mode] + 1 < MATRIX_DEPTH) {
        Com_Memcpy(gl.matrix[mode][gl.depth[mode] + 1], gl.matrix[mode][gl.depth[mode]], sizeof(float) * 16);
        gl.depth[mode]++;
    }
}

static void APIENTRY vkglPopMatrix(void)
{
    if (gl.depth[gl.matrixMode] > 0) {
        gl.depth[gl.matrixMode]--;
    }
}

static void APIENTRY vkglTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
    float t[16];
    Identity(t);
    t[12] = x;
    t[13] = y;
    t[14] = z;
    MatrixMultiply4(CurrentMatrix(), t, CurrentMatrix());
}

static void APIENTRY vkglOrtho(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
    float o[16];
    Com_Memset(o, 0, sizeof(o));
    o[0]  = (float)(2.0 / (r - l));
    o[5]  = (float)(2.0 / (t - b));
    o[10] = (float)(-2.0 / (f - n));
    o[12] = (float)(-(r + l) / (r - l));
    o[13] = (float)(-(t + b) / (t - b));
    o[14] = (float)(-(f + n) / (f - n));
    o[15] = 1.0f;
    MatrixMultiply4(CurrentMatrix(), o, CurrentMatrix());
}

static void APIENTRY vkglFrustum(GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f)
{
    float m[16];
    Com_Memset(m, 0, sizeof(m));
    m[0]  = (float)(2 * n / (r - l));
    m[5]  = (float)(2 * n / (t - b));
    m[8]  = (float)((r + l) / (r - l));
    m[9]  = (float)((t + b) / (t - b));
    m[10] = (float)(-(f + n) / (f - n));
    m[11] = -1.0f;
    m[14] = (float)(-2 * f * n / (f - n));
    MatrixMultiply4(CurrentMatrix(), m, CurrentMatrix());
}

static void APIENTRY vkglViewport(GLint x, GLint y, GLsizei w, GLsizei h)
{
    gl.viewport[0] = x;
    gl.viewport[1] = y;
    gl.viewport[2] = w;
    gl.viewport[3] = h;
}

static void APIENTRY vkglScissor(GLint x, GLint y, GLsizei w, GLsizei h)
{
    gl.scissor[0] = x;
    gl.scissor[1] = y;
    gl.scissor[2] = w;
    gl.scissor[3] = h;
}

static void APIENTRY vkglClearColor(GLclampf r, GLclampf g, GLclampf b, GLclampf a)
{
    gl.clearColor[0] = r;
    gl.clearColor[1] = g;
    gl.clearColor[2] = b;
    gl.clearColor[3] = a;
}

static void APIENTRY vkglClearDepth(GLclampd d)
{
    gl.clearDepth = d;
}

static void APIENTRY vkglClearStencil(GLint s)
{
    gl.clearStencil = s;
}

static void APIENTRY vkglClear(GLbitfield mask)
{
    int      fbWidth = VK_FramebufferWidth(), fbHeight = VK_FramebufferHeight();
    VkRect2D rect;

    rect.offset.x      = 0;
    rect.offset.y      = 0;
    rect.extent.width  = fbWidth;
    rect.extent.height = fbHeight;
    if (gl.scissorTest) {
        int x0 = Q_max(0, gl.scissor[0]), x1 = Q_min(fbWidth, gl.scissor[0] + gl.scissor[2]);
        int y1 = Q_min(fbHeight, fbHeight - gl.scissor[1]), y0 = Q_max(0, fbHeight - (gl.scissor[1] + gl.scissor[3]));
        rect.offset.x      = x0;
        rect.offset.y      = y0;
        rect.extent.width  = Q_max(0, x1 - x0);
        rect.extent.height = Q_max(0, y1 - y0);
    }
    // shortcut: a partial color mask clears all channels; only the renderer's full masks are used
    VK_Clear(
        &rect,
        (mask & GL_COLOR_BUFFER_BIT) && gl.colorMask,
        gl.clearColor,
        (mask & GL_DEPTH_BUFFER_BIT) && gl.depthMask,
        gl.clearDepth,
        (mask & GL_STENCIL_BUFFER_BIT) != 0,
        gl.clearStencil
    );
}

static void APIENTRY vkglReadPixels(GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, GLvoid *pixels)
{
    int   components = format == GL_RGBA || format == GL_BGRA ? 4 : 3;
    int   rowBytes, row, col;
    byte *rgba;

    if (w <= 0 || h <= 0) {
        return;
    }
    if (type != GL_UNSIGNED_BYTE || (format != GL_RGB && format != GL_RGBA && format != GL_BGR && format != GL_BGRA)) {
        Warn("glReadPixels formats other than RGB(A)/BGR(A) bytes");
        return;
    }
    rowBytes = (w * components + gl.packAlignment - 1) & ~(gl.packAlignment - 1);
    rgba     = UploadBuffer(w * h * 4);
    Com_Memset(rgba, 0, w * h * 4);
    VK_ReadPixels(x, VK_FramebufferHeight() - (y + h), w, h, rgba);
    // GL rows go bottom to top
    for (row = 0; row < h; row++) {
        const byte *src = rgba + (h - 1 - row) * w * 4;
        byte       *dst = (byte *)pixels + row * rowBytes;
        for (col = 0; col < w; col++, src += 4, dst += components) {
            qboolean bgr = format == GL_BGR || format == GL_BGRA;
            dst[0]       = bgr ? src[2] : src[0];
            dst[1]       = src[1];
            dst[2]       = bgr ? src[0] : src[2];
            if (components == 4) {
                dst[3] = src[3];
            }
        }
    }
}

static void APIENTRY vkglGetIntegerv(GLenum pname, GLint *params)
{
    switch (pname) {
    case GL_MAX_TEXTURE_SIZE:
        params[0] = VK_MaxTextureSize();
        break;
    case GL_MAX_TEXTURE_UNITS_ARB:
        params[0] = MAX_UNITS;
        break;
    case GL_PACK_ALIGNMENT:
        params[0] = gl.packAlignment;
        break;
    case GL_UNPACK_ALIGNMENT:
        params[0] = gl.unpackAlignment;
        break;
    case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
        params[0] = (GLint)VK_MaxAnisotropy();
        break;
    case GL_VIEWPORT:
        Com_Memcpy(params, gl.viewport, sizeof(gl.viewport));
        break;
    default:
        params[0] = 0;
        break;
    }
}

static void APIENTRY vkglGetBooleanv(GLenum pname, GLboolean *params)
{
    params[0] = GL_FALSE;
}

static const GLubyte *APIENTRY vkglGetString(GLenum name)
{
    static char renderer[256];

    switch (name) {
    case GL_VENDOR:
        return (const GLubyte *)"OpenMOHAA Plus";
    case GL_RENDERER:
        Com_sprintf(renderer, sizeof(renderer), "%s (Vulkan)", VK_DeviceName());
        return (const GLubyte *)renderer;
    case GL_VERSION:
        return (const GLubyte *)"1.1 Vulkan";
    case GL_EXTENSIONS:
        return (const GLubyte *)"GL_ARB_multitexture GL_EXT_texture_env_add GL_EXT_compiled_vertex_array "
                                "GL_ARB_texture_env_combine GL_NV_texture_env_combine4 GL_SGIS_texture_edge_clamp "
                                "GL_EXT_texture_filter_anisotropic";
    }
    return (const GLubyte *)"";
}

static GLenum APIENTRY vkglGetError(void)
{
    return GL_NO_ERROR;
}

static void APIENTRY vkglFinish(void)
{
    VK_Finish();
}

static void APIENTRY vkglFlush(void) {}

static void APIENTRY vkglDrawBuffer(GLenum mode) {}

static void APIENTRY vkglShadeModel(GLenum mode) {}

static void APIENTRY vkglLineStipple(GLint factor, GLushort pattern) {}

static void APIENTRY vkglPixelZoom(GLfloat x, GLfloat y) {}

static void APIENTRY vkglDrawPixels(GLsizei w, GLsizei h, GLenum format, GLenum type, const GLvoid *pixels)
{
    Warn("glDrawPixels");
}

static void APIENTRY vkglCallList(GLuint list)
{
    Warn("display lists");
}

static void APIENTRY vkglCopyTexSubImage2D(GLenum t, GLint l, GLint xo, GLint yo, GLint x, GLint y, GLsizei w, GLsizei h)
{
    Warn("glCopyTexSubImage2D");
}

/*
==============================================================================

Setup

==============================================================================
*/

static void ResetState(void)
{
    int u;

    Com_Memset(&gl, 0, sizeof(gl));
    Identity(gl.matrix[0][0]);
    Identity(gl.matrix[1][0]);
    gl.blendSrc         = GL_ONE;
    gl.blendDst         = GL_ZERO;
    gl.depthFunc        = GL_LESS;
    gl.depthMask        = qtrue;
    gl.alphaFunc        = GL_ALWAYS;
    gl.cullFace         = GL_BACK;
    gl.polygonMode      = GL_FILL;
    gl.colorMask        = 15;
    gl.lineWidth        = 1.0f;
    gl.fogMode          = GL_EXP;
    gl.fogDensity       = 1.0f;
    gl.fogEnd           = 1.0f;
    gl.stencilFunc      = GL_ALWAYS;
    gl.stencilMask      = 0xFFFFFFFF;
    gl.stencilWriteMask = 0xFFFFFFFF;
    gl.stencilFail      = GL_KEEP;
    gl.stencilZFail     = GL_KEEP;
    gl.stencilZPass     = GL_KEEP;
    gl.viewport[2]      = VK_FramebufferWidth();
    gl.viewport[3]      = VK_FramebufferHeight();
    gl.scissor[2]       = gl.viewport[2];
    gl.scissor[3]       = gl.viewport[3];
    gl.depthFar         = 1.0f;
    gl.clearDepth       = 1.0f;
    gl.currentColor[0] = gl.currentColor[1] = gl.currentColor[2] = gl.currentColor[3] = 1.0f;
    gl.packAlignment    = 4;
    gl.unpackAlignment  = 4;
    gl.nextName         = 1 << 20; // above the renderer's own texture numbers
    for (u = 0; u < MAX_UNITS; u++) {
        glEnv_t *e      = &gl.env[u];
        e->mode         = GL_MODULATE;
        e->combineRgb   = GL_MODULATE;
        e->combineAlpha = GL_MODULATE;
        e->srcRgb[0] = e->srcAlpha[0] = GL_TEXTURE;
        e->srcRgb[1] = e->srcAlpha[1] = GL_PREVIOUS;
        e->srcRgb[2] = e->srcAlpha[2] = GL_CONSTANT;
        e->srcRgb[3] = e->srcAlpha[3] = GL_ZERO;
        e->opRgb[0] = e->opRgb[1] = e->opRgb[3] = GL_SRC_COLOR;
        e->opRgb[2]                             = GL_SRC_ALPHA;
        e->opAlpha[0] = e->opAlpha[1] = e->opAlpha[2] = e->opAlpha[3] = GL_SRC_ALPHA;
    }
}

void VK_GL_SetProcAddresses(void)
{
    ResetState();

    // every pointer the GL1 renderer may call; the rest stay NULL
#define GLE(ret, name, ...) qgl##name = NULL;
    QGL_1_1_PROCS;
    QGL_1_1_FIXED_FUNCTION_PROCS;
    QGL_DESKTOP_1_1_PROCS;
    QGL_DESKTOP_1_1_FIXED_FUNCTION_PROCS;
    QGL_1_3_PROCS;
#undef GLE

    qglBindTexture           = vkglBindTexture;
    qglBlendFunc             = vkglBlendFunc;
    qglCallList              = vkglCallList;
    qglClearColor            = vkglClearColor;
    qglClear                 = vkglClear;
    qglClearStencil          = vkglClearStencil;
    qglColorMask             = vkglColorMask;
    qglCopyTexSubImage2D     = vkglCopyTexSubImage2D;
    qglCullFace              = vkglCullFace;
    qglDeleteTextures        = vkglDeleteTextures;
    qglDepthFunc             = vkglDepthFunc;
    qglDepthMask             = vkglDepthMask;
    qglDisable               = vkglDisable;
    qglDrawArrays            = vkglDrawArrays;
    qglDrawElements          = vkglDrawElements;
    qglDrawPixels            = vkglDrawPixels;
    qglEnable                = vkglEnable;
    qglFinish                = vkglFinish;
    qglFlush                 = vkglFlush;
    qglFogf                  = vkglFogf;
    qglFogfv                 = vkglFogfv;
    qglFogi                  = vkglFogi;
    qglGenTextures           = vkglGenTextures;
    qglGetBooleanv           = vkglGetBooleanv;
    qglGetError              = vkglGetError;
    qglGetIntegerv           = vkglGetIntegerv;
    qglGetString             = vkglGetString;
    qglLineStipple           = vkglLineStipple;
    qglLineWidth             = vkglLineWidth;
    qglPixelZoom             = vkglPixelZoom;
    qglPolygonOffset         = vkglPolygonOffset;
    qglReadPixels            = vkglReadPixels;
    qglScissor               = vkglScissor;
    qglStencilFunc           = vkglStencilFunc;
    qglStencilMask           = vkglStencilMask;
    qglStencilOp             = vkglStencilOp;
    qglTexImage2D            = vkglTexImage2D;
    qglTexParameterf         = vkglTexParameterf;
    qglTexParameteri         = vkglTexParameteri;
    qglTexSubImage2D         = vkglTexSubImage2D;
    qglTranslatef            = vkglTranslatef;
    qglViewport              = vkglViewport;
    qglAlphaFunc             = vkglAlphaFunc;
    qglColor4f               = vkglColor4f;
    qglColorPointer          = vkglColorPointer;
    qglDisableClientState    = vkglDisableClientState;
    qglEnableClientState     = vkglEnableClientState;
    qglLoadIdentity          = vkglLoadIdentity;
    qglLoadMatrixf           = vkglLoadMatrixf;
    qglMatrixMode            = vkglMatrixMode;
    qglPopMatrix             = vkglPopMatrix;
    qglPushMatrix            = vkglPushMatrix;
    qglShadeModel            = vkglShadeModel;
    qglTexCoordPointer       = vkglTexCoordPointer;
    qglTexEnvf               = vkglTexEnvf;
    qglVertexPointer         = vkglVertexPointer;
    qglClearDepth            = vkglClearDepth;
    qglDepthRange            = vkglDepthRange;
    qglDrawBuffer            = vkglDrawBuffer;
    qglPolygonMode           = vkglPolygonMode;
    qglArrayElement          = vkglArrayElement;
    qglBegin                 = vkglBegin;
    qglClipPlane             = vkglClipPlane;
    qglColor3f               = vkglColor3f;
    qglColor4ubv             = vkglColor4ubv;
    qglEnd                   = vkglEnd;
    qglFrustum               = vkglFrustum;
    qglOrtho                 = vkglOrtho;
    qglTexCoord2f            = vkglTexCoord2f;
    qglTexCoord2fv           = vkglTexCoord2fv;
    qglVertex2f              = vkglVertex2f;
    qglVertex3f              = vkglVertex3f;
    qglVertex3fv             = vkglVertex3fv;
    qglActiveTexture         = vkglActiveTextureARB;
    qglCompressedTexImage2D  = vkglCompressedTexImage2D;

    qglActiveTextureARB       = vkglActiveTextureARB;
    qglClientActiveTextureARB = vkglClientActiveTextureARB;
    qglMultiTexCoord2fARB     = vkglMultiTexCoord2fARB;
    qglLockArraysEXT          = vkglLockArraysEXT;
    qglUnlockArraysEXT        = vkglUnlockArraysEXT;
}

void VK_GL_Shutdown(void)
{
    int i;

    for (i = 0; i < numTextureSlots; i++) {
        if (textures[i].tex) {
            VK_DestroyTexture(textures[i].tex);
        }
    }
    if (textures) {
        ri.Free(textures);
    }
    textures        = NULL;
    numTextureSlots = 0;
    if (uploadBuffer) {
        ri.Free(uploadBuffer);
    }
    uploadBuffer     = NULL;
    uploadBufferSize = 0;
}
