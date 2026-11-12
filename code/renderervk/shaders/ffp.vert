#version 450
// Copyright (C) 2026 the OpenMOHAA Plus contributors
// SPDX-License-Identifier: AGPL-3.0-or-later
#extension GL_GOOGLE_include_directive : require
// Added in OpenMOHAA Plus: vertex stage of the emulated fixed-function pipeline
#include "ffp_common.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in vec2 inTc0;
layout(location = 3) in vec2 inTc1;

layout(location = 0) out vec4 vColor;
layout(location = 1) out vec2 vTc0;
layout(location = 2) out vec2 vTc1;
layout(location = 3) out float vEyeDepth;

out gl_PerVertex {
    vec4  gl_Position;
    float gl_PointSize;
    float gl_ClipDistance[1];
};

void main()
{
    vec4 eye = u.modelView * vec4(inPos, 1.0);
    vec4 pos = u.mvp * vec4(inPos, 1.0);

    vColor    = inColor;
    vTc0      = inTc0;
    vTc1      = inTc1;
    vEyeDepth = abs(eye.z);

    gl_ClipDistance[0] = u.misc.z != 0 ? dot(u.clipPlane, eye) : 1.0;
    gl_PointSize       = 1.0;
    // OpenGL clip space to Vulkan: y points down, depth 0..1
    pos.y       = -pos.y;
    pos.z       = (pos.z + pos.w) * 0.5;
    gl_Position = pos;
}
