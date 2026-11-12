// Copyright (C) 2026 the OpenMOHAA Plus contributors
// SPDX-License-Identifier: AGPL-3.0-or-later
// Added in OpenMOHAA Plus: per-draw state of the emulated OpenGL 1.x fixed-function pipeline (vk_gl.c)
layout(set = 0, binding = 0, std140) uniform DrawState {
    mat4  mvp;
    mat4  modelView;
    vec4  clipPlane;    // eye space
    vec4  fogColor;
    vec4  fogParams;    // start, end, density, unused
    vec4  alphaParams;  // reference, unused...
    ivec4 misc;         // alpha func (GL enum, 0 = off), fog mode (GL enum, 0 = off), clip plane on, unused
    // per texture unit: (enabled, env mode, combine rgb, combine alpha), sources rgb, operands rgb,
    // sources alpha, operands alpha (GL enums)
    ivec4 env[10];
} u;
