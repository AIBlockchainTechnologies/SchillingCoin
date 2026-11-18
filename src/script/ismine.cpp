// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2021 The PIVX Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/ismine.h>

#include <keystore.h>
#include <script/script.h>
#include <script/standard.h>

#include <vector>

namespace {

unsigned int HaveKeys(const std::vector<valtype>& pubkeys, const CKeyStore& keystore)
{
    unsigned int result{0};

    for (const valtype& pubkey : pubkeys) {
        const CKeyID key_id{CPubKey(pubkey).GetID()};
        if (keystore.HaveKey(key_id)) {
            ++result;
        }
    }

    return result;
}

} // namespace

isminetype IsMine(const CKeyStore& keystore, const CTxDestination& dest)
{
    return IsMine(keystore, GetScriptForDestination(dest));
}

isminetype IsMine(const CKeyStore& keystore, const CScript& script_pub_key)
{
    std::vector<valtype> solutions;
    const TxoutType which_type{Solver(script_pub_key, solutions)};

    CKeyID key_id;

    switch (which_type) {
    case TxoutType::NONSTANDARD:
    case TxoutType::NULL_DATA:
        break;

    case TxoutType::ZEROCOINMINT:
        // Retained only for historical SCH chain interpretation.
        break;

    case TxoutType::PUBKEY:
        key_id = CPubKey(solutions[0]).GetID();
        if (keystore.HaveKey(key_id)) {
            return ISMINE_SPENDABLE;
        }
        break;

    case TxoutType::PUBKEYHASH:
        key_id = CKeyID(uint160(solutions[0]));
        if (keystore.HaveKey(key_id)) {
            return ISMINE_SPENDABLE;
        }
        break;

    case TxoutType::SCRIPTHASH: {
        const CScriptID script_id{uint160(solutions[0])};
        CScript subscript;

        if (keystore.GetCScript(script_id, subscript)) {
            const isminetype result{IsMine(keystore, subscript)};
            if (result != ISMINE_NO) {
                return result;
            }
        }
        break;
    }

    case TxoutType::COLDSTAKE: {
        const CKeyID staking_key_id{uint160(solutions[0])};
        const CKeyID spending_key_id{uint160(solutions[1])};

        const bool staking_key_is_mine{keystore.HaveKey(staking_key_id)};
        const bool spending_key_is_mine{keystore.HaveKey(spending_key_id)};

        if (spending_key_is_mine) {
            // Match PIVX ownership precedence: the spending-key
            // classification wins when both P2CS keys are present.
            return ISMINE_SPENDABLE_DELEGATED;
        }

        if (staking_key_is_mine) {
            return ISMINE_COLD;
        }

        break;
    }

    case TxoutType::MULTISIG: {
        const std::vector<valtype> keys{
            solutions.begin() + 1,
            solutions.end() - 1
        };

        if (HaveKeys(keys, keystore) == keys.size()) {
            return ISMINE_SPENDABLE;
        }
        break;
    }
    }

    if (keystore.HaveWatchOnly(script_pub_key)) {
        return which_type == TxoutType::COLDSTAKE
            ? ISMINE_WATCH_COLDSTAKE
            : ISMINE_WATCH_ONLY_;
    }

    return ISMINE_NO;
}