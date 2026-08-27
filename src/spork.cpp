// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "masternode-budget.h"
#include "messagesigner.h"
#include "net.h"
#include "spork.h"
#include "sporkdb.h"
#include <iostream>

#define MAKE_SPORK_DEF(name, defaultValue) CSporkDef(name, defaultValue, #name)

/*
    Don't ever reuse these IDs for other sporks...
    - This would result in old clients getting confused about which spork is for what.

    The following legacy sporks have been permanently removed from SCH:
      * SwiftTX sporks
          (SPORK_2_SWIFTTX,
           SPORK_3_SWIFTTX_BLOCK_FILTERING,
           SPORK_5_MAX_VALUE)

      * Zerocoin sporks
          (SPORK_16_ZEROCOIN_MAINTENANCE_MODE,
           SPORK_18_ZEROCOIN_PUBLICSPEND_V4)

      * Governance / Superblock sporks
          (SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT,
           SPORK_13_ENABLE_SUPERBLOCKS)

      * Masternode payment enforcement spork
          (SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT)

    SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT has been retired because its
    behavior is now statically hard‑coded into SCH. The chain permanently
    uses the stable masternode set for payment and validation logic, and no
    longer relies on runtime spork activation or deactivation signals.

    These sporks were tied exclusively to subsystems that no longer exist or
    no longer require dynamic toggles. They must never be reused for new
    features, as older clients may misinterpret the IDs and behave
    unpredictably.
*/

std::vector<CSporkDef> sporkDefs = {
    MAKE_SPORK_DEF(SPORK_14_NEW_PROTOCOL_ENFORCEMENT,       4070908800ULL), // OFF
    MAKE_SPORK_DEF(SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2,     4070908800ULL), // OFF
    MAKE_SPORK_DEF(SPORK_17_COLDSTAKING_ENFORCEMENT,        4070908800ULL), // OFF
};

CSporkManager sporkManager;
std::map<uint256, CSporkMessage> mapSporks;

CSporkManager::CSporkManager()
{
    for (auto& sporkDef : sporkDefs) {
        sporkDefsById.emplace(sporkDef.sporkId, &sporkDef);
        sporkDefsByName.emplace(sporkDef.name, &sporkDef);
    }
}

void CSporkManager::Clear()
{
    strMasterPrivKey = "";
    mapSporksActive.clear();
}

// SchillingCoin: on startup load spork values from previous session if they exist in the sporkDB
void CSporkManager::LoadSporksFromDB()
{
    for (const auto& sporkDef : sporkDefs) {
        CSporkMessage spork;
        if (!pSporkDB->ReadSpork(sporkDef.sporkId, spork)) {
            LogPrintf("%s : no previous value for %s found in database\n", __func__, sporkDef.name);
            continue;
        }

        mapSporks[spork.GetHash()] = spork;
        mapSporksActive[spork.nSporkID] = spork;

        std::time_t result = spork.nValue;
        std::string sporkName = sporkManager.GetSporkNameByID(spork.nSporkID);
        if (spork.nValue > 1000000) {
            char* res = std::ctime(&result);
            LogPrintf("%s : loaded spork %s with value %d : %s\n", __func__,
                      sporkName.c_str(), spork.nValue, (res ? res : "no time"));
        } else {
            LogPrintf("%s : loaded spork %s with value %d\n", __func__,
                      sporkName, spork.nValue);
        }
    }
}

void CSporkManager::ProcessSpork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (fLiteMode) return; // disable all obfuscation/masternode related functionality

    int nChainHeight = 0;
    {
        LOCK(cs_main);
        if (chainActive.Tip() == nullptr)
            return;
        nChainHeight = chainActive.Height();
    }

    if (strCommand == "spork") {

        CSporkMessage spork;
        vRecv >> spork;

        std::string strSpork = sporkManager.GetSporkNameByID(spork.nSporkID);
        if (strSpork == "Unknown") return;

        if (spork.nTimeSigned > GetAdjustedTime() + 2 * 60 * 60) {
            LOCK(cs_main);
            LogPrintf("%s : ERROR: too far into the future\n", __func__);
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        if (spork.nMessVersion != MessageVersion::MESS_VER_HASH) {
            if (Params().GetConsensus().IsMessSigV2(nChainHeight - 600)) {
                LogPrintf("%s : nMessVersion=%d not accepted anymore at block %d\n",
                          __func__, spork.nMessVersion, nChainHeight);
                return;
            }
        }

        uint256 hash = spork.GetHash();
        std::string sporkName = sporkManager.GetSporkNameByID(spork.nSporkID);
        {
            LOCK(cs);
            if (mapSporksActive.count(spork.nSporkID)) {
                if (mapSporksActive[spork.nSporkID].nTimeSigned >= spork.nTimeSigned) {
                    LogPrintf("%s : spork %d (%s) in memory is more recent: %d >= %d\n", __func__,
                              spork.nSporkID, sporkName,
                              mapSporksActive[spork.nSporkID].nTimeSigned, spork.nTimeSigned);
                    return;
                } else {
                    LogPrintf("%s : got updated spork %d (%s) with value %d (signed at %d) - block %d\n",
                              __func__, spork.nSporkID, sporkName,
                              spork.nValue, spork.nTimeSigned, nChainHeight);
                }
            } else {
                LogPrintf("%s : got new spork %d (%s) with value %d (signed at %d) - block %d\n",
                          __func__, spork.nSporkID, sporkName,
                          spork.nValue, spork.nTimeSigned, nChainHeight);
            }
        }

        const bool fRequireNew = spork.nTimeSigned >= Params().GetConsensus().nTime_EnforceNewSporkKey;
        bool fValidSig = spork.CheckSignature();
        if (!fValidSig && !fRequireNew) {
            if (GetAdjustedTime() < Params().GetConsensus().nTime_RejectOldSporkKey) {
                CPubKey pubkeyold = spork.GetPublicKeyOld();
                fValidSig = spork.CheckSignature(pubkeyold);
            }
        }

        // NOTE:
        // After Darksend/Obfuscation removal and related refactors, legacy spork signatures
        // from existing peers may no longer validate against the current spork keys.
        // Banning peers here causes the node to lose all connections and prevents sync.
        // For now, we IGNORE invalid spork signatures instead of banning the peer, so
        // networking and block/tx relay continue to function while spork signing is
        // being migrated/updated.
        if (!fValidSig) {
            LOCK(cs_main);
            LogPrintf("%s : Invalid Signature (IGNORED - NOT BANNING PEER, POSSIBLY DUE TO DARKSEND/OBFUSCATION REMOVAL)\n", __func__);
            // Do NOT ban the peer. Just ignore this spork message.
            return;
        }

        {
            LOCK(cs);
            mapSporks[hash] = spork;
            mapSporksActive[spork.nSporkID] = spork;
        }
        spork.Relay();

        pSporkDB->WriteSpork(spork.nSporkID, spork);
    }

    if (strCommand == "getsporks") {
        LOCK(cs);
        std::map<SporkId, CSporkMessage>::iterator it = mapSporksActive.begin();
        while (it != mapSporksActive.end()) {
            pfrom->PushMessage("spork", it->second);
            ++it;
        }
    }
}

bool CSporkManager::UpdateSpork(SporkId nSporkID, int64_t nValue)
{
    bool fNewSigs = false;
    {
        LOCK(cs_main);
        fNewSigs = chainActive.NewSigsActive();
    }

    CSporkMessage spork(nSporkID, nValue, GetTime());

    if (spork.Sign(strMasterPrivKey, fNewSigs)) {
        spork.Relay();
        LOCK(cs);
        mapSporks[spork.GetHash()] = spork;
        mapSporksActive[nSporkID] = spork;
        return true;
    }

    return false;
}

bool CSporkManager::IsSporkActive(SporkId nSporkID)
{
    return GetSporkValue(nSporkID) < GetAdjustedTime();
}

int64_t CSporkManager::GetSporkValue(SporkId nSporkID)
{
    LOCK(cs);

    if (mapSporksActive.count(nSporkID)) {
        return mapSporksActive[nSporkID].nValue;
    } else {
        auto it = sporkDefsById.find(nSporkID);
        if (it != sporkDefsById.end()) {
            return it->second->defaultValue;
        } else {
            LogPrintf("%s : Unknown Spork %d\n", __func__, nSporkID);
        }
    }

    return -1;
}

SporkId CSporkManager::GetSporkIDByName(std::string strName)
{
    auto it = sporkDefsByName.find(strName);
    if (it == sporkDefsByName.end()) {
        LogPrintf("%s : Unknown Spork name '%s'\n", __func__, strName);
        return SPORK_INVALID;
    }
    return it->second->sporkId;
}

std::string CSporkManager::GetSporkNameByID(SporkId nSporkID)
{
    auto it = sporkDefsById.find(nSporkID);
    if (it == sporkDefsById.end()) {
        LogPrint("%s : Unknown Spork ID %d\n", __func__, nSporkID);
        return "Unknown";
    }
    return it->second->name;
}

bool CSporkManager::SetPrivKey(std::string strPrivKey)
{
    CSporkMessage spork;

    spork.Sign(strPrivKey, true);

    const bool fRequireNew = GetTime() >= Params().GetConsensus().nTime_EnforceNewSporkKey;
    bool fValidSig = spork.CheckSignature();
    if (!fValidSig && !fRequireNew) {
        if (GetAdjustedTime() < Params().GetConsensus().nTime_RejectOldSporkKey) {
            CPubKey pubkeyold = spork.GetPublicKeyOld();
            fValidSig = spork.CheckSignature(pubkeyold);
        }
    }

    if (fValidSig) {
        LOCK(cs);
        LogPrintf("%s : Successfully initialized as spork signer\n", __func__);
        strMasterPrivKey = strPrivKey;
        return true;
    }

    return false;
}

std::string CSporkManager::ToString() const
{
    LOCK(cs);
    return strprintf("Sporks: %llu", mapSporksActive.size());
}

uint256 CSporkMessage::GetSignatureHash() const
{
    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << nMessVersion;
    ss << nSporkID;
    ss << nValue;
    ss << nTimeSigned;
    return ss.GetHash();
}

std::string CSporkMessage::GetStrMessage() const
{
    return std::to_string(nSporkID) +
           std::to_string(nValue) +
           std::to_string(nTimeSigned);
}

const CPubKey CSporkMessage::GetPublicKey(std::string& strErrorRet) const
{
    // No error path currently; keep signature compatibility
    strErrorRet.clear();
    return CPubKey(ParseHex(Params().GetConsensus().strSporkPubKey));
}

const CPubKey CSporkMessage::GetPublicKeyOld() const
{
    return CPubKey(ParseHex(Params().GetConsensus().strSporkPubKeyOld));
}

bool CSporkMessage::CheckSignature() const
{
    std::string strError;
    CPubKey pubkey = GetPublicKey(strError);
    if (!strError.empty()) {
        LogPrintf("%s : GetPublicKey error: %s\n", __func__, strError);
        return false;
    }
    return CheckSignature(pubkey);
}

bool CSporkMessage::CheckSignature(const CPubKey& pubkey) const
{
    std::string strError;
    std::string strMessage;

    if (nMessVersion == MessageVersion::MESS_VER_STRMESS) {
        strMessage = GetStrMessage();
    } else {
        strMessage = GetSignatureHash().GetHex();
    }

    if (!CMessageSigner::VerifyMessage(pubkey, vchSig, strMessage, strError)) {
        LogPrintf("%s : VerifyMessage failed: %s\n", __func__, strError);
        return false;
    }

    return true;
}

bool CSporkMessage::Sign(const std::string& strPrivKey, bool fNewSigs)
{
    vchSig.clear();

    nMessVersion = fNewSigs ? MessageVersion::MESS_VER_HASH
                            : MessageVersion::MESS_VER_STRMESS;

    std::string strMessage;
    if (nMessVersion == MessageVersion::MESS_VER_STRMESS) {
        strMessage = GetStrMessage();
    } else {
        strMessage = GetSignatureHash().GetHex();
    }

    // Get CKey from secret (WIF-style) string
    CKey key;
    CPubKey pubkey;
    if (!CMessageSigner::GetKeysFromSecret(strPrivKey, key, pubkey)) {
        LogPrintf("%s : GetKeysFromSecret failed\n", __func__);
        return false;
    }

    // Sign message: signature goes into vchSig
    if (!CMessageSigner::SignMessage(strMessage, vchSig, key)) {
        LogPrintf("%s : SignMessage failed\n", __func__);
        return false;
    }

    return true;
}

void CSporkMessage::Relay()
{
    CInv inv(MSG_SPORK, GetHash());
    RelayInv(inv);
}