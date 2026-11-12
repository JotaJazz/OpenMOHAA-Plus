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

// cl_serverbook.cpp: Added in OpenMOHAA Plus
//  Favorite and recently played servers for the server browser.
//  serverfavorites.txt / serverhistory.txt, one server per line: <ip>:<game port> <query port>

#include "client.h"
#include "cl_serverbook.h"

#include <string>
#include <vector>

namespace
{
struct Entry {
    std::string address; // ip:port used to connect
    int         queryPort; // GameSpy query port
};

const int   MAX_HISTORY    = 20;
const char *FAVORITES_FILE = "serverfavorites.txt";
const char *HISTORY_FILE   = "serverhistory.txt";

std::vector<Entry> s_favorites;
std::vector<Entry> s_history;
bool               s_loaded;
Entry              s_lastSelected; // query port of the server picked in the browser

int DefaultQueryPort(const std::string& address)
{
    // shortcut: servers usually keep the default pair 12203 / 12300 (net_port / net_gamespy_port), and
    // hosts with several servers shift both; a server with another query port must be added from the
    // browser (key F), which knows it
    const size_t colon = address.rfind(':');
    const int    port  = colon == std::string::npos ? 12203 : atoi(address.c_str() + colon + 1);
    return port + 97;
}

void Load(const char *file, std::vector<Entry>& list)
{
    char *text;

    list.clear();
    if (FS_ReadFile(file, (void **)&text) < 0 || !text) {
        return;
    }
    for (char *line = text; line && *line;) {
        char *next = strchr(line, '\n');
        char  address[128];
        int   queryPort = 0;

        if (next) {
            *next++ = 0;
        }
        if (sscanf(line, "%127s %d", address, &queryPort) >= 1 && address[0] != '/' && address[0] != '#') {
            list.push_back({address, queryPort > 0 ? queryPort : DefaultQueryPort(address)});
        }
        line = next;
    }
    FS_FreeFile(text);
}

void Save(const char *file, const std::vector<Entry>& list)
{
    fileHandle_t f = FS_FOpenFileWrite_HomeConfig(file);

    if (!f) {
        Com_Printf("Couldn't write %s\n", file);
        return;
    }
    for (const Entry& e : list) {
        FS_Printf(f, "%s %d\n", e.address.c_str(), e.queryPort);
    }
    FS_FCloseFile(f);
}

void EnsureLoaded()
{
    if (!s_loaded) {
        Load(FAVORITES_FILE, s_favorites);
        Load(HISTORY_FILE, s_history);
        s_loaded = true;
    }
}

int Find(const std::vector<Entry>& list, const char *address)
{
    for (size_t i = 0; i < list.size(); i++) {
        if (!Q_stricmp(list[i].address.c_str(), address)) {
            return (int)i;
        }
    }
    return -1;
}

// The connected server, or the argument
bool CommandAddress(std::string& address)
{
    if (Cmd_Argc() >= 2) {
        address = Cmd_Argv(1);
        if (!strchr(address.c_str(), ':')) {
            address += ":12203";
        }
        return true;
    }
    if (clc.state >= CA_CONNECTED && !clc.demoplaying && clc.serverAddress.type != NA_LOOPBACK) {
        address = NET_AdrToStringwPort(clc.serverAddress);
        return true;
    }
    return false;
}

void FavAdd_f()
{
    std::string address;

    EnsureLoaded();
    if (!CommandAddress(address)) {
        Com_Printf("Usage: fav_add [ip:port [query port]] (no address: the server you are on)\n");
        return;
    }
    if (Find(s_favorites, address.c_str()) >= 0) {
        Com_Printf("%s is already a favorite\n", address.c_str());
        return;
    }
    int queryPort = Cmd_Argc() >= 3 ? atoi(Cmd_Argv(2)) : 0;
    if (queryPort <= 0) {
        queryPort = !Q_stricmp(s_lastSelected.address.c_str(), address.c_str()) ? s_lastSelected.queryPort
                                                                                 : DefaultQueryPort(address);
    }
    s_favorites.push_back({address, queryPort});
    Save(FAVORITES_FILE, s_favorites);
    Com_Printf("Added favorite %s\n", address.c_str());
}

void FavDel_f()
{
    std::string address;
    int         index;

    EnsureLoaded();
    if (!CommandAddress(address)) {
        Com_Printf("Usage: fav_del [ip:port]\n");
        return;
    }
    index = Find(s_favorites, address.c_str());
    if (index < 0) {
        Com_Printf("%s is not a favorite\n", address.c_str());
        return;
    }
    s_favorites.erase(s_favorites.begin() + index);
    Save(FAVORITES_FILE, s_favorites);
    Com_Printf("Removed favorite %s\n", address.c_str());
}

void PrintList(const char *title, const std::vector<Entry>& list)
{
    Com_Printf("%s (%d):\n", title, (int)list.size());
    for (const Entry& e : list) {
        Com_Printf("  %s (query port %d)\n", e.address.c_str(), e.queryPort);
    }
}

void FavList_f()
{
    EnsureLoaded();
    PrintList("Favorite servers", s_favorites);
}

void HistoryList_f()
{
    EnsureLoaded();
    PrintList("Recent servers", s_history);
}

void HistoryClear_f()
{
    EnsureLoaded();
    s_history.clear();
    Save(HISTORY_FILE, s_history);
    Com_Printf("Server history cleared\n");
}
} // namespace

void CL_ServerBook_Init(void)
{
    Cmd_AddCommand("fav_add", FavAdd_f);
    Cmd_AddCommand("fav_del", FavDel_f);
    Cmd_AddCommand("fav_list", FavList_f);
    Cmd_AddCommand("history_list", HistoryList_f);
    Cmd_AddCommand("history_clear", HistoryClear_f);
}

bool CL_ServerBook_IsFavorite(const char *address)
{
    EnsureLoaded();
    return Find(s_favorites, address) >= 0;
}

bool CL_ServerBook_IsRecent(const char *address)
{
    EnsureLoaded();
    return Find(s_history, address) >= 0;
}

bool CL_ServerBook_ToggleFavorite(const char *address, int queryPort)
{
    const int index = (EnsureLoaded(), Find(s_favorites, address));

    if (index >= 0) {
        s_favorites.erase(s_favorites.begin() + index);
    } else {
        s_favorites.push_back({address, queryPort});
    }
    Save(FAVORITES_FILE, s_favorites);
    return index < 0;
}

void CL_ServerBook_Selected(const char *address, int queryPort)
{
    s_lastSelected = {address, queryPort};
}

void CL_ServerBook_Connected(void)
{
    std::string address;
    int         index;

    if (clc.demoplaying || clc.serverAddress.type == NA_LOOPBACK || clc.serverAddress.type == NA_BOT) {
        return;
    }
    EnsureLoaded();
    address = NET_AdrToStringwPort(clc.serverAddress);
    index   = Find(s_history, address.c_str());

    Entry entry = {address, DefaultQueryPort(address)};
    if (index >= 0) {
        entry = s_history[index];
        s_history.erase(s_history.begin() + index);
    }
    if (!Q_stricmp(s_lastSelected.address.c_str(), address.c_str())) {
        entry.queryPort = s_lastSelected.queryPort;
    } else {
        // OpenMOHAA Plus servers publish their query port
        const int published =
            atoi(Info_ValueForKey(cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO], "net_gamespy_port"));
        if (published > 0) {
            entry.queryPort = published;
        }
    }
    s_history.insert(s_history.begin(), entry);
    if ((int)s_history.size() > MAX_HISTORY) {
        s_history.resize(MAX_HISTORY);
    }
    Save(HISTORY_FILE, s_history);
}

void CL_ServerBook_ForEach(int which, void (*callback)(const char *address, int queryPort, void *data), void *data)
{
    EnsureLoaded();
    for (const Entry& e : which == SERVERBOOK_FAVORITES ? s_favorites : s_history) {
        callback(e.address.c_str(), e.queryPort, data);
    }
}
