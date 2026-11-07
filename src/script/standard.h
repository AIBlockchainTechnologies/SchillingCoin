// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_SCRIPT_STANDARD_H
#define SCHILLINGCOIN_SCRIPT_STANDARD_H

#include "script/interpreter.h"
#include "uint256.h"
#include "util/hash_type.h"

#include <stdint.h>
#include <variant>

class CKeyID;
class CScript;
struct ScriptHash;

/** A reference to a CScript: the Hash160 of its serialization (see script.h) */
class CScriptID : public BaseHash<uint160>
{
public:
    CScriptID() : BaseHash() {}
    explicit CScriptID(const CScript& in);
    explicit CScriptID(const uint160& in) : BaseHash(in) {}
    explicit CScriptID(const ScriptHash& in);
};

static const unsigned int MAX_OP_RETURN_RELAY = 83;      //!< bytes (+1 for OP_RETURN, +2 for the pushdata opcodes)
extern unsigned nMaxDatacarrierBytes;

/**
 * Mandatory script verification flags that all new blocks must comply with for
 * them to be valid. (but old blocks may not comply with) Currently just P2SH,
 * but in the future other flags may be added, such as a soft-fork to enforce
 * strict DER encoding.
 *
 * Failing one of these tests may trigger a DoS ban - see CheckInputs() for
 * details.
 */
static const unsigned int MANDATORY_SCRIPT_VERIFY_FLAGS = SCRIPT_VERIFY_P2SH;

enum txnouttype
{
    TX_NONSTANDARD,
    // 'standard' transaction types:
    TX_PUBKEY,
    TX_PUBKEYHASH,
    TX_SCRIPTHASH,
    TX_MULTISIG,
    TX_NULL_DATA,
    TX_ZEROCOINMINT,
    TX_COLDSTAKE
};

class CNoDestination {
public:
    friend bool operator==(const CNoDestination& a, const CNoDestination& b) { return true; }
    friend bool operator<(const CNoDestination& a, const CNoDestination& b) { return true; }
};

struct PKHash : public BaseHash<uint160>
{
    PKHash() : BaseHash() {}
    explicit PKHash(const uint160& hash) : BaseHash(hash) {}
    explicit PKHash(const CPubKey& pubkey);
    explicit PKHash(const CKeyID& pubkey_id);
};

CKeyID ToKeyID(const PKHash& key_hash);

struct ScriptHash : public BaseHash<uint160>
{
    ScriptHash() : BaseHash() {}

    // These do not do what callers usually expect. Construct the ScriptHash
    // from the actual redeem script instead.
    explicit ScriptHash(const PKHash& hash) = delete;

    explicit ScriptHash(const uint160& hash) : BaseHash(hash) {}
    explicit ScriptHash(const CScript& script);
    explicit ScriptHash(const CScriptID& script);
};

/**
 * A transaction-output script destination.
 *
 * CNoDestination represents no destination.
 * PKHash represents a P2PKH destination.
 * ScriptHash represents a P2SH destination.
 */
using CTxDestination = std::variant<CNoDestination, PKHash, ScriptHash>;

/** Check whether a CTxDestination is a CNoDestination. */
bool IsValidDestination(const CTxDestination& dest);

const char* GetTxnOutputType(txnouttype t);

bool Solver(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<std::vector<unsigned char> >& vSolutionsRet);
int ScriptSigArgsExpected(txnouttype t, const std::vector<std::vector<unsigned char> >& vSolutions);
bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet, bool fColdStake = false);
bool ExtractDestinations(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<CTxDestination>& addressRet, int& nRequiredRet);

CScript GetScriptForDestination(const CTxDestination& dest);
CScript GetLockedScriptForDestination(const CTxDestination& dest, int nLockTime);
CScript GetScriptForRawPubKey(const CPubKey& pubKey);
CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys);
CScript GetScriptForStakeDelegation(const CKeyID& stakingKey, const CKeyID& spendingKey);

#endif // SCHILLINGCOIN_SCRIPT_STANDARD_H