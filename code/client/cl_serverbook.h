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
