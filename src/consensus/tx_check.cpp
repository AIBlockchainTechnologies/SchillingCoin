// Copyright (c) 2017-2021 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/tx_check.h>

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <version.h>

#include <set>

bool CheckTransaction(const CTransaction& tx, TxValidationState& state)
{
    bool allow_empty_inputs{false};
    bool allow_empty_outputs{false};

    if (tx.nType == TRANSACTION_QUORUM_COMMITMENT ||
        tx.nType == TRANSACTION_MNHF_SIGNAL) {
        allow_empty_inputs = true;
        allow_empty_outputs = true;
    }

    if (tx.nType == TRANSACTION_ASSET_UNLOCK) {
        allow_empty_inputs = true;
    }

    if (!allow_empty_inputs && tx.vin.empty()) {
        return state.Invalid(
            TxValidationResult::TX_CONSENSUS,
            "bad-txns-vin-empty");
    }

    if (!allow_empty_outputs && tx.vout.empty()) {
        return state.Invalid(
            TxValidationResult::TX_CONSENSUS,
            "bad-txns-vout-empty");
    }

    if (::GetSerializeSize(tx, PROTOCOL_VERSION) > MAX_ZEROCOIN_TX_SIZE) {
        return state.Invalid(
            TxValidationResult::TX_CONSENSUS,
            "bad-txns-oversize");
    }

    if (tx.vExtraPayload.size() > MAX_TX_EXTRA_PAYLOAD) {
        return state.Invalid(
            TxValidationResult::TX_CONSENSUS,
            "bad-txns-payload-oversize");
    }

    if (tx.nVersion < 1) {
        return state.Invalid(
            TxValidationResult::TX_CONSENSUS,
            "bad-txns-version");
    }

    if (tx.nVersion == 1) {
        if (tx.nType != TRANSACTION_NORMAL) {
            return state.Invalid(
                TxValidationResult::TX_CONSENSUS,
                "bad-txns-type");
        }

        if (!tx.vExtraPayload.empty()) {
            return state.Invalid(
                TxValidationResult::TX_CONSENSUS,
                "bad-txns-payload");
        }
    }

    if (tx.nVersion == 2) {
        return state.Invalid(
            TxValidationResult::TX_CONSENSUS,
            "bad-txns-version");
    }

    if (tx.nVersion > CTransaction::SPECIAL_VERSION) {
        return state.Invalid(
            TxValidationResult::TX_CONSENSUS,
            "bad-txns-version");
    }

    if (tx.nVersion != CTransaction::SPECIAL_VERSION &&
        tx.nType != TRANSACTION_NORMAL) {
        return state.Invalid(
            TxValidationResult::TX_CONSENSUS,
            "bad-txns-type");
    }

    if (tx.nVersion == CTransaction::SPECIAL_VERSION &&
        tx.nType == TRANSACTION_NORMAL &&
        !tx.vExtraPayload.empty()) {
        return state.Invalid(
            TxValidationResult::TX_CONSENSUS,
            "bad-txns-payload");
    }

    CAmount value_out{0};

    for (const CTxOut& txout : tx.vout) {
        if (txout.IsEmpty() && !tx.IsCoinBase() && !tx.IsCoinStake()) {
            return state.Invalid(
                TxValidationResult::TX_CONSENSUS,
                "bad-txns-vout-empty");
        }

        if (txout.nValue < 0) {
            return state.Invalid(
                TxValidationResult::TX_CONSENSUS,
                "bad-txns-vout-negative");
        }

        if (txout.nValue > MAX_MONEY) {
            return state.Invalid(
                TxValidationResult::TX_CONSENSUS,
                "bad-txns-vout-toolarge");
        }

        value_out += txout.nValue;

        if (!MoneyRange(value_out)) {
            return state.Invalid(
                TxValidationResult::TX_CONSENSUS,
                "bad-txns-txouttotal-toolarge");
        }

        if (txout.scriptPubKey.IsPayToColdStaking() &&
            txout.nValue < MIN_COLDSTAKING_AMOUNT) {
            return state.Invalid(
                TxValidationResult::TX_CONSENSUS,
                "bad-txns-cold-stake",
                "cold-staking output is below the minimum amount");
        }
    }

    std::set<COutPoint> input_outpoints;

    for (const CTxIn& txin : tx.vin) {
        // Historical Zerocoin spends use a special null prevout and may share
        // that serialized outpoint. Their serial-spend rules are validated by
        // the retained historical Zerocoin compatibility path.
        if (txin.IsZerocoinSpend()) {
            continue;
        }

        if (!input_outpoints.insert(txin.prevout).second) {
            return state.Invalid(
                TxValidationResult::TX_CONSENSUS,
                "bad-txns-inputs-duplicate");
        }
    }

    if (tx.IsCoinBase()) {
        size_t minimum_coinbase_script_size{2};

        if (tx.nType == TRANSACTION_COINBASE) {
            minimum_coinbase_script_size = 1;
        }

        if (tx.vin[0].scriptSig.size() < minimum_coinbase_script_size ||
            tx.vin[0].scriptSig.size() > 150) {
            return state.Invalid(
                TxValidationResult::TX_CONSENSUS,
                "bad-cb-length");
        }
    } else {
        for (const CTxIn& txin : tx.vin) {
            if (txin.prevout.IsNull() && !txin.IsZerocoinSpend()) {
                return state.Invalid(
                    TxValidationResult::TX_CONSENSUS,
                    "bad-txns-prevout-null");
            }
        }
    }

    return true;
}