/*
===========================================================================
Copyright (C) 2026 the OpenMOHAA Plus contributors

This file is part of OpenMOHAA Plus source code.

OpenMOHAA Plus source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMOHAA Plus source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMOHAA Plus source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// g_antilag.cpp: Added in OpenMOHAA Plus
//  Lag compensation for bullets (g_antilag). A shooter sees the other players where they were a
//  ping and an interpolation delay ago; when a player fires bullets, every other player is moved
//  back to that moment (position, angles and animation pose, which the bone hit locations use),
//  the bullets are traced, and everything is put back.

#include "g_local.h"
#include "g_antilag.h"
#include "player.h"

namespace
{
const int HISTORY = 64; // frames: 3.2 s at 20 Hz, 1.6 s at 40 Hz


struct History {
    AntilagRecord records[HISTORY];
    int    count;
    int    head; // next slot to write
};

History s_history[MAX_CLIENTS];

void Capture(const gentity_t *ent, AntilagRecord& r)
{
    VectorCopy(ent->s.origin, r.origin);
    VectorCopy(ent->s.angles, r.angles);
    memcpy(r.frameInfo, ent->s.frameInfo, sizeof(r.frameInfo));
    r.actionWeight = ent->s.actionWeight;
    memcpy(r.bone_tag, ent->s.bone_tag, sizeof(r.bone_tag));
    memcpy(r.bone_quat, ent->s.bone_quat, sizeof(r.bone_quat));
}

void Apply(gentity_t *ent, const AntilagRecord& r)
{
    VectorCopy(r.origin, ent->s.origin);
    VectorCopy(r.angles, ent->s.angles);
    VectorCopy(r.angles, ent->r.currentAngles);
    memcpy(ent->s.frameInfo, r.frameInfo, sizeof(r.frameInfo));
    ent->s.actionWeight = r.actionWeight;
    memcpy(ent->s.bone_tag, r.bone_tag, sizeof(r.bone_tag));
    memcpy(ent->s.bone_quat, r.bone_quat, sizeof(r.bone_quat));
    // the bone hit locations come from a pose cached once per frame
    level.skel_index[ent->s.number] = -1;
    gi.linkentity(ent);
}

// Sideways distance from the shooter's aim to a point (pitch and recoil left out)
float SidewaysMiss(Player *player, const vec3_t point)
{
    Vector forward;
    Vector to = Vector(point) - player->m_vViewPos;

    Vector(0, player->m_vViewAng.y, 0).AngleVectors(&forward);
    to.z = 0;
    return (to - forward * (to * forward)).length();
}

bool IsTarget(const gentity_t *ent)
{
    return ent->inuse && ent->client && ent->entity && ent->entity->IsSubclassOfPlayer()
        && !ent->entity->IsDead() && !static_cast<Player *>(ent->entity)->IsSpectator();
}
} // namespace

void G_AntilagStore(void)
{
    if (!g_antilag->integer || g_gametype->integer == GT_SINGLE_PLAYER) {
        return;
    }

    for (int i = 0; i < game.maxclients && i < MAX_CLIENTS; i++) {
        gentity_t *ent = g_entities + i;
        History&   h   = s_history[i];

        if (!IsTarget(ent)) {
            h.count = 0; // no rewinding across a death, respawn or teleport to spectator
            continue;
        }
        AntilagRecord& r = h.records[h.head];
        r.time    = level.svsTime;
        Capture(ent, r);
        h.head  = (h.head + 1) % HISTORY;
        h.count = Q_min(h.count + 1, HISTORY);
    }
}

AntilagRewind::AntilagRewind(Entity *shooter)
    : m_active(false)
{
    if (!g_antilag->integer || g_gametype->integer == GT_SINGLE_PLAYER || !shooter
        || !shooter->IsSubclassOfPlayer()) {
        return;
    }

    Player   *player = static_cast<Player *>(shooter);
    const int now    = level.svsTime;
    int       target = player->GetLastUcmd().serverTime;

    // bots and local players see the present; never go back further than g_antilag_maxms
    if (target >= now) {
        return;
    }
    target = Q_max(target, now - Q_max(0, g_antilag_maxms->integer));

    for (int i = 0; i < game.maxclients && i < MAX_CLIENTS; i++) {
        gentity_t     *ent = g_entities + i;
        const History& h   = s_history[i];
        const AntilagRecord *before = NULL, *after = NULL;

        m_moved[i] = false;
        if (ent == shooter->edict || !IsTarget(ent) || !h.count) {
            continue;
        }

        // newest record at or before the target time, and the one after it
        for (int k = 0; k < h.count; k++) {
            const AntilagRecord& r = h.records[(h.head - 1 - k + HISTORY) % HISTORY];
            if (r.time <= target) {
                before = &r;
                break;
            }
            after = &r;
        }
        if (!before) {
            continue; // not that old in the history
        }

        AntilagRecord rewound = *before;
        if (after && after->time > before->time) {
            const float frac = (float)(target - before->time) / (after->time - before->time);
            for (int a = 0; a < 3; a++) {
                rewound.origin[a] = before->origin[a] + (after->origin[a] - before->origin[a]) * frac;
            }
        }

        if (g_antilag_debug->integer) {
            gi.Printf(
                "antilag: %s rewound %s %d ms; aim passes %.1f units from it, %.1f from where it is now\n",
                player->client->pers.netname,
                ent->client->pers.netname,
                now - target,
                SidewaysMiss(player, rewound.origin),
                SidewaysMiss(player, ent->s.origin)
            );
        }

        Capture(ent, m_saved[i]);
        Apply(ent, rewound);
        memcpy(m_rewoundFrames[i], rewound.frameInfo, sizeof(rewound.frameInfo));
        m_moved[i] = true;
        m_active   = true;
    }
}

AntilagRewind::~AntilagRewind()
{
    if (!m_active) {
        return;
    }
    for (int i = 0; i < game.maxclients && i < MAX_CLIENTS; i++) {
        gentity_t    *ent = g_entities + i;
        AntilagRecord back;

        if (!m_moved[i]) {
            continue;
        }
        back = m_saved[i];
        if (memcmp(ent->s.frameInfo, m_rewoundFrames[i], sizeof(back.frameInfo))) {
            // a bullet changed the animation (killed it): keep the new one, only move it back
            Capture(ent, back);
            VectorCopy(m_saved[i].origin, back.origin);
            VectorCopy(m_saved[i].angles, back.angles);
        }
        Apply(ent, back);
    }
}
