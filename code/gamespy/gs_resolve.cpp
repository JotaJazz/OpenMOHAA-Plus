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

// gs_resolve.cpp: Added in OpenMOHAA Plus, see gs_resolve.h

#include "gs_resolve.h"

#include <chrono>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#    include <winsock2.h>
#    include <ws2tcpip.h>
#else
#    include <netdb.h>
#    include <netinet/in.h>
#    include <sys/socket.h>
#endif

namespace
{
enum State {
    PENDING,
    RESOLVED,
    FAILED
};

struct Entry {
    State        state;
    unsigned int address;   // last good address, kept while a refresh runs
    bool         hasAddress;
    double       time;      // when the last lookup finished
};

const double RETRY_FAILED_AFTER = 30;    // seconds
const double REFRESH_AFTER      = 600;   // seconds

// Never destroyed: lookup threads may still run when the program exits
std::mutex&                    Lock = *new std::mutex;
std::map<std::string, Entry>& Cache = *new std::map<std::string, Entry>;

double Now()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void Resolve(std::string host)
{
    struct addrinfo  hints;
    struct addrinfo *result = NULL;
    bool             ok;
    unsigned int     address = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    ok                = getaddrinfo(host.c_str(), NULL, &hints, &result) == 0 && result;
    if (ok) {
        address = ((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
        freeaddrinfo(result);
    }

    std::lock_guard<std::mutex> guard(Lock);
    Entry&                      e = Cache[host];
    e.time                        = Now();
    if (ok) {
        e.state      = RESOLVED;
        e.address    = address;
        e.hasAddress = true;
    } else {
        e.state = e.hasAddress ? RESOLVED : FAILED; // keep using the old address if there is one
    }
}
} // namespace

int GS_LookupHost(const char *host, unsigned int *address)
{
    std::lock_guard<std::mutex> guard(Lock);
    auto                        it  = Cache.find(host);
    const double                now = Now();

    if (it == Cache.end() || (it->second.state == FAILED && now - it->second.time > RETRY_FAILED_AFTER)
        || (it->second.state == RESOLVED && now - it->second.time > REFRESH_AFTER)) {
        Entry& e = Cache[host];
        if (it == Cache.end()) {
            e.hasAddress = false;
            e.address    = 0;
        }
        e.state = PENDING;
        e.time  = now;
        std::thread(Resolve, std::string(host)).detach();
        it = Cache.find(host);
    }

    if (it->second.hasAddress) {
        *address = it->second.address;
        return 1;
    }
    return it->second.state == FAILED ? -1 : 0;
}
