// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2021 The C_Note developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/protocol.h"

#include "random.h"
#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"
#include "utiltime.h"
#include "version.h"

#include <stdint.h>
#include <fstream>


/**
 * JSON-RPC protocol.  C_Note speaks version 1.0 for maximum compatibility,
 * but uses JSON-RPC 1.1/2.0 standards for parts of the 1.0 standard that were
 * unspecified (HTTP errors and contents of 'error').
 *
 * 1.0 spec: http://json-rpc.org/wiki/specification
 * 1.2 spec: http://jsonrpc.org/historical/json-rpc-over-http.html
 * http://www.codeproject.com/KB/recipes/JSON_Spirit.aspx
 */

UniValue JSONRPCRequestObj(const std::string& strMethod, const UniValue& params, const UniValue& id)
{
    UniValue request(UniValue::VOBJ);
    request.pushKV("method", strMethod);
    request.pushKV("params", params);
    request.pushKV("id", id);
    return request;
}

UniValue JSONRPCReplyObj(const UniValue& result, const UniValue& error, const UniValue& id)
{
    UniValue reply(UniValue::VOBJ);
    if (!error.isNull())
        reply.pushKV("result", NullUniValue);
    else
        reply.pushKV("result", result);
    reply.pushKV("error", error);
    reply.pushKV("id", id);
    return reply;
}

std::string JSONRPCReply(const UniValue& result, const UniValue& error, const UniValue& id)
{
    UniValue reply = JSONRPCReplyObj(result, error, id);
    return reply.write() + "\n";
}

UniValue JSONRPCError(int code, const std::string& message)
{
    UniValue error(UniValue::VOBJ);
    error.pushKV("code", code);
    error.pushKV("message", message);
    return error;
}

/** Username used when cookie authentication is in use (arbitrary, only for
 * recognizability in debugging/logging purposes)
 */
static const std::string COOKIEAUTH_USER = "__cookie__";
/** Default name for auth cookie file */
static const std::string COOKIEAUTH_FILE = ".cookie";

fs::path GetAuthCookieFile()
{
    fs::path path(gArgs.GetArg("-rpccookiefile", COOKIEAUTH_FILE));
    return AbsPathForConfigVal(path);
}

bool GenerateAuthCookie(std::string *cookie_out)
{
    unsigned char rand_pwd[32];
    GetRandBytes(rand_pwd, 32);
    std::string cookie = COOKIEAUTH_USER + ":" + EncodeBase64(&rand_pwd[0],32);

    /** the umask determines what permissions are used to create this file -
     * these are set to 077 in init.cpp unless overridden with -sysperms.
     */
    std::ofstream file;
    fs::path filepath = GetAuthCookieFile();
    file.open(filepath.string().c_str());
    if (!file.is_open()) {
        LogPrintf("Unable to open cookie authentication file %s for writing\n", filepath.string());
        return false;
    }
    file << cookie;
    file.close();
    LogPrintf("Generated RPC authentication cookie %s\n", filepath.string());

    if (cookie_out)
        *cookie_out = cookie;
    return true;
}

bool GetAuthCookie(std::string *cookie_out)
{
    std::ifstream file;
    std::string cookie;
    fs::path filepath = GetAuthCookieFile();
    file.open(filepath.string().c_str());
    if (!file.is_open())
        return false;
    std::getline(file, cookie);
    file.close();

    if (cookie_out)
        *cookie_out = cookie;
    return true;
}

void DeleteAuthCookie()
{
    try {
        fs::remove(GetAuthCookieFile());
    } catch (const fs::filesystem_error& e) {
        LogPrintf("%s: Unable to remove random auth cookie file: %s\n", __func__, e.what());
    }
}
