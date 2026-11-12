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

// gs_resolve.h: Added in OpenMOHAA Plus
//  Host name lookups that never block the caller (upstream #376: the server browser froze the game
//  while the DNS lookup of a master server hung).

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Returns 1 and the IPv4 address (network byte order) when the host is resolved, 0 while the lookup
// runs in the background, -1 when the last lookup failed (retried after a while).
int GS_LookupHost(const char *host, unsigned int *address);

#ifdef __cplusplus
}
#endif
