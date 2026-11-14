// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_SCRIPT_STANDARD_H
#define SCHILLINGCOIN_SCRIPT_STANDARD_H

#include "addresstype.h"
#include "script/interpreter.h"

#include <stdint.h>

class CKeyID;
class CScript;

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

enum class TxoutType
{
    NONSTANDARD,
    // 'standard' transaction types:
    PUBKEY,
    PUBKEYHASH,
    SCRIPTHASH,
    MULTISIG,
    NULL_DATA, //<! unspendable OP_RETURN script that carries data

    // Historical SCH Zerocoin mint classification. Runtime Zerocoin remains
    // retired; this type is retained for historical-chain interpretation.
    ZEROCOINMINT,

    // Historical SCH pay-to-cold-staking output.
    COLDSTAKE,
};

/** Get the name of a TxoutType. */
std::string GetTxnOutputType(TxoutType type);

constexpr bool IsPushdataOp(opcodetype opcode)
{
    return opcode > OP_FALSE && opcode <= OP_PUSHDATA4;
}

/**
 * Parse a scriptPubKey and identify its standard output type.
 * TxoutType::NONSTANDARD represents a failed solve.
 */
TxoutType Solver(const CScript& scriptPubKey, std::vector<std::vector<unsigned char>>& vSolutionsRet);

int ScriptSigArgsExpected(TxoutType type, const std::vector<std::vector<unsigned char>>& vSolutions);

bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet, bool fColdStake);

bool ExtractDestinations(const CScript& scriptPubKey, TxoutType& typeRet, std::vector<CTxDestination>& addressRet, int& nRequiredRet);

CScript GetLockedScriptForDestination(const CTxDestination& dest, int nLockTime);
CScript GetScriptForRawPubKey(const CPubKey& pubKey);
CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys);
CScript GetScriptForStakeDelegation(const CKeyID& stakingKey, const CKeyID& spendingKey);

#endif // SCHILLINGCOIN_SCRIPT_STANDARD_H