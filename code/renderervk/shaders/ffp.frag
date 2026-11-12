#version 450
// Copyright (C) 2026 the OpenMOHAA Plus contributors
// SPDX-License-Identifier: AGPL-3.0-or-later
#extension GL_GOOGLE_include_directive : require
// Added in OpenMOHAA Plus: fragment stage of the emulated fixed-function pipeline
// (texture environments including GL_COMBINE / GL_COMBINE4_NV, alpha test, fog)
#include "ffp_common.glsl"

layout(set = 0, binding = 1) uniform sampler2D tex0;
layout(set = 0, binding = 2) uniform sampler2D tex1;

layout(location = 0) in vec4 vColor;
layout(location = 1) in vec2 vTc0;
layout(location = 2) in vec2 vTc1;
layout(location = 3) in float vEyeDepth;

layout(location = 0) out vec4 outColor;

// GL enums
const int G_ZERO = 0, G_ADD = 0x0104, G_MODULATE = 0x2100, G_DECAL = 0x2101, G_REPLACE = 0x1E01;
const int G_COMBINE = 0x8570, G_COMBINE4_NV = 0x8503, G_ADD_SIGNED = 0x8574, G_INTERPOLATE = 0x8575,
          G_SUBTRACT = 0x84E7;
const int G_TEXTURE = 0x1702, G_TEXTURE0 = 0x84C0, G_TEXTURE1 = 0x84C1, G_CONSTANT = 0x8576,
          G_PRIMARY_COLOR = 0x8577, G_PREVIOUS = 0x8578;
const int G_SRC_COLOR = 0x0300, G_ONE_MINUS_SRC_COLOR = 0x0301, G_SRC_ALPHA = 0x0302,
          G_ONE_MINUS_SRC_ALPHA = 0x0303;
const int G_NEVER = 0x0200, G_LESS = 0x0201, G_EQUAL = 0x0202, G_LEQUAL = 0x0203, G_GREATER = 0x0204,
          G_NOTEQUAL = 0x0205, G_GEQUAL = 0x0206;
const int G_LINEAR = 0x2601, G_EXP = 0x0800, G_EXP2 = 0x0801;

vec4 source(int src, vec4 tex, vec4 t0, vec4 t1, vec4 prev)
{
    if (src == G_TEXTURE) return tex;
    if (src == G_TEXTURE0) return t0;
    if (src == G_TEXTURE1) return t1;
    if (src == G_PRIMARY_COLOR) return vColor;
    if (src == G_PREVIOUS) return prev;
    if (src == G_ZERO) return vec4(0.0);
    return vec4(0.0); // GL_CONSTANT: the env color is never set by the renderer
}

vec3 operandRgb(int op, vec4 v)
{
    if (op == G_ONE_MINUS_SRC_COLOR) return 1.0 - v.rgb;
    if (op == G_SRC_ALPHA) return vec3(v.a);
    if (op == G_ONE_MINUS_SRC_ALPHA) return vec3(1.0 - v.a);
    return v.rgb;
}

float operandAlpha(int op, vec4 v)
{
    return op == G_ONE_MINUS_SRC_ALPHA ? 1.0 - v.a : v.a;
}

vec4 applyEnv(int unit, vec4 tex, vec4 t0, vec4 t1, vec4 prev)
{
    ivec4 e    = u.env[unit * 5];
    int   mode = e.y;

    if (e.x == 0) return prev;
    if (mode == G_MODULATE) return prev * tex;
    if (mode == G_REPLACE) return tex;
    if (mode == G_DECAL) return vec4(mix(prev.rgb, tex.rgb, tex.a), prev.a);
    if (mode == G_ADD) return vec4(prev.rgb + tex.rgb, prev.a * tex.a);

    // GL_COMBINE (3 arguments) and GL_COMBINE4_NV (4 arguments)
    ivec4 srcRgb = u.env[unit * 5 + 1], opRgb = u.env[unit * 5 + 2];
    ivec4 srcA = u.env[unit * 5 + 3], opA = u.env[unit * 5 + 4];
    vec3  a[4];
    float b[4];
    for (int i = 0; i < 4; i++) {
        a[i] = operandRgb(opRgb[i], source(srcRgb[i], tex, t0, t1, prev));
        b[i] = operandAlpha(opA[i], source(srcA[i], tex, t0, t1, prev));
    }

    vec3  rgb;
    float alpha;
    bool  four = mode == G_COMBINE4_NV;
    int   fr = e.z, fa = e.w;

    if (fr == G_REPLACE) rgb = a[0];
    else if (fr == G_MODULATE) rgb = a[0] * a[1];
    else if (fr == G_ADD) rgb = four ? a[0] * a[1] + a[2] * a[3] : a[0] + a[1];
    else if (fr == G_ADD_SIGNED) rgb = (four ? a[0] * a[1] + a[2] * a[3] : a[0] + a[1]) - 0.5;
    else if (fr == G_INTERPOLATE) rgb = a[0] * a[2] + a[1] * (1.0 - a[2]);
    else if (fr == G_SUBTRACT) rgb = a[0] - a[1];
    else rgb = a[0] * a[1];

    if (fa == G_REPLACE) alpha = b[0];
    else if (fa == G_MODULATE) alpha = b[0] * b[1];
    else if (fa == G_ADD) alpha = four ? b[0] * b[1] + b[2] * b[3] : b[0] + b[1];
    else if (fa == G_ADD_SIGNED) alpha = (four ? b[0] * b[1] + b[2] * b[3] : b[0] + b[1]) - 0.5;
    else if (fa == G_INTERPOLATE) alpha = b[0] * b[2] + b[1] * (1.0 - b[2]);
    else if (fa == G_SUBTRACT) alpha = b[0] - b[1];
    else alpha = b[0] * b[1];

    return clamp(vec4(rgb, alpha), 0.0, 1.0);
}

bool alphaPasses(float a)
{
    float ref = u.alphaParams.x;
    switch (u.misc.x) {
    case G_NEVER: return false;
    case G_LESS: return a < ref;
    case G_EQUAL: return a == ref;
    case G_LEQUAL: return a <= ref;
    case G_GREATER: return a > ref;
    case G_NOTEQUAL: return a != ref;
    case G_GEQUAL: return a >= ref;
    }
    return true;
}

void main()
{
    vec4 t0 = texture(tex0, vTc0);
    vec4 t1 = texture(tex1, vTc1);
    vec4 c  = applyEnv(0, t0, t0, t1, vColor);
    c       = clamp(applyEnv(1, t1, t0, t1, c), 0.0, 1.0);

    if (u.misc.x != 0 && !alphaPasses(c.a)) {
        discard;
    }

    if (u.misc.y != 0) {
        float f;
        if (u.misc.y == G_LINEAR) {
            f = (u.fogParams.y - vEyeDepth) / max(u.fogParams.y - u.fogParams.x, 1e-6);
        } else if (u.misc.y == G_EXP) {
            f = exp(-u.fogParams.z * vEyeDepth);
        } else {
            f = exp(-pow(u.fogParams.z * vEyeDepth, 2.0));
        }
        c.rgb = mix(u.fogColor.rgb, c.rgb, clamp(f, 0.0, 1.0));
    }
    outColor = c;
}
