// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "protocol.h"

#include "chainparams.h"
#include "util.h"
#include "util/system.h"
#include "utilstrencodings.h"

#ifndef WIN32
#include <arpa/inet.h>
#endif

static const char* ppszTypeName[] =
    {
        "ERROR",
        "tx",
        "block",
        "filtered block",
        "mn winner",
        "mn announce",
        "mn ping",
    };

static const int pnTypeValue[] =
    {
        0,
        MSG_TX,
        MSG_BLOCK,
        MSG_FILTERED_BLOCK,
        MSG_MASTERNODE_WINNER,
        MSG_MASTERNODE_ANNOUNCE,
        MSG_MASTERNODE_PING,
    };

static_assert(
    ARRAYLEN(ppszTypeName) == ARRAYLEN(pnTypeValue),
    "Inventory type names and values must remain aligned");

CMessageHeader::CMessageHeader()
{
    memcpy(pchMessageStart, Params().MessageStart(), MESSAGE_START_SIZE);
    memset(pchCommand, 0, sizeof(pchCommand));
    nMessageSize = -1;
    nChecksum = 0;
}

CMessageHeader::CMessageHeader(const char* pszCommand, unsigned int nMessageSizeIn)
{
    memcpy(pchMessageStart, Params().MessageStart(), MESSAGE_START_SIZE);
    memset(pchCommand, 0, sizeof(pchCommand));
    strncpy(pchCommand, pszCommand, COMMAND_SIZE);
    nMessageSize = nMessageSizeIn;
    nChecksum = 0;
}

std::string CMessageHeader::GetCommand() const
{
    return std::string(pchCommand, pchCommand + strnlen(pchCommand, COMMAND_SIZE));
}

bool CMessageHeader::IsValid() const
{
    // Check start string
    if (memcmp(pchMessageStart, Params().MessageStart(), MESSAGE_START_SIZE) != 0)
        return false;

    // Check the command string for errors
    for (const char* p1 = pchCommand; p1 < pchCommand + COMMAND_SIZE; p1++) {
        if (*p1 == 0) {
            // Must be all zeros after the first zero
            for (; p1 < pchCommand + COMMAND_SIZE; p1++)
                if (*p1 != 0)
                    return false;
        } else if (*p1 < ' ' || *p1 > 0x7E)
            return false;
    }

    // Message size
    if (nMessageSize > MAX_SIZE) {
        LogPrintf("CMessageHeader::IsValid() : (%s, %u bytes) nMessageSize > MAX_SIZE\n", GetCommand(), nMessageSize);
        return false;
    }

    return true;
}

CAddress::CAddress() : CService()
{
    Init();
}

CAddress::CAddress(CService ipIn, uint64_t nServicesIn) : CService(ipIn)
{
    Init();
    nServices = nServicesIn;
}

void CAddress::Init()
{
    nServices = NODE_NETWORK;
    nTime = 100000000;
}

CInv::CInv()
{
    type = 0;
    hash.SetNull();
}

CInv::CInv(int typeIn, const uint256& hashIn)
{
    type = typeIn;
    hash = hashIn;
}

CInv::CInv(const std::string& strType, const uint256& hashIn)
{
    type = 0;

    for (unsigned int i = 1; i < ARRAYLEN(ppszTypeName); ++i) {
        if (strType == ppszTypeName[i]) {
            type = pnTypeValue[i];
            break;
        }
    }

    if (type == 0) {
        LogPrint(
            BCLog::NET,
            "CInv::CInv(string, uint256) : unknown type '%s'",
            strType);
    }

    hash = hashIn;
}

bool operator<(const CInv& a, const CInv& b)
{
    return (a.type < b.type || (a.type == b.type && a.hash < b.hash));
}

bool CInv::IsKnownType() const
{
    switch (type) {
    case MSG_TX:
    case MSG_BLOCK:
    case MSG_FILTERED_BLOCK:
    case MSG_MASTERNODE_WINNER:
    case MSG_MASTERNODE_ANNOUNCE:
    case MSG_MASTERNODE_PING:
        return true;
    default:
        return false;
    }
}

bool CInv::IsMasterNodeType() const
{
    switch (type) {
    case MSG_MASTERNODE_WINNER:
    case MSG_MASTERNODE_ANNOUNCE:
    case MSG_MASTERNODE_PING:
        return true;
    default:
        return false;
    }
}

const char* CInv::GetCommand() const
{
    switch (type) {
    case MSG_TX:
        return ppszTypeName[1];
    case MSG_BLOCK:
        return ppszTypeName[2];
    case MSG_FILTERED_BLOCK:
        return ppszTypeName[3];
    case MSG_MASTERNODE_WINNER:
        return ppszTypeName[4];
    case MSG_MASTERNODE_ANNOUNCE:
        return ppszTypeName[5];
    case MSG_MASTERNODE_PING:
        return ppszTypeName[6];
    default:
        LogPrint(
            BCLog::NET,
            "CInv::GetCommand() : type=%d unknown type",
            type);
        return ppszTypeName[0];
    }
}

std::string CInv::ToString() const
{
    return strprintf("%s %s", GetCommand(), hash.ToString());
}