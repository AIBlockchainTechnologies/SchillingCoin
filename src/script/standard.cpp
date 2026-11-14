// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "script/standard.h"

#include "pubkey.h"
#include "script/script.h"
#include "util.h"
#include "util/system.h"

typedef std::vector<unsigned char> valtype;

unsigned nMaxDatacarrierBytes = MAX_OP_RETURN_RELAY;

std::string GetTxnOutputType(TxoutType type)
{
    switch (type) {
    case TxoutType::NONSTANDARD: return "nonstandard";
    case TxoutType::PUBKEY: return "pubkey";
    case TxoutType::PUBKEYHASH: return "pubkeyhash";
    case TxoutType::SCRIPTHASH: return "scripthash";
    case TxoutType::MULTISIG: return "multisig";
    case TxoutType::NULL_DATA: return "nulldata";
    case TxoutType::ZEROCOINMINT: return "zerocoinmint";
    case TxoutType::COLDSTAKE: return "coldstake";
    }
    assert(false);
}

static bool MatchPayToPubkey(const CScript& script, valtype& pubkey)
{
    if (script.size() == CPubKey::SIZE + 2 &&
        script[0] == CPubKey::SIZE &&
        script.back() == OP_CHECKSIG) {
        pubkey = valtype(script.begin() + 1, script.begin() + CPubKey::SIZE + 1);
        return CPubKey::ValidSize(pubkey);
    }

    if (script.size() == CPubKey::COMPRESSED_SIZE + 2 &&
        script[0] == CPubKey::COMPRESSED_SIZE &&
        script.back() == OP_CHECKSIG) {
        pubkey = valtype(script.begin() + 1, script.begin() + CPubKey::COMPRESSED_SIZE + 1);
        return CPubKey::ValidSize(pubkey);
    }

    return false;
}

static bool MatchPayToPubkeyHash(const CScript& script, valtype& pubkeyhash)
{
    if (script.size() == 25 &&
        script[0] == OP_DUP &&
        script[1] == OP_HASH160 &&
        script[2] == 20 &&
        script[23] == OP_EQUALVERIFY &&
        script[24] == OP_CHECKSIG) {
        pubkeyhash = valtype(script.begin() + 3, script.begin() + 23);
        return true;
    }

    return false;
}

static bool MatchPayToColdStaking(const CScript& script, valtype& staker_pubkey_hash, valtype& owner_pubkey_hash)
{
    if (script.IsPayToColdStaking()) {
        staker_pubkey_hash = valtype(script.begin() + 6, script.begin() + 26);
        owner_pubkey_hash = valtype(script.begin() + 28, script.begin() + 48);
        return true;
    }

    return false;
}

/** Test for "small positive integer" script opcodes - OP_1 through OP_16. */
static constexpr bool IsSmallInteger(opcodetype opcode)
{
    return opcode >= OP_1 && opcode <= OP_16;
}

static constexpr bool IsValidMultisigKeyCount(int n_keys)
{
    return n_keys > 0 && n_keys <= MAX_PUBKEYS_PER_MULTISIG;
}

static bool GetMultisigKeyCount(opcodetype opcode, valtype data, int& count)
{
    if (IsSmallInteger(opcode)) {
        count = CScript::DecodeOP_N(opcode);
        return IsValidMultisigKeyCount(count);
    }

    if (IsPushdataOp(opcode)) {
        if (!CheckMinimalPush(data, opcode)) return false;
        try {
            count = CScriptNum(data, /* fRequireMinimal = */ true).getint();
            return IsValidMultisigKeyCount(count);
        } catch (const scriptnum_error&) {
            return false;
        }
    }

    return false;
}

static bool MatchMultisig(const CScript& script, int& required_sigs, std::vector<valtype>& pubkeys)
{
    opcodetype opcode;
    valtype data;
    int num_keys;

    CScript::const_iterator it = script.begin();
    if (script.size() < 1 || script.back() != OP_CHECKMULTISIG) return false;

    if (!script.GetOp(it, opcode, data) ||
        !GetMultisigKeyCount(opcode, data, required_sigs)) {
        return false;
    }

    while (script.GetOp(it, opcode, data) && CPubKey::ValidSize(data)) {
        pubkeys.emplace_back(std::move(data));
    }

    if (!GetMultisigKeyCount(opcode, data, num_keys)) return false;

    if (pubkeys.size() != static_cast<unsigned long>(num_keys) ||
        num_keys < required_sigs) {
        return false;
    }

    return it + 1 == script.end();
}

TxoutType Solver(const CScript& scriptPubKey, std::vector<std::vector<unsigned char>>& vSolutionsRet)
{
    vSolutionsRet.clear();

    // Shortcut for pay-to-script-hash, which is more constrained than the other types.
    if (scriptPubKey.IsPayToScriptHash()) {
        std::vector<unsigned char> hash_bytes(scriptPubKey.begin() + 2, scriptPubKey.begin() + 22);
        vSolutionsRet.push_back(std::move(hash_bytes));
        return TxoutType::SCRIPTHASH;
    }

    // Historical SCH Zerocoin mint classification. Runtime Zerocoin is retired,
    // but historical-chain interpretation must still recognize these scripts.
    if (scriptPubKey.IsZerocoinMint()) {
        if (scriptPubKey.size() > 150) {
            return TxoutType::NONSTANDARD;
        }

        std::vector<unsigned char> hash_bytes(scriptPubKey.begin() + 2, scriptPubKey.end());
        vSolutionsRet.push_back(std::move(hash_bytes));
        return TxoutType::ZEROCOINMINT;
    }

    // Provably prunable, data-carrying output.
    if (scriptPubKey.size() >= 1 &&
        scriptPubKey[0] == OP_RETURN &&
        scriptPubKey.IsPushOnly(scriptPubKey.begin() + 1)) {
        return TxoutType::NULL_DATA;
    }

    std::vector<unsigned char> data;

    if (MatchPayToPubkey(scriptPubKey, data)) {
        vSolutionsRet.push_back(std::move(data));
        return TxoutType::PUBKEY;
    }

    if (MatchPayToPubkeyHash(scriptPubKey, data)) {
        vSolutionsRet.push_back(std::move(data));
        return TxoutType::PUBKEYHASH;
    }

    std::vector<unsigned char> owner_data;
    if (MatchPayToColdStaking(scriptPubKey, data, owner_data)) {
        vSolutionsRet.push_back(std::move(data));
        vSolutionsRet.push_back(std::move(owner_data));
        return TxoutType::COLDSTAKE;
    }

    int required;
    std::vector<std::vector<unsigned char>> keys;
    if (MatchMultisig(scriptPubKey, required, keys)) {
        vSolutionsRet.push_back({static_cast<unsigned char>(required)});
        vSolutionsRet.insert(vSolutionsRet.end(), keys.begin(), keys.end());
        vSolutionsRet.push_back({static_cast<unsigned char>(keys.size())});
        return TxoutType::MULTISIG;
    }

    vSolutionsRet.clear();
    return TxoutType::NONSTANDARD;
}

int ScriptSigArgsExpected(TxoutType type, const std::vector<std::vector<unsigned char>>& solutions)
{
    switch (type) {
    case TxoutType::NONSTANDARD:
    case TxoutType::NULL_DATA:
    case TxoutType::ZEROCOINMINT:
        return -1;
    case TxoutType::PUBKEY:
        return 1;
    case TxoutType::PUBKEYHASH:
        return 2;
    case TxoutType::COLDSTAKE:
        return 3;
    case TxoutType::MULTISIG:
        if (solutions.empty() || solutions[0].empty()) {
            return -1;
        }
        return solutions[0][0] + 1;
    case TxoutType::SCRIPTHASH:
        return 1;
    }

    return -1;
}

bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet, bool fColdStake)
{
    std::vector<valtype> solutions;
    const TxoutType type = Solver(scriptPubKey, solutions);

    switch (type) {
    case TxoutType::PUBKEY: {
        CPubKey pubkey(solutions[0]);
        if (!pubkey.IsValid()) {
            return false;
        }

        addressRet = PKHash(pubkey);
        return true;
    }

    case TxoutType::PUBKEYHASH:
        addressRet = PKHash(uint160(solutions[0]));
        return true;

    case TxoutType::SCRIPTHASH:
        addressRet = ScriptHash(uint160(solutions[0]));
        return true;

    case TxoutType::COLDSTAKE:
        if (solutions.size() < 2) {
            return false;
        }
        addressRet = PKHash(uint160(solutions[!fColdStake]));
        return true;

    case TxoutType::MULTISIG:
    case TxoutType::NULL_DATA:
    case TxoutType::ZEROCOINMINT:
    case TxoutType::NONSTANDARD:
        return false;
    }

    assert(false);
}

bool ExtractDestinations(const CScript& scriptPubKey, TxoutType& typeRet, std::vector<CTxDestination>& addressRet, int& nRequiredRet)
{
    addressRet.clear();

    std::vector<valtype> solutions;
    typeRet = Solver(scriptPubKey, solutions);

    if (typeRet == TxoutType::NONSTANDARD ||
        typeRet == TxoutType::NULL_DATA ||
        typeRet == TxoutType::ZEROCOINMINT) {
        return false;
    }

    if (typeRet == TxoutType::MULTISIG) {
        nRequiredRet = solutions.front()[0];

        for (unsigned int i = 1; i < solutions.size() - 1; ++i) {
            CPubKey pubkey(solutions[i]);
            if (!pubkey.IsValid()) {
                continue;
            }

            addressRet.emplace_back(PKHash(pubkey));
        }

        return !addressRet.empty();
    }

    if (typeRet == TxoutType::COLDSTAKE) {
        if (solutions.size() < 2) {
            return false;
        }

        nRequiredRet = 2;
        addressRet.emplace_back(PKHash(uint160(solutions[0])));
        addressRet.emplace_back(PKHash(uint160(solutions[1])));
        return true;
    }

    nRequiredRet = 1;
    CTxDestination address;
    if (!ExtractDestination(scriptPubKey, address)) {
        return false;
    }

    addressRet.push_back(address);
    return true;
}

namespace
{
class CLockedScriptVisitor
{
private:
    int nLockTime;

public:
    explicit CLockedScriptVisitor(int nLockTime) : nLockTime(nLockTime) {}

    CScript operator()(const PKHash& keyID) const
    {
        return CScript() << nLockTime << OP_CHECKLOCKTIMEVERIFY << OP_DROP
                         << OP_DUP << OP_HASH160 << ToByteVector(keyID)
                         << OP_EQUALVERIFY << OP_CHECKSIG;
    }

    CScript operator()(const ScriptHash& scriptID) const
    {
        return CScript() << nLockTime << OP_CHECKLOCKTIMEVERIFY << OP_DROP
                         << OP_HASH160 << ToByteVector(scriptID) << OP_EQUAL;
    }

    template <typename Destination>
    CScript operator()(const Destination&) const
    {
        return {};
    }
};
}

CScript GetScriptForStakeDelegation(const CKeyID& stakingKey, const CKeyID& spendingKey)
{
    CScript script;
    script << OP_DUP << OP_HASH160 << OP_ROT <<
            OP_IF << OP_CHECKCOLDSTAKEVERIFY << ToByteVector(stakingKey) <<
            OP_ELSE << ToByteVector(spendingKey) << OP_ENDIF <<
            OP_EQUALVERIFY << OP_CHECKSIG;
    return script;
}

CScript GetLockedScriptForDestination(const CTxDestination& dest, int nLockTime)
{
    return std::visit(CLockedScriptVisitor(nLockTime), dest);
}

CScript GetScriptForRawPubKey(const CPubKey& pubKey)
{
    return CScript() << std::vector<unsigned char>(pubKey.begin(), pubKey.end()) << OP_CHECKSIG;
}

CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys)
{
    CScript script;

    script << CScript::EncodeOP_N(nRequired);
    for (const CPubKey& key : keys)
        script << ToByteVector(key);
    script << CScript::EncodeOP_N(keys.size()) << OP_CHECKMULTISIG;
    return script;
}