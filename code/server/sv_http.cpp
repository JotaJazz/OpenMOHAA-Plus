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

// sv_http.cpp -- Added in OpenMOHAA Plus: asynchronous HTTP requests for game scripts
//
// The game module queues requests through gi.HTTP_Request and collects results with
// gi.HTTP_PollResponse once per frame. A single worker thread performs them with the engine's
// curl. Only http and https are allowed (also for redirects), responses are capped and requests
// time out, so a script cannot block the server or read local files.

#include "server.h"
#include "../sys/sys_curl.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

#define SV_HTTP_MAX_RESPONSE (1024 * 1024)
#define SV_HTTP_TIMEOUT      20L

namespace
{
struct HttpRequest {
    int         id;
    std::string url;
    std::string method;
    std::string body;
};

struct HttpResponse {
    int         id;
    int         code; // HTTP status, or -1 on error with the error text as body
    std::string body;
};

std::mutex                 httpMutex;
std::condition_variable    httpWake;
std::deque<HttpRequest>    httpRequests;
std::deque<HttpResponse>   httpResponses;
std::thread               *httpThread = NULL;
std::atomic<bool>          httpStopping(false);
int                        httpNextId = 1;

#ifdef USE_HTTP
size_t SV_HTTP_Write(char *data, size_t size, size_t nmemb, void *user)
{
    std::string *body  = static_cast<std::string *>(user);
    const size_t bytes = size * nmemb;

    if (body->size() + bytes > SV_HTTP_MAX_RESPONSE) {
        return 0; // abort: response too large
    }
    body->append(data, bytes);
    return bytes;
}

int SV_HTTP_Progress(void *, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    // abort transfers when the server shuts down
    return httpStopping.load() ? 1 : 0;
}

HttpResponse SV_HTTP_Perform(const HttpRequest& req)
{
    HttpResponse       resp;
    struct curl_slist *headers = NULL;
    CURL              *curl;
    CURLcode           result;
    long               status = 0;

    resp.id   = req.id;
    resp.code = -1;

    curl = curlImport.qcurl_easy_init();
    if (!curl) {
        resp.body = "curl initialization failed";
        return resp;
    }

    curlImport.qcurl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
    curlImport.qcurl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
    curlImport.qcurl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    curlImport.qcurl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curlImport.qcurl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curlImport.qcurl_easy_setopt(curl, CURLOPT_TIMEOUT, SV_HTTP_TIMEOUT);
    curlImport.qcurl_easy_setopt(curl, CURLOPT_USERAGENT, "OpenMOHAA-Plus");
    curlImport.qcurl_easy_setopt(curl, CURLOPT_MAXFILESIZE, (long)SV_HTTP_MAX_RESPONSE);
    curlImport.qcurl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, SV_HTTP_Write);
    curlImport.qcurl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.body);
    curlImport.qcurl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curlImport.qcurl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, SV_HTTP_Progress);
    curlImport.qcurl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    if (req.method == "post") {
        headers = curlImport.qcurl_slist_append(headers, "Content-Type: application/json");
        curlImport.qcurl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curlImport.qcurl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)req.body.size());
        curlImport.qcurl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, req.body.c_str());
    }

    result = curlImport.qcurl_easy_perform(curl);
    if (result == CURLE_OK) {
        curlImport.qcurl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        resp.code = (int)status;
    } else {
        resp.body = curlImport.qcurl_easy_strerror(result);
    }

    if (headers) {
        curlImport.qcurl_slist_free_all(headers);
    }
    curlImport.qcurl_easy_cleanup(curl);
    return resp;
}
#endif

void SV_HTTP_Worker()
{
    for (;;) {
        HttpRequest req;
        {
            std::unique_lock<std::mutex> lock(httpMutex);
            httpWake.wait(lock, [] { return httpStopping.load() || !httpRequests.empty(); });
            if (httpStopping.load()) {
                return;
            }
            req = httpRequests.front();
            httpRequests.pop_front();
        }

#ifdef USE_HTTP
        HttpResponse resp = SV_HTTP_Perform(req);
#else
        HttpResponse resp = {req.id, -1, "built without HTTP support"};
#endif

        std::lock_guard<std::mutex> lock(httpMutex);
        httpResponses.push_back(resp);
    }
}
} // namespace

/*
====================
SV_HTTP_Request

Queues a request; returns its id (> 0), or -1 when HTTP is unavailable.
method is "get" or "post"; body is sent as JSON for "post".
====================
*/
extern "C" int SV_HTTP_Request(const char *url, const char *method, const char *body)
{
#ifdef USE_HTTP
    if (!Com_IsCurlImportValid(&curlImport)) {
        return -1;
    }
#else
    return -1;
#endif

    std::lock_guard<std::mutex> lock(httpMutex);

    if (!httpThread) {
        httpStopping.store(false);
        httpThread = new std::thread(SV_HTTP_Worker);
    }

    HttpRequest req;
    req.id     = httpNextId++;
    req.url    = url ? url : "";
    req.method = method ? method : "get";
    req.body   = body ? body : "";
    httpRequests.push_back(req);
    httpWake.notify_one();
    return req.id;
}

/*
====================
SV_HTTP_PollResponse

Pops one finished request. buffer receives the response body (or the error text when code is -1),
truncated to bufferSize - 1.
====================
*/
extern "C" qboolean SV_HTTP_PollResponse(int *id, int *code, char *buffer, int bufferSize)
{
    std::lock_guard<std::mutex> lock(httpMutex);

    if (httpResponses.empty()) {
        return qfalse;
    }

    const HttpResponse& resp = httpResponses.front();
    *id                      = resp.id;
    *code                    = resp.code;
    Q_strncpyz(buffer, resp.body.c_str(), bufferSize);
    httpResponses.pop_front();
    return qtrue;
}

/*
====================
SV_HTTP_Clear

Drops queued requests and undelivered responses (map change). A request being performed still
completes, and the game ignores its unknown id.
====================
*/
extern "C" void SV_HTTP_Clear(void)
{
    std::lock_guard<std::mutex> lock(httpMutex);
    httpRequests.clear();
    httpResponses.clear();
}

extern "C" void SV_HTTP_Shutdown(void)
{
    {
        std::lock_guard<std::mutex> lock(httpMutex);
        if (!httpThread) {
            return;
        }
        httpStopping.store(true);
        httpRequests.clear();
        httpResponses.clear();
    }
    httpWake.notify_all();
    httpThread->join();
    delete httpThread;
    httpThread = NULL;
}
