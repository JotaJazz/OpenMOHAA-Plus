// Used by tests/test_client.py (ServerBrowser): LD_PRELOAD fault injection, Linux only.
// Fault injection for tests: looking up a host name takes 5 s and fails, like an unreachable DNS
// server. Numeric addresses still go to the real resolver (they never need DNS).
#define _GNU_SOURCE
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>
#include <arpa/inet.h>
static int numeric(const char *n) { unsigned char b[16]; return !n || inet_pton(AF_INET, n, b) == 1 || inet_pton(AF_INET6, n, b) == 1; }
static void note(const char *f, const char *n) { fprintf(stderr, "SLOWDNS %s(%s)\n", f, n); }
struct hostent *gethostbyname(const char *name) {
    if (numeric(name)) { struct hostent *(*real)(const char *) = dlsym(RTLD_NEXT, "gethostbyname"); return real(name); }
    note("gethostbyname", name); sleep(5); h_errno = TRY_AGAIN; return NULL;
}
int getaddrinfo(const char *n, const char *s, const struct addrinfo *h, struct addrinfo **r) {
    if (numeric(n)) { int (*real)(const char *, const char *, const struct addrinfo *, struct addrinfo **) = dlsym(RTLD_NEXT, "getaddrinfo"); return real(n, s, h, r); }
    note("getaddrinfo", n); sleep(5); return EAI_AGAIN;
}
