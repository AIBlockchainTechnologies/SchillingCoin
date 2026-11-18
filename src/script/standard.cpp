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