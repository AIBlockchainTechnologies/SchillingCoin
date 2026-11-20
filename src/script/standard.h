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
#include "script/solver.h"

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

int ScriptSigArgsExpected(TxoutType type, const std::vector<std::vector<unsigned char>>& vSolutions);

bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet, bool fColdStake);

bool ExtractDestinations(const CScript& scriptPubKey, TxoutType& typeRet, std::vector<CTxDestination>& addressRet, int& nRequiredRet);

CScript GetLockedScriptForDestination(const CTxDestination& dest, int nLockTime);
CScript GetScriptForStakeDelegation(const CKeyID& stakingKey, const CKeyID& spendingKey);

#endif // SCHILLINGCOIN_SCRIPT_STANDARD_H