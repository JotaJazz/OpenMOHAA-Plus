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

// cl_serverbook.h: Added in OpenMOHAA Plus
//  Favorite and recently played servers for the server browser (cl_serverbook.cpp).

#pragma once

enum {
    SERVERBOOK_FAVORITES,
    SERVERBOOK_HISTORY
};

void CL_ServerBook_Init(void);
bool CL_ServerBook_IsFavorite(const char *address);
bool CL_ServerBook_IsRecent(const char *address);
// Returns the new state: true when the server is now a favorite
bool CL_ServerBook_ToggleFavorite(const char *address, int queryPort);
// The browser remembers the query port of the server the player connects to
void CL_ServerBook_Selected(const char *address, int queryPort);
// Called once the client has the server's game state
void CL_ServerBook_Connected(void);
void CL_ServerBook_ForEach(int which, void (*callback)(const char *address, int queryPort, void *data), void *data);
