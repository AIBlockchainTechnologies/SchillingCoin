// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "policy/policy.h"

#include "coins.h"
#include "consensus/tx_verify.h"
#include "main.h"
#include "protocol.h"
#include "util/system.h"

#include <vector>

bool fIsBareMultisigStd = DEFAULT_PERMIT_BAREMULTISIG;

CFeeRate dustRelayFee = CFeeRate(DUST_RELAY_TX_FEE);

CAmount GetDustThreshold(const CTxOut& txout, const CFeeRate& dustRelayFeeIn)
{
    if (txout.scriptPubKey.IsUnspendable())
        return 0;

    size_t nSize = GetSerializeSize(txout, SER_DISK, 0) + 148u;
    return dustRelayFeeIn.GetFee(nSize);
}

CAmount GetDustThreshold(const CFeeRate& dustRelayFeeIn)
{
    return dustRelayFeeIn.GetFee(182);
}

bool IsDust(const CTxOut& txout, const CFeeRate& dustRelayFeeIn)
{
    return txout.nValue < GetDustThreshold(txout, dustRelayFeeIn);
}

bool IsStandard(const CScript& scriptPubKey, txnouttype& whichType)
{
    std::vector<valtype> vSolutions;
    if (!Solver(scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_MULTISIG)
    {
        unsigned char m = vSolutions.front()[0];
        unsigned char n = vSolutions.back()[0];
        // Support up to x-of-3 multisig txns as standard
        if (n < 1 || n > 3)
            return false;
        if (m < 1 || m > n)
            return false;
    } else if (whichType == TX_NULL_DATA &&
                (!gArgs.GetBoolArg("-datacarrier", true) || scriptPubKey.size() > nMaxDatacarrierBytes))
        return false;

    return whichType != TX_NONSTANDARD;
}

bool IsStandardTx(const CTransaction& tx, std::string& reason)
{
    AssertLockHeld(cs_main);

    if (tx.nVersion < 1) {
        reason = "version";
        return false;
    }

    if (tx.nVersion == 2) {
        reason = "version";
        return false;
    }

    if (tx.nVersion > CTransaction::SPECIAL_VERSION) {
        reason = "version";
        return false;
    }

    if (tx.nVersion == 1 && tx.nType != TRANSACTION_NORMAL) {
        reason = "type";
        return false;
    }

    if (tx.vExtraPayload.size() > MAX_TX_EXTRA_PAYLOAD) {
        reason = "payload-size";
        return false;
    }

    // Treat non-final transactions as non-standard to prevent a specific type
    // of double-spend attack, as well as DoS attacks. (if the transaction
    // can't be mined, the attacker isn't expending resources broadcasting it)
    if (!IsFinalTx(tx, chainActive.Height() + 1)) {
        reason = "non-final";
        return false;
    }

    unsigned int sz = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
    unsigned int nMaxSize = tx.ContainsZerocoins() ? MAX_ZEROCOIN_TX_SIZE : MAX_STANDARD_TX_SIZE;

    if (sz >= nMaxSize) {
        reason = "tx-size";
        return false;
    }

    for (const CTxIn& txin : tx.vin) {
        if (txin.IsZerocoinSpend())
            continue;

        if (txin.scriptSig.size() > 1650) {
            reason = "scriptsig-size";
            return false;
        }

        if (!txin.scriptSig.IsPushOnly()) {
            reason = "scriptsig-not-pushonly";
            return false;
        }
    }

    unsigned int nDataOut = 0;
    txnouttype whichType;

    for (const CTxOut& txout : tx.vout) {
        if (!::IsStandard(txout.scriptPubKey, whichType)) {
            reason = "scriptpubkey";
            return false;
        }

        if (whichType == TX_NULL_DATA) {
            nDataOut++;
        } else if ((whichType == TX_MULTISIG) && (!fIsBareMultisigStd)) {
            reason = "bare-multisig";
            return false;
        } else if (IsDust(txout, dustRelayFee)) {
            reason = "dust";
            return false;
        }
    }

    if (nDataOut > 1) {
        reason = "multi-op-return";
        return false;
    }

    return true;
}

/**
 * Check transaction inputs to mitigate two
 * potential denial-of-service attacks:
 *
 * 1. scriptSigs with extra data stuffed into them,
 *    not consumed by scriptPubKey (or P2SH script)
 * 2. P2SH scripts with a crazy number of expensive
 *    CHECKSIG/CHECKMULTISIG operations
 */
bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs)
{
    if (tx.IsCoinBase() || tx.HasZerocoinSpendInputs())
        return true; // coinbase has no inputs and zerocoinspend has a special input

    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const Coin& coin = mapInputs.AccessCoin(tx.vin[i].prevout);

        if (coin.IsSpent())
            return false;

        const CTxOut& prev = coin.out;

        std::vector<std::vector<unsigned char> > vSolutions;
        txnouttype whichType;

        const CScript& prevScript = prev.scriptPubKey;
        if (!Solver(prevScript, whichType, vSolutions))
            return false;

        int nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
        if (nArgsExpected < 0)
            return false;

        std::vector<std::vector<unsigned char> > stack;
        if (!EvalScript(
                stack,
                tx.vin[i].scriptSig,
                false,
                BaseSignatureChecker(),
                SIGVERSION_BASE,
                nullptr)) {
            return false;
        }

        if (whichType == TX_SCRIPTHASH) {
            if (stack.empty())
                return false;

            CScript subscript(stack.back().begin(), stack.back().end());
            std::vector<std::vector<unsigned char> > vSolutions2;
            txnouttype whichType2;

            if (Solver(subscript, whichType2, vSolutions2)) {
                int tmpExpected = ScriptSigArgsExpected(whichType2, vSolutions2);
                if (tmpExpected < 0)
                    return false;

                nArgsExpected += tmpExpected;
            } else {
                unsigned int sigops = subscript.GetSigOpCount(true);
                return (sigops <= MAX_P2SH_SIGOPS);
            }
        }

        if (stack.size() != (unsigned int)nArgsExpected)
            return false;
    }

    return true;
}