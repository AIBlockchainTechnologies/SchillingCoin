// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SPORK_H
#define SPORK_H

#include "base58.h"
#include "hash.h"
#include "key.h"
#include "main.h"
#include "net.h"
#include "sporkid.h"
#include "sync.h"
#include "util.h"
#include "protocol.h"
#include "masternode-sync.h"

class CSporkMessage;
class CSporkManager;

extern std::vector<CSporkDef> sporkDefs;
extern std::map<uint256, CSporkMessage> mapSporks;
extern CSporkManager sporkManager;

/*
    NOTE (stub behavior):
    - This header pairs with a minimal stub implementation in spork.cpp.
    - Dynamic spork handling, signing, relay, DB writes, and runtime updates are
      deliberately disabled; spork behavior is enforced statically by consensus.
    - The header preserves the original API for compatibility; some functions
      are intentionally no-ops in the implementation.
*/

class CSporkMessage
{
public:
    SporkId nSporkID;
    int64_t nValue;
    int64_t nTimeSigned;
    std::vector<unsigned char> vchSig;

    int nMessVersion;

    CSporkMessage()
        : nSporkID((SporkId)0),
          nValue(0),
          nTimeSigned(0),
          nMessVersion(0)
    {}

    CSporkMessage(SporkId id, int64_t val, int64_t time)
        : nSporkID(id),
          nValue(val),
          nTimeSigned(time),
          nMessVersion(0)
    {}

    uint256 GetHash() const { return XEVAN(BEGIN(nSporkID), END(nTimeSigned)); }

    void Relay();

    // REQUIRED BY spork.cpp — MUST be present.
    uint256 GetSignatureHash() const;
    std::string GetStrMessage() const;
    const CPubKey GetPublicKey(std::string& strErrorRet) const;
    const CPubKey GetPublicKeyOld() const;
    bool CheckSignature() const;
    bool CheckSignature(const CPubKey& pubkey) const;
    bool Sign(const std::string& strPrivKey, bool fNewSigs);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nSporkID);
        READWRITE(nValue);
        READWRITE(nTimeSigned);
        READWRITE(vchSig);
        READWRITE(nMessVersion);
    }
};

class CSporkManager
{
private:
    mutable RecursiveMutex cs;
    std::string strMasterPrivKey;
    std::map<SporkId, CSporkDef*> sporkDefsById;
    std::map<std::string, CSporkDef*> sporkDefsByName;
    std::map<SporkId, CSporkMessage> mapSporksActive;

public:
    CSporkManager();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(mapSporksActive);
    }

    void Clear();
    void LoadSporksFromDB();
    void ProcessSpork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    int64_t GetSporkValue(SporkId nSporkID);

    // ExecuteSpork is intentionally a no-op in the stub implementation.
    // Provided a small inline no-op here so the header does not declare an
    // undefined symbol when the implementation is intentionally omitted.
    inline void ExecuteSpork(SporkId nSporkID, int nValue)
    {
        (void)nSporkID;
        (void)nValue;
        // Intentionally empty: dynamic spork execution disabled.
    }

    bool UpdateSpork(SporkId nSporkID, int64_t nValue);
    bool IsSporkActive(SporkId nSporkID);
    std::string GetSporkNameByID(SporkId id);
    SporkId GetSporkIDByName(std::string strName);
    bool SetPrivKey(std::string strPrivKey);
    std::string ToString() const;
};

#endif