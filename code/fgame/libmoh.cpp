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

// libmoh.cpp -- Added in OpenMOHAA Plus: extended script functions
//
// create_api_request, json_parse and json_stringify, compatible with NightFall's functions of the
// same names (handler arguments, return codes, typed JSON for json_stringify and POST bodies).
// HTTP runs in the engine (gi.HTTP_*); responses are delivered one per server frame.

#include "g_local.h"
#include "scriptthread.h"
#include "scriptmaster.h"
#include "scriptexception.h"
#include "../qcommon/json.hpp"

#include <map>
#include <vector>

using json = nlohmann::json;

enum {
    CAPIR_SUCCESS    = 0,
    CAPIR_INVALID    = 1,
    CAPIR_NOTRUNNING = 2,
    CAPIR_ERROR      = 3
};

struct LibMohRequest {
    ScriptThreadLabel label;
    ScriptVariable    userData;
};

static std::map<int, LibMohRequest> libmohRequests;
static cvar_t                      *sv_api_client;

static void LibMoh_JsonToVar(const json& j, ScriptVariable& var)
{
    switch (j.type()) {
    case json::value_t::null:
        var.setListenerValue(NULL);
        break;
    case json::value_t::string:
        var.setStringValue(j.get<std::string>().c_str());
        break;
    case json::value_t::boolean:
        var.setIntValue(j.get<bool>() ? 1 : 0);
        break;
    case json::value_t::number_integer:
    case json::value_t::number_unsigned:
        var.setIntValue(j.get<int>());
        break;
    case json::value_t::number_float:
        var.setFloatValue(j.get<float>());
        break;
    case json::value_t::array:
        for (size_t i = 0; i < j.size(); i++) {
            ScriptVariable index, value;
            index.setIntValue((int)i);
            LibMoh_JsonToVar(j[i], value);
            var.setArrayAtRef(index, value);
        }
        break;
    case json::value_t::object:
        for (auto it = j.begin(); it != j.end(); ++it) {
            ScriptVariable index, value;
            index.setStringValue(it.key().c_str());
            LibMoh_JsonToVar(it.value(), value);
            var.setArrayAtRef(index, value);
        }
        break;
    default:
        break;
    }
}

// NightFall's typed format: {"type": ..., "content": ...}
static void LibMoh_VarToJson(ScriptVariable& var, json& j)
{
    switch (var.GetType()) {
    case VARIABLE_STRING:
    case VARIABLE_CONSTSTRING:
        j["type"]    = "string";
        j["content"] = var.stringValue().c_str();
        break;
    case VARIABLE_INTEGER:
        j["type"]    = "integer";
        j["content"] = var.intValue();
        break;
    case VARIABLE_FLOAT:
        j["type"]    = "float";
        j["content"] = var.floatValue();
        break;
    case VARIABLE_CHAR:
        j["type"]    = "char";
        j["content"] = var.charValue();
        break;
    case VARIABLE_ARRAY:
    {
        con_map_enum<ScriptVariable, ScriptVariable> en(var.m_data.arrayValue->arrayValue);
        json                                         content = json::object();
        ScriptVariable                              *key;

        for (key = en.NextKey(); key; key = en.NextKey()) {
            LibMoh_VarToJson(*en.CurrentValue(), content[key->stringValue().c_str()]);
        }
        j["type"]    = "array_object";
        j["content"] = content;
        break;
    }
    case VARIABLE_CONSTARRAY:
    {
        ScriptConstArrayHolder *holder = var.m_data.constArrayValue;

        j["type"]    = "array";
        j["content"] = json::array();
        for (unsigned int i = 0; i < holder->size; i++) {
            LibMoh_VarToJson(holder->constArrayValue[i], j["content"][i]);
        }
        break;
    }
    case VARIABLE_VECTOR:
    {
        const Vector v = var.vectorValue();
        j["type"]         = "vector";
        j["content"]["x"] = v.x;
        j["content"]["y"] = v.y;
        j["content"]["z"] = v.z;
        break;
    }
    case VARIABLE_NONE:
        j["type"]    = "none";
        j["content"] = nullptr;
        break;
    default:
        // containers, listeners, pointers and references cannot be converted
        j["type"]    = "unsupported";
        j["content"] = var.GetTypeName();
        break;
    }
}

void ScriptThread::CreateAPIRequest(Event *ev)
{
    if (!sv_api_client) {
        sv_api_client = gi.Cvar_Get("sv_api_client", "0", 0);
    }
    if (!sv_api_client->integer) {
        ev->AddInteger(CAPIR_NOTRUNNING);
        return;
    }

    if (ev->NumArgs() < 3 || ev->NumArgs() > 4) {
        throw ScriptException("Wrong number of arguments for create_api_request");
    }

    str url    = ev->GetString(1);
    str method = ev->GetString(2);
    method.tolower();
    if (!url.length() || (method != "get" && method != "post")) {
        ev->AddInteger(CAPIR_INVALID);
        return;
    }

    LibMohRequest request;
    request.label.SetThread(ev->GetValue(3));
    if (ev->NumArgs() == 4) {
        request.userData = ev->GetValue(4);
    }

    std::string body;
    if (method == "post") {
        json j;
        LibMoh_VarToJson(request.userData, j);
        body = j.dump();
    }

    const int id = gi.HTTP_Request(url.c_str(), method.c_str(), body.c_str());
    if (id < 0) {
        ev->AddInteger(CAPIR_ERROR);
        return;
    }

    libmohRequests[id] = request;
    ev->AddInteger(CAPIR_SUCCESS);
}

void ScriptThread::JsonParse(Event *ev)
{
    if (ev->NumArgs() != 1) {
        throw ScriptException("Wrong number of arguments for json_parse");
    }

    const json j = json::parse(ev->GetString(1).c_str(), nullptr, false);
    if (j.is_discarded()) {
        throw ScriptException("json_parse failed: invalid JSON");
    }

    ScriptVariable result;
    LibMoh_JsonToVar(j, result);
    ev->AddValue(result);
}

void ScriptThread::JsonStringify(Event *ev)
{
    if (ev->NumArgs() != 1) {
        throw ScriptException("Wrong number of arguments for json_stringify");
    }

    json j;
    LibMoh_VarToJson(ev->GetValue(1), j);
    ev->AddString(j.dump().c_str());
}

/*
====================
LibMoh_RunFrame

Delivers at most one HTTP response per server frame to its script handler:
handler local.http_code local.response local.user_data (http_code is -1 on error).
====================
*/
void LibMoh_RunFrame(void)
{
    static char buffer[1024 * 1024 + 1];
    int         id, code;

    if (!gi.HTTP_PollResponse || !gi.HTTP_PollResponse(&id, &code, buffer, sizeof(buffer))) {
        return;
    }

    auto it = libmohRequests.find(id);
    if (it == libmohRequests.end()) {
        return; // made before the last script reset
    }

    LibMohRequest request = it->second;
    libmohRequests.erase(it);

    Event ev;
    ev.AddInteger(code);
    ev.AddString(buffer);
    ev.AddValue(request.userData);

    try {
        request.label.Execute(NULL, ev);
    } catch (const ScriptException& e) {
        gi.Printf("create_api_request handler: %s\n", e.string.c_str());
    }
}

/*
====================
LibMoh_Reset

Scripts are being freed (map change or restart): forget pending requests.
====================
*/
void LibMoh_Reset(void)
{
    libmohRequests.clear();
    if (gi.HTTP_Clear) {
        gi.HTTP_Clear();
    }
}
