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

// g_antilag.h: Added in OpenMOHAA Plus
//  Lag compensation for bullets (g_antilag), see g_antilag.cpp.

#pragma once

#include "g_local.h"

class Entity;

// What decides where a player is hit: position, angles and animation pose
struct AntilagRecord {
    int         time; // level.svsTime of the frame
    vec3_t      origin;
    vec3_t      angles;
    frameInfo_t frameInfo[MAX_FRAMEINFOS];
    float       actionWeight;
    int         bone_tag[NUM_BONE_CONTROLLERS];
    quat_t      bone_quat[NUM_BONE_CONTROLLERS];
};

// Record every player for this frame (end of the server frame)
void G_AntilagStore(void);

// While it lives, the other players are back where the shooter saw them
class AntilagRewind
{
public:
    explicit AntilagRewind(Entity *shooter);
    ~AntilagRewind();

private:
    bool          m_active;
    bool          m_moved[MAX_CLIENTS];
    AntilagRecord m_saved[MAX_CLIENTS];
    frameInfo_t   m_rewoundFrames[MAX_CLIENTS][MAX_FRAMEINFOS];
};
