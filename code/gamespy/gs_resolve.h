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
