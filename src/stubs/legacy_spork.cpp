// Copyright (c) 2014-2016 The Dash developers
// Copyright (c) 2016-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
/*
    Don't ever reuse these IDs for other sporks...
    - This would result in old clients getting confused about which spork is for what.

    The following legacy sporks have been permanently removed from SCH:
      * SwiftTX sporks
          SPORK_2_SWIFTTX;
          SPORK_3_SWIFTTX_BLOCK_FILTERING;
          SPORK_5_MAX_VALUE;

      * Zerocoin sporks
          SPORK_16_ZEROCOIN_MAINTENANCE_MODE;
          SPORK_18_ZEROCOIN_PUBLICSPEND_V4;

      * Governance / Superblock sporks
          SPORK_9_MASTERNODE_BUDGET_ENFORCEMENT;
          SPORK_13_ENABLE_SUPERBLOCKS;

      * Masternode payment enforcement spork
          SPORK_8_MASTERNODE_PAYMENT_ENFORCEMENT;

      * Cold staking enforcement spork
          SPORK_17_COLDSTAKING_ENFORCEMENT;

      * Protocol‑enforcement sporks
          SPORK_14_NEW_PROTOCOL_ENFORCEMENT;
          SPORK_15_NEW_PROTOCOL_ENFORCEMENT_2;

    Rationale:
    - SCH now uses static, hard‑coded consensus rules for protocol enforcement,
      masternode payments, cold staking, and all previously spork‑controlled features.
    - SPORK_14 and SPORK_15 were never used dynamically in the original SCH binary;
      SPORK_14 was effectively always ON, and SPORK_15 was always OFF.
    - All listed sporks are permanently removed and must never be reintroduced or
      repurposed. Their numeric IDs must remain retired to avoid older clients
      misinterpreting future spork messages.

    Important:
    - These numeric IDs must never be reused for new features.
    - Reusing an ID risks older clients misinterpreting the meaning of a spork
      message and behaving unpredictably.

    NOTE (stub behavior):
    - The code in this file is intentionally a minimal stub/no-op.
    - Dynamic spork handling, signing, relay, DB writes, and runtime updates are
      deliberately disabled; spork behavior is enforced statically by consensus.
    - This file preserves API compatibility only; functions are retained as
      no-ops to avoid breaking callers.
    - Do not re-enable dynamic spork behavior or signature checks without a
      full security audit and explicit design justification.
*/

#include "main.h"
#include "masternode-budget.h"
#include "messagesigner.h"
#include "net.h"
#include "stubs/legacy_spork.h"
#include "stubs/legacy_sporkdb.h"
#include <iostream>

#define MAKE_SPORK_DEF(name, defaultValue) CSporkDef(name, defaultValue, #name)

std::vector<CSporkDef> sporkDefs = { };

CSporkManager sporkManager;
std::map<uint256, CSporkMessage> mapSporks;

CSporkManager::CSporkManager()
{
    // Initialize spork definition maps.
    sporkDefsById.clear();
    sporkDefsByName.clear();
}

void CSporkManager::Clear()
{
    // Clear sensitive and runtime state.
    strMasterPrivKey.clear();
    mapSporksActive.clear();
}

void CSporkManager::LoadSporksFromDB()
{
    // Runtime maps are not populated from DB for static spork handling.
    mapSporks.clear();
    mapSporksActive.clear();
    sporkDefsById.clear();
    sporkDefsByName.clear();
}

void CSporkManager::ProcessSpork(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{
    if (fLiteMode) return;

    // NOTE:
    // After Darksend/Obfuscation removal and related refactors, legacy SPORK signatures
    // from existing peers may no longer validate against the current SPORK keys.
    // Banning peers here causes the node to lose all connections and prevents sync.
    // For now, we IGNORE invalid SPORK signatures instead of banning the peer, so
    // networking and block/tx relay continue to function while SPORK signing is
    // being migrated/updated.

    LOCK(cs);
    mapSporks.clear();
    mapSporksActive.clear();
    sporkDefsById.clear();
    sporkDefsByName.clear();

    if (strCommand == "spork") {
        CSporkMessage spork;
        try {
            vRecv >> spork; // Consume to keep stream state consistent.
        } catch (...) {
            return;
        }

        // Preserve compatibility: check signature, log invalid signatures, but do not ban.
        bool fValidSig = spork.CheckSignature();
        if (!fValidSig) {
            LOCK(cs_main);
            LogPrintf("%s : Invalid Signature (IGNORED - NOT BANNING PEER, POSSIBLY DUE TO DARKSEND/OBFUSCATION REMOVAL)\n", __func__);
            return;
        }

        // Ignore dynamic spork content: do not store, relay, or write to DB.
        return;
    }

    if (strCommand == "getsporks") {
        // Do not send dynamic sporks; return silently.
        return;
    }

    (void)pfrom;
    (void)vRecv;
}

bool CSporkManager::UpdateSpork(SporkId nSporkID, int64_t nValue)
{
    // Spork logic is static/hardcoded; do not create/sign/relay dynamic sporks.
    (void)nSporkID;
    (void)nValue;
    return false;
}

bool CSporkManager::IsSporkActive(SporkId nSporkID)
{
    // Runtime/dynamic sporks disabled; static spork logic is authoritative.
    (void)nSporkID;
    return false;
}

int64_t CSporkManager::GetSporkValue(SporkId nSporkID)
{
    // Static spork handling: return hardcoded/default value if available.
    LOCK(cs);
    auto it = sporkDefsById.find(nSporkID);
    if (it != sporkDefsById.end()) {
        return it->second->defaultValue;
    }
    LogPrintf("%s : Unknown Spork %d\n", __func__, nSporkID);
    return -1;
}

SporkId CSporkManager::GetSporkIDByName(std::string strName)
{
    // Lookup in static/hardcoded spork definitions (thread-safe).
    LOCK(cs);
    auto it = sporkDefsByName.find(strName);
    if (it == sporkDefsByName.end()) {
        LogPrintf("%s : Unknown Spork name '%s'\n", __func__, strName);
        return SPORK_INVALID;
    }
    return it->second->sporkId;
}

std::string CSporkManager::GetSporkNameByID(SporkId nSporkID)
{
    // Lookup in static/hardcoded spork definitions (thread-safe).
    LOCK(cs);
    auto it = sporkDefsById.find(nSporkID);
    if (it == sporkDefsById.end()) {
        LogPrint("%s : Unknown Spork ID %d\n", __func__, nSporkID);
        return "Unknown";
    }
    return it->second->name;
}

bool CSporkManager::SetPrivKey(std::string strPrivKey)
{
    // Spork signing disabled; do not accept or store runtime private keys.
    (void)strPrivKey;
    LOCK(cs);
    strMasterPrivKey.clear();
    return false;
}

std::string CSporkManager::ToString() const
{
    // Report active spork count (static spork handling).
    LOCK(cs);
    return strprintf("Sporks: %llu", (unsigned long long)mapSporksActive.size());
}

uint256 CSporkMessage::GetSignatureHash() const
{
    // Signing disabled in SCH; return null hash.
    return uint256();
}

std::string CSporkMessage::GetStrMessage() const
{
    // Signing/serialization disabled in SCH; return empty string.
    return std::string();
}

const CPubKey CSporkMessage::GetPublicKey(std::string& strErrorRet) const
{
    // No error path currently; keep signature compatibility.
    strErrorRet.clear();
    return CPubKey(ParseHex(Params().GetConsensus().strSporkPubKey));
}

const CPubKey CSporkMessage::GetPublicKeyOld() const
{
    // Returns the consensus "old" spork public key; callers should handle an empty CPubKey.
    return CPubKey(ParseHex(Params().GetConsensus().strSporkPubKeyOld));
}

bool CSporkMessage::CheckSignature() const
{
    // Signatures are disabled under SCH static enforcement; treat spork messages as valid.
    return true;
}

bool CSporkMessage::CheckSignature(const CPubKey& pubkey) const
{
    // Signatures are disabled under SCH static enforcement; accept spork messages as valid.
    (void)pubkey;
    return true;
}

bool CSporkMessage::Sign(const std::string& strPrivKey, bool fNewSigs)
{
    // Signing disabled under SCH static enforcement; keep signature empty and treat message as valid.
    (void)strPrivKey;
    (void)fNewSigs;
    vchSig.clear();
    nMessVersion = MessageVersion::MESS_VER_HASH;
    return true;
}

void CSporkMessage::Relay()
{
    // Sporks are statically enforced in SCH; no runtime relay required.
    // Function retained for API compatibility; intentionally a no-op.
}