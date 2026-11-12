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

// sv_admin.cpp: Added in OpenMOHAA Plus
//  In-game administration (upstream issue #905): admin accounts, roles with
//  allow/deny permissions, ad_* wrappers around existing console commands.
//  RCON is unchanged.
//
//  admin_accounts.txt (written by the server):
//    account <name> <enabled 0|1> pbkdf2-sha256$<iterations>$<salt hex>$<hash hex>
//  admin_config.txt (written by hand, read at startup):
//    role <role> allow|deny <permission>...
//    account <name> roles <role>...
//    account <name> allow|deny <permission>...
//  Permissions: exact names, a trailing * prefix wildcard, or *. Deny always wins.

#include "server.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

//
// SHA-256 (FIPS 180-4), HMAC (RFC 2104) and PBKDF2 (RFC 8018)
//

namespace
{
struct Sha256 {
    uint32_t h[8];
    uint8_t  buf[64];
    uint64_t length;
    size_t   used;
};

const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

inline uint32_t Ror(uint32_t x, int n)
{
    return (x >> n) | (x << (32 - n));
}

void Sha256_Block(Sha256& s, const uint8_t *p)
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    int      i;

    for (i = 0; i < 16; i++) {
        w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    }
    for (; i < 64; i++) {
        const uint32_t s0 = Ror(w[i - 15], 7) ^ Ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 = Ror(w[i - 2], 17) ^ Ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i]              = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = s.h[0], b = s.h[1], c = s.h[2], d = s.h[3], e = s.h[4], f = s.h[5], g = s.h[6], h = s.h[7];
    for (i = 0; i < 64; i++) {
        const uint32_t t1 = h + (Ror(e, 6) ^ Ror(e, 11) ^ Ror(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
        const uint32_t t2 = (Ror(a, 2) ^ Ror(a, 13) ^ Ror(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        h                 = g;
        g                 = f;
        f                 = e;
        e                 = d + t1;
        d                 = c;
        c                 = b;
        b                 = a;
        a                 = t1 + t2;
    }
    s.h[0] += a, s.h[1] += b, s.h[2] += c, s.h[3] += d, s.h[4] += e, s.h[5] += f, s.h[6] += g, s.h[7] += h;
}

void Sha256_Init(Sha256& s)
{
    static const uint32_t iv[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };
    memcpy(s.h, iv, sizeof(iv));
    s.length = 0;
    s.used   = 0;
}

void Sha256_Update(Sha256& s, const uint8_t *data, size_t len)
{
    s.length += len;
    while (len) {
        const size_t n = len < 64 - s.used ? len : 64 - s.used;
        memcpy(s.buf + s.used, data, n);
        s.used += n;
        data += n;
        len -= n;
        if (s.used == 64) {
            Sha256_Block(s, s.buf);
            s.used = 0;
        }
    }
}

void Sha256_Final(Sha256& s, uint8_t out[32])
{
    const uint64_t bits = s.length * 8;
    uint8_t        pad  = 0x80;
    uint8_t        lenBytes[8];
    int            i;

    Sha256_Update(s, &pad, 1);
    pad = 0;
    while (s.used != 56) {
        Sha256_Update(s, &pad, 1);
    }
    for (i = 0; i < 8; i++) {
        lenBytes[i] = (uint8_t)(bits >> (56 - i * 8));
    }
    Sha256_Update(s, lenBytes, 8);
    for (i = 0; i < 8; i++) {
        out[i * 4]     = (uint8_t)(s.h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(s.h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(s.h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)s.h[i];
    }
}

// PBKDF2-HMAC-SHA256 with a 32-byte output (one block)
void Pbkdf2Sha256(const std::string& password, const uint8_t *salt, size_t saltLen, int iterations, uint8_t out[32])
{
    uint8_t key[64] = {0};
    uint8_t ipad[64], opad[64];
    uint8_t u[32];
    Sha256  inner, outer, ctx;
    int     i, j;

    if (password.size() > 64) {
        Sha256_Init(ctx);
        Sha256_Update(ctx, (const uint8_t *)password.data(), password.size());
        Sha256_Final(ctx, key);
    } else {
        memcpy(key, password.data(), password.size());
    }
    for (i = 0; i < 64; i++) {
        ipad[i] = key[i] ^ 0x36;
        opad[i] = key[i] ^ 0x5c;
    }
    Sha256_Init(inner);
    Sha256_Update(inner, ipad, 64);
    Sha256_Init(outer);
    Sha256_Update(outer, opad, 64);

    // U1 = HMAC(password, salt || INT(1))
    static const uint8_t blockIndex[4] = {0, 0, 0, 1};
    ctx = inner;
    Sha256_Update(ctx, salt, saltLen);
    Sha256_Update(ctx, blockIndex, 4);
    Sha256_Final(ctx, u);
    ctx = outer;
    Sha256_Update(ctx, u, 32);
    Sha256_Final(ctx, u);
    memcpy(out, u, 32);

    for (i = 1; i < iterations; i++) {
        ctx = inner;
        Sha256_Update(ctx, u, 32);
        Sha256_Final(ctx, u);
        ctx = outer;
        Sha256_Update(ctx, u, 32);
        Sha256_Final(ctx, u);
        for (j = 0; j < 32; j++) {
            out[j] ^= u[j];
        }
    }
}

std::string ToHex(const uint8_t *p, size_t len)
{
    static const char digits[] = "0123456789abcdef";
    std::string       s;

    for (size_t i = 0; i < len; i++) {
        s += digits[p[i] >> 4];
        s += digits[p[i] & 15];
    }
    return s;
}

bool FromHex(const std::string& s, std::vector<uint8_t>& out)
{
    out.clear();
    if (s.size() % 2) {
        return false;
    }
    for (size_t i = 0; i < s.size(); i += 2) {
        int v = 0;
        for (int k = 0; k < 2; k++) {
            const char c = s[i + k];
            v <<= 4;
            if (c >= '0' && c <= '9') {
                v |= c - '0';
            } else if (c >= 'a' && c <= 'f') {
                v |= c - 'a' + 10;
            } else {
                return false;
            }
        }
        out.push_back((uint8_t)v);
    }
    return true;
}

const int PBKDF2_ITERATIONS = 100000;

std::string HashPassword(const std::string& password)
{
    uint8_t salt[16], hash[32];

    if (!Sys_RandomBytes(salt, sizeof(salt))) {
        Com_RandomBytes(salt, sizeof(salt));
    }
    Pbkdf2Sha256(password, salt, sizeof(salt), PBKDF2_ITERATIONS, hash);
    return va("pbkdf2-sha256$%d$%s$%s", PBKDF2_ITERATIONS, ToHex(salt, 16).c_str(), ToHex(hash, 32).c_str());
}

bool VerifyPassword(const std::string& stored, const std::string& password)
{
    char                 scheme[32];
    int                  iterations;
    char                 saltHex[129], hashHex[129];
    std::vector<uint8_t> salt, expected;
    uint8_t              hash[32];
    uint8_t              diff = 0;

    if (sscanf(stored.c_str(), "%31[^$]$%d$%128[^$]$%128s", scheme, &iterations, saltHex, hashHex) != 4
        || strcmp(scheme, "pbkdf2-sha256") || iterations < 1 || iterations > 10000000 || !FromHex(saltHex, salt)
        || !FromHex(hashHex, expected) || expected.size() != 32) {
        return false;
    }
    Pbkdf2Sha256(password, salt.data(), salt.size(), iterations, hash);
    for (int i = 0; i < 32; i++) {
        diff |= hash[i] ^ expected[i];
    }
    return diff == 0;
}

//
// Accounts, policy and sessions
//

struct Account {
    std::string name;
    bool        enabled;
    std::string hash;
};

struct Rules {
    std::vector<std::string> roles;
    std::vector<std::string> allow;
    std::vector<std::string> deny;
};

struct Throttle {
    int failures;
    int nextTime;
};

std::vector<Account>            s_accounts;
std::map<std::string, Rules>    s_roles;          // lower-case role name
std::map<std::string, Rules>    s_accountRules;   // lower-case account name
std::string                     s_sessions[MAX_CLIENTS]; // account name, empty when not logged in
std::map<std::string, Throttle> s_throttle;       // by address
int                             s_nextVerifyTime; // a hash costs ~50 ms of server frame: at most one per second

const char *ACCOUNTS_FILE = "admin_accounts.txt";
const char *CONFIG_FILE   = "admin_config.txt";
const char *LOG_FILE      = "admin.log";

std::string Lower(std::string s)
{
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c += 'a' - 'A';
        }
    }
    return s;
}

bool ValidName(const char *name)
{
    size_t len = strlen(name);

    if (!len || len > 32) {
        return false;
    }
    for (; *name; name++) {
        if (!((*name >= 'a' && *name <= 'z') || (*name >= 'A' && *name <= 'Z') || (*name >= '0' && *name <= '9')
              || *name == '_' || *name == '-' || *name == '.')) {
            return false;
        }
    }
    return true;
}

Account *FindAccount(const std::string& name)
{
    for (Account& a : s_accounts) {
        if (!Q_stricmp(a.name.c_str(), name.c_str())) {
            return &a;
        }
    }
    return NULL;
}

bool ReadFile(const char *name, std::string& out)
{
    char         path[MAX_QPATH];
    fileHandle_t f;
    long         len;

    Com_sprintf(path, sizeof(path), "%s/%s", FS_GetCurrentGameDir(), name);
    len = FS_BaseDir_FOpenFileRead(path, &f);
    if (len < 0) {
        return false;
    }
    out.resize(len);
    if (len) {
        FS_Read(&out[0], len, f);
    }
    FS_FCloseFile(f);
    return true;
}

// Splits a file into lines of whitespace-separated words, skipping comments
std::vector<std::vector<std::string>> ParseLines(const std::string& text)
{
    std::vector<std::vector<std::string>> lines;
    size_t                                pos = 0;

    while (pos < text.size()) {
        size_t                   end  = text.find('\n', pos);
        std::string              line = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        std::vector<std::string> words;
        size_t                   i = 0;

        pos = end == std::string::npos ? text.size() : end + 1;
        if (line.find("//") != std::string::npos) {
            line.erase(line.find("//"));
        }
        if (line.find('#') != std::string::npos) {
            line.erase(line.find('#'));
        }
        while (i < line.size()) {
            while (i < line.size() && strchr(" \t\r", line[i])) {
                i++;
            }
            size_t start = i;
            while (i < line.size() && !strchr(" \t\r", line[i])) {
                i++;
            }
            if (i > start) {
                words.push_back(line.substr(start, i - start));
            }
        }
        if (!words.empty()) {
            lines.push_back(words);
        }
    }
    return lines;
}

void LoadAccounts()
{
    std::string text;

    s_accounts.clear();
    if (!ReadFile(ACCOUNTS_FILE, text)) {
        return;
    }
    for (const auto& w : ParseLines(text)) {
        if (w.size() == 4 && w[0] == "account" && ValidName(w[1].c_str())) {
            s_accounts.push_back({w[1], w[2] == "1", w[3]});
        } else {
            Com_Printf("%s: ignoring a malformed line\n", ACCOUNTS_FILE);
        }
    }
}

void SaveAccounts()
{
    char         path[MAX_QPATH];
    fileHandle_t f;

    Com_sprintf(path, sizeof(path), "%s/%s", FS_GetCurrentGameDir(), ACCOUNTS_FILE);
    f = FS_BaseDir_FOpenFileWrite_HomeState(path);
    if (!f) {
        Com_Printf("Couldn't write %s\n", ACCOUNTS_FILE);
        return;
    }
    FS_Printf(f, "// OpenMOHAA Plus admin accounts, written by the server (admin_add, admin_passwd...).\n");
    for (const Account& a : s_accounts) {
        FS_Printf(f, "account %s %d %s\n", a.name.c_str(), a.enabled ? 1 : 0, a.hash.c_str());
    }
    FS_FCloseFile(f);
}

void LoadConfig()
{
    std::string text;

    s_roles.clear();
    s_accountRules.clear();
    if (!ReadFile(CONFIG_FILE, text)) {
        return;
    }
    for (const auto& w : ParseLines(text)) {
        Rules *rules = NULL;

        if (w.size() >= 3 && w[0] == "role") {
            rules = &s_roles[Lower(w[1])];
        } else if (w.size() >= 3 && w[0] == "account") {
            rules = &s_accountRules[Lower(w[1])];
        }
        if (!rules || (w[2] != "allow" && w[2] != "deny" && (w[2] != "roles" || w[0] != "account"))) {
            Com_Printf("%s: ignoring line starting with '%s %s'\n", CONFIG_FILE, w[0].c_str(), w.size() > 1 ? w[1].c_str() : "");
            continue;
        }
        std::vector<std::string>& list = w[2] == "allow" ? rules->allow : (w[2] == "deny" ? rules->deny : rules->roles);
        for (size_t i = 3; i < w.size(); i++) {
            list.push_back(Lower(w[i]));
        }
    }
    for (const auto& acc : s_accountRules) {
        for (const std::string& role : acc.second.roles) {
            if (!s_roles.count(role)) {
                Com_Printf("%s: account '%s' uses undefined role '%s'\n", CONFIG_FILE, acc.first.c_str(), role.c_str());
            }
        }
    }
}

bool PatternMatches(const std::string& pattern, const std::string& permission)
{
    if (pattern == "*") {
        return true;
    }
    if (!pattern.empty() && pattern.back() == '*') {
        return !permission.compare(0, pattern.size() - 1, pattern, 0, pattern.size() - 1);
    }
    return pattern == permission;
}

// allowed = role allows + account allows, denied = role denies + account denies; deny wins
bool HasPermission(const std::string& account, const std::string& permissionName)
{
    const std::string                     permission = Lower(permissionName);
    std::vector<const Rules *>            sets;
    std::map<std::string, Rules>::iterator it = s_accountRules.find(Lower(account));
    bool                                  allowed = false;

    if (it == s_accountRules.end()) {
        return false;
    }
    sets.push_back(&it->second);
    for (const std::string& role : it->second.roles) {
        if (s_roles.count(role)) {
            sets.push_back(&s_roles[role]);
        }
    }
    for (const Rules *r : sets) {
        for (const std::string& p : r->deny) {
            if (PatternMatches(p, permission)) {
                return false;
            }
        }
        for (const std::string& p : r->allow) {
            allowed = allowed || PatternMatches(p, permission);
        }
    }
    return allowed;
}

void EndSessions(const std::string& account, const char *reason)
{
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!s_sessions[i].empty() && !Q_stricmp(s_sessions[i].c_str(), account.c_str())) {
            s_sessions[i].clear();
            if (i < sv_maxclients->integer && svs.clients[i].state >= CS_CONNECTED) {
                SV_SendServerCommand(&svs.clients[i], "print \"Admin session ended: %s\n\"", reason);
            }
        }
    }
}

void AuditLog(client_t *cl, const char *account, const char *action, const char *result)
{
    qtime_t      t;
    fileHandle_t f;

    Com_RealTime(&t);
    f = FS_FOpenFileAppend_HomeData(LOG_FILE);
    if (!f) {
        return;
    }
    FS_Printf(
        f,
        "%04d-%02d-%02d %02d:%02d:%02d %s \"%s\" account=%s %s: %s\n",
        t.tm_year + 1900,
        t.tm_mon + 1,
        t.tm_mday,
        t.tm_hour,
        t.tm_min,
        t.tm_sec,
        cl ? NET_AdrToString(cl->netchan.remoteAddress) : "console",
        cl ? cl->name : "",
        account && *account ? account : "-",
        result,
        action
    );
    FS_FCloseFile(f);
}

void Print(client_t *cl, const char *text)
{
    std::string s = text;

    for (char& c : s) {
        if (c == '"') {
            c = '\''; // a double quote would end the print command
        }
    }
    SV_SendServerCommand(cl, "print \"%s\n\"", s.c_str());
}

client_t *s_redirectClient;

void FlushToClient(char *text)
{
    for (char *p = text; *p; p++) {
        if (*p == '"') {
            *p = '\'';
        }
    }
    if (*text) {
        SV_SendServerCommand(s_redirectClient, "print \"%s\"", text);
    }
}

// Commands that reload the level must not run in the middle of a client message
bool MustDefer(const char *command)
{
    static const char *deferred[] = {"map", "devmap", "spmap", "spdevmap", "gamemap", "restart", "reloadmap",
                                     "killserver", "quit", "loadgame", "loadlastgame", NULL};

    for (int i = 0; deferred[i]; i++) {
        if (!Q_stricmp(command, deferred[i])) {
            return true;
        }
    }
    return false;
}

// Rebuilds a console line from the client's arguments; refuses characters that would start
// another command
bool BuildLine(const char *command, int firstArg, std::string& line)
{
    line = command;
    for (int i = firstArg; i < Cmd_Argc(); i++) {
        const char *arg = Cmd_Argv(i);
        if (strpbrk(arg, ";\"\n\r")) {
            return false;
        }
        line += strchr(arg, ' ') || !*arg ? va(" \"%s\"", arg) : va(" %s", arg);
    }
    return true;
}

void RunConsoleLine(client_t *cl, const std::string& line, const char *command)
{
    static char buffer[1000];

    if (MustDefer(command)) {
        Cbuf_AddText(va("%s\n", line.c_str()));
        return;
    }
    s_redirectClient = cl;
    Com_BeginRedirect(buffer, sizeof(buffer), FlushToClient);
    Cmd_ExecuteString(line.c_str());
    Com_EndRedirect();
}

struct AdminCommand {
    const char *name;
    const char *console;
    const char *permission;
    const char *usage;
};

const AdminCommand s_commands[] = {
    {"ad_status",     "status",     "admin.status",     ""                                    },
    {"ad_kick",       "kick",       "admin.kick",       "<player name>"                       },
    {"ad_clientkick", "clientkick", "admin.clientkick", "<client number> [reason]"            },
    {"ad_ban",        "banaddr",    "admin.ban",        "<ip[/subnet] | client number> [reason]"},
    {"ad_unban",      "bandel",     "admin.unban",      "<ip[/subnet] | ban number>"          },
    {"ad_listbans",   "listbans",   "admin.listbans",   ""                                    },
    {"ad_map",        "map",        "admin.map",        "<map>"                               },
    {"ad_restart",    "restart",    "admin.restart",    ""                                    },
    {"ad_pause",      "matchpause", "admin.pause",      ""                                    },
    {"ad_resume",     "matchresume","admin.pause",      ""                                    },
    {"ad_say",        "say",        "admin.say",        "<message>"                           },
    {"ad_tell",       "tell",       "admin.tell",       "<client number> <message>"           },
    {"ad_dumpuser",   "dumpuser",   "admin.dumpuser",   "<player name>"                       },
    {NULL,            NULL,         NULL,               NULL                                  }
};

// Cvars that grant more than the cvar itself; only admin.rcon may change them
bool IsSensitiveCvar(const char *name)
{
    static const char *sensitive[] = {"rconPassword", "rcon_password", "sv_privatePassword", "sv_banFile", NULL};

    for (int i = 0; sensitive[i]; i++) {
        if (!Q_stricmp(name, sensitive[i])) {
            return true;
        }
    }
    return false;
}

void Login(client_t *cl, int clientNum)
{
    const std::string address = NET_AdrToString(cl->netchan.remoteAddress);
    Throttle&         throttle = s_throttle[address];
    const int         now      = Sys_Milliseconds();
    Account          *account;
    std::string       name;

    if (Cmd_Argc() != 3) {
        Print(cl, "Usage: ad_login <account> <password>");
        return;
    }
    name = Cmd_Argv(1);
    if (throttle.failures && now < throttle.nextTime) {
        Print(cl, va("Too many failed logins, wait %d seconds", (throttle.nextTime - now + 999) / 1000));
        AuditLog(cl, name.c_str(), "ad_login", "throttled");
        return;
    }

    account = FindAccount(name);
    if (account && account->enabled && now < s_nextVerifyTime) {
        Print(cl, "Server busy, try again in a second");
        return;
    }
    if (account && account->enabled) {
        s_nextVerifyTime = now + 1000;
    }
    if (!account || !account->enabled || !VerifyPassword(account->hash, Cmd_Argv(2))) {
        throttle.failures++;
        throttle.nextTime = now + (throttle.failures >= 5 ? 60000 : 2000);
        Print(cl, "Login failed");
        AuditLog(cl, name.c_str(), "ad_login", "failed");
        return;
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != clientNum && !Q_stricmp(s_sessions[i].c_str(), account->name.c_str())) {
            Print(cl, "Login failed: this account is already in use");
            AuditLog(cl, name.c_str(), "ad_login", "in use");
            return;
        }
    }

    s_throttle.erase(address);
    s_sessions[clientNum] = account->name;
    Print(cl, va("Logged in as %s; ad_help lists your commands", account->name.c_str()));
    AuditLog(cl, account->name.c_str(), "ad_login", "ok");
}

void Help(client_t *cl, const std::string& account)
{
    std::string text = "Admin commands for " + account + ": ad_logout";

    for (const AdminCommand *c = s_commands; c->name; c++) {
        if (HasPermission(account, c->permission)) {
            text += std::string(" ") + c->name;
        }
    }
    text += " ad_set (cvar.<name>)";
    if (HasPermission(account, "admin.rcon")) {
        text += " ad_rcon";
    }
    Print(cl, text.c_str());
}

void SetCvar(client_t *cl, const std::string& account)
{
    const char *name;
    std::string action;

    if (Cmd_Argc() < 3) {
        Print(cl, "Usage: ad_set <cvar> <value>");
        return;
    }
    name   = Cmd_Argv(1);
    action = va("ad_set %s %s", name, Cmd_ArgsFrom(2));
    if (!HasPermission(account, va("cvar.%s", name))
        || (IsSensitiveCvar(name) && !HasPermission(account, "admin.rcon"))) {
        Print(cl, va("Permission denied: cvar.%s", name));
        AuditLog(cl, account.c_str(), action.c_str(), "denied");
        return;
    }
    if (!Cvar_FindVar(name)) {
        Print(cl, va("Unknown cvar %s", name));
        return;
    }
    // not forced: read-only, init and cheat-protected cvars keep their rules
    Cvar_Set2(name, Cmd_ArgsFrom(2), qfalse);
    Print(cl, va("%s is \"%s\"", name, Cvar_VariableString(name)));
    AuditLog(cl, account.c_str(), action.c_str(), "ok");
}
} // namespace

//
// Entry points
//

qboolean SV_Admin_ClientCommand(client_t *cl)
{
    const char  *cmd       = Cmd_Argv(0);
    const int    clientNum = cl - svs.clients;
    std::string  account;
    std::string  line;

    if (Q_stricmpn(cmd, "ad_", 3)) {
        return qfalse;
    }
    if (cl->netchan.remoteAddress.type == NA_BOT) {
        return qtrue;
    }
    if (!Q_stricmp(cmd, "ad_login")) {
        Login(cl, clientNum);
        return qtrue;
    }

    account = s_sessions[clientNum];
    if (account.empty()) {
        Print(cl, "Not logged in: ad_login <account> <password>");
        return qtrue;
    }
    if (!Q_stricmp(cmd, "ad_logout")) {
        s_sessions[clientNum].clear();
        Print(cl, "Logged out");
        AuditLog(cl, account.c_str(), "ad_logout", "ok");
        return qtrue;
    }
    if (!Q_stricmp(cmd, "ad_help")) {
        Help(cl, account);
        return qtrue;
    }
    if (!Q_stricmp(cmd, "ad_set")) {
        SetCvar(cl, account);
        return qtrue;
    }

    if (!Q_stricmp(cmd, "ad_rcon")) {
        if (!HasPermission(account, "admin.rcon")) {
            Print(cl, "Permission denied: admin.rcon");
            AuditLog(cl, account.c_str(), va("ad_rcon %s", Cmd_ArgsFrom(1)), "denied");
            return qtrue;
        }
        if (Cmd_Argc() < 2 || !BuildLine(Cmd_Argv(1), 2, line)) {
            Print(cl, "Usage: ad_rcon <command> [arguments] (no ; or quotes)");
            return qtrue;
        }
        AuditLog(cl, account.c_str(), line.c_str(), "ok");
        RunConsoleLine(cl, line, Cmd_Argv(1));
        return qtrue;
    }

    for (const AdminCommand *c = s_commands; c->name; c++) {
        if (Q_stricmp(cmd, c->name)) {
            continue;
        }
        if (!HasPermission(account, c->permission)) {
            Print(cl, va("Permission denied: %s", c->permission));
            AuditLog(cl, account.c_str(), va("%s %s", cmd, Cmd_ArgsFrom(1)), "denied");
            return qtrue;
        }
        if (!BuildLine(c->console, 1, line)) {
            Print(cl, va("Usage: %s %s (no ; or quotes)", c->name, c->usage));
            return qtrue;
        }
        AuditLog(cl, account.c_str(), line.c_str(), "ok");
        RunConsoleLine(cl, line, c->console);
        return qtrue;
    }

    Print(cl, va("Unknown admin command %s; ad_help lists your commands", cmd));
    return qtrue;
}

void SV_Admin_ClientReset(client_t *cl)
{
    s_sessions[cl - svs.clients].clear();
}

static void SV_Admin_Add_f(void)
{
    const char *name = Cmd_Argv(1);

    if (Cmd_Argc() != 3) {
        Com_Printf("Usage: admin_add <account> <password>\n");
        return;
    }
    if (!ValidName(name)) {
        Com_Printf("Account names use 1-32 letters, digits, '_', '-' or '.'\n");
        return;
    }
    if (FindAccount(name)) {
        Com_Printf("Account %s already exists\n", name);
        return;
    }
    s_accounts.push_back({name, true, HashPassword(Cmd_Argv(2))});
    SaveAccounts();
    AuditLog(NULL, name, "admin_add", "ok");
    Com_Printf("Account %s created; give it roles or permissions in %s\n", name, CONFIG_FILE);
}

static Account *ConsoleAccount(int argc, const char *usage)
{
    Account *account;

    if (Cmd_Argc() != argc) {
        Com_Printf("Usage: %s\n", usage);
        return NULL;
    }
    account = FindAccount(Cmd_Argv(1));
    if (!account) {
        Com_Printf("No account %s\n", Cmd_Argv(1));
    }
    return account;
}

static void SV_Admin_Passwd_f(void)
{
    Account *account = ConsoleAccount(3, "admin_passwd <account> <password>");

    if (account) {
        account->hash = HashPassword(Cmd_Argv(2));
        SaveAccounts();
        EndSessions(account->name, "password changed");
        AuditLog(NULL, account->name.c_str(), "admin_passwd", "ok");
        Com_Printf("Password of %s changed\n", account->name.c_str());
    }
}

static void SV_Admin_Enable(bool enable)
{
    Account *account = ConsoleAccount(2, enable ? "admin_enable <account>" : "admin_disable <account>");

    if (account) {
        account->enabled = enable;
        SaveAccounts();
        if (!enable) {
            EndSessions(account->name, "account disabled");
        }
        AuditLog(NULL, account->name.c_str(), enable ? "admin_enable" : "admin_disable", "ok");
        Com_Printf("Account %s %s\n", account->name.c_str(), enable ? "enabled" : "disabled");
    }
}

static void SV_Admin_Enable_f(void)
{
    SV_Admin_Enable(true);
}

static void SV_Admin_Disable_f(void)
{
    SV_Admin_Enable(false);
}

static void SV_Admin_Del_f(void)
{
    Account *account = ConsoleAccount(2, "admin_del <account>");

    if (account) {
        const std::string name = account->name;
        s_accounts.erase(s_accounts.begin() + (account - s_accounts.data()));
        SaveAccounts();
        EndSessions(name, "account deleted");
        AuditLog(NULL, name.c_str(), "admin_del", "ok");
        Com_Printf("Account %s deleted\n", name.c_str());
    }
}

static void SV_Admin_List_f(void)
{
    for (const Account& a : s_accounts) {
        std::string online;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (!Q_stricmp(s_sessions[i].c_str(), a.name.c_str())) {
                online = va(", logged in as client %d", i);
            }
        }
        Com_Printf("%s (%s%s)\n", a.name.c_str(), a.enabled ? "enabled" : "disabled", online.c_str());
    }
    Com_Printf("%d account(s)\n", (int)s_accounts.size());
}

void SV_Admin_Init(void)
{
    LoadAccounts();
    LoadConfig();
    Cmd_AddCommand("admin_add", SV_Admin_Add_f);
    Cmd_AddCommand("admin_passwd", SV_Admin_Passwd_f);
    Cmd_AddCommand("admin_enable", SV_Admin_Enable_f);
    Cmd_AddCommand("admin_disable", SV_Admin_Disable_f);
    Cmd_AddCommand("admin_del", SV_Admin_Del_f);
    Cmd_AddCommand("admin_list", SV_Admin_List_f);
    if (!s_accounts.empty()) {
        Com_Printf("Admin: %d account(s), %d role(s)\n", (int)s_accounts.size(), (int)s_roles.size());
    }
}
