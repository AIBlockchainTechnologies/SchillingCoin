// Copyright (c) 2017-2017 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/tx_verify.h>

#include <chain.h>
#include <coins.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <tinyformat.h>
#include <util/check.h>
#include <util/moneystr.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <utility>

bool IsFinalTx(const CTransaction& tx, int nBlockHeight, int64_t nBlockTime)
{
    if (tx.nLockTime == 0) {
        return true;
    }

    if (static_cast<int64_t>(tx.nLockTime) <
        (tx.nLockTime < LOCKTIME_THRESHOLD
             ? static_cast<int64_t>(nBlockHeight)
             : nBlockTime)) {
        return true;
    }

    for (const auto& txin : tx.vin) {
        if (txin.nSequence != CTxIn::SEQUENCE_FINAL) {
            return false;
        }
    }

    return true;
}

std::pair<int, int64_t> CalculateSequenceLocks(const CTransaction& tx, int flags, std::vector<int>& prevHeights, const CBlockIndex& block)
{
    assert(prevHeights.size() == tx.vin.size());
    int nMinHeight{-1};
    int64_t nMinTime{-1};
    const bool enforce_bip68 = static_cast<uint32_t>(tx.nVersion) >= 2 && (flags & LOCKTIME_VERIFY_SEQUENCE);
    if (!enforce_bip68) return {nMinHeight, nMinTime};

    for (size_t txin_index = 0; txin_index < tx.vin.size(); ++txin_index) {
        const CTxIn& txin = tx.vin[txin_index];
        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) {
            prevHeights[txin_index] = 0;
            continue;
        }
        const int coin_height = prevHeights[txin_index];
        if (txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG) {
            const int64_t coin_time{Assert(block.GetAncestor(std::max(coin_height - 1, 0)))->GetMedianTimePast()};
            nMinTime = std::max(nMinTime, coin_time + (static_cast<int64_t>(txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) << CTxIn::SEQUENCE_LOCKTIME_GRANULARITY) - 1);
        } else {
            nMinHeight = std::max(nMinHeight, coin_height + static_cast<int>(txin.nSequence & CTxIn::SEQUENCE_LOCKTIME_MASK) - 1);
        }
    }
    return {nMinHeight, nMinTime};
}

bool EvaluateSequenceLocks(const CBlockIndex& block, std::pair<int, int64_t> lockPair)
{
    assert(block.pprev);
    const int64_t block_time{block.pprev->GetMedianTimePast()};
    return lockPair.first < block.nHeight && lockPair.second < block_time;
}

bool SequenceLocks(const CTransaction& tx, int flags, std::vector<int>& prevHeights, const CBlockIndex& block)
{
    return EvaluateSequenceLocks(block, CalculateSequenceLocks(tx, flags, prevHeights, block));
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int sigops{0};
    for (const auto& txin : tx.vin) sigops += txin.scriptSig.GetSigOpCount(false);
    for (const auto& txout : tx.vout) sigops += txout.scriptPubKey.GetSigOpCount(false);
    return sigops;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase() || tx.HasZerocoinSpendInputs()) return 0;
    unsigned int sigops{0};
    for (unsigned int i = 0; i < tx.vin.size(); ++i) {
        const Coin& coin{inputs.AccessCoin(tx.vin[i].prevout)};
        assert(!coin.IsSpent());
        const CTxOut& prevout = coin.out;
        if (prevout.scriptPubKey.IsPayToScriptHash()) sigops += prevout.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return sigops;
}

unsigned int GetTransactionSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs, uint32_t flags)
{
    unsigned int sigops{GetLegacySigOpCount(tx)};
    if (tx.IsCoinBase() || tx.HasZerocoinSpendInputs()) return sigops;
    if (flags & SCRIPT_VERIFY_P2SH) sigops += GetP2SHSigOpCount(tx, inputs);
    return sigops;
}

bool Consensus::CheckTxInputs(const CTransaction& tx, TxValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight, const Consensus::Params& consensus_params, CAmount& txfee)
{
    assert(!tx.IsCoinBase());

    if (tx.HasZerocoinSpendInputs()) {
        const CAmount value_in{tx.GetZerocoinSpent()};
        const CAmount value_out{tx.GetValueOut()};
        if (!consensus_params.MoneyRange(value_in)) return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-inputvalues-outofrange");
        if (value_in < value_out) return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-in-belowout", strprintf("value in (%s) < value out (%s)", FormatMoney(value_in), FormatMoney(value_out)));
        const CAmount fee{value_in - value_out};
        if (!consensus_params.MoneyRange(fee)) return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-fee-outofrange");
        txfee = fee;
        return true;
    }

    if (!inputs.HaveInputs(tx)) return state.Invalid(TxValidationResult::TX_MISSING_INPUTS, "bad-txns-inputs-missingorspent", strprintf("%s: inputs missing/spent", __func__));

    CAmount value_in{0};
    for (const CTxIn& txin : tx.vin) {
        const Coin& coin{inputs.AccessCoin(txin.prevout)};
        assert(!coin.IsSpent());
        if ((coin.IsCoinBase() || coin.IsCoinStake()) && nSpendHeight - coin.nHeight < consensus_params.nCoinbaseMaturity) {
            return state.Invalid(TxValidationResult::TX_PREMATURE_SPEND, "bad-txns-premature-spend-of-coinbase", strprintf("tried to spend coinbase/coinstake at depth %d", nSpendHeight - coin.nHeight));
        }
        value_in += coin.out.nValue;
        if (!consensus_params.MoneyRange(coin.out.nValue) || !consensus_params.MoneyRange(value_in)) return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-inputvalues-outofrange");
    }

    const CAmount value_out{tx.GetValueOut()};
    if (value_in < value_out) return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-in-belowout", strprintf("value in (%s) < value out (%s)", FormatMoney(value_in), FormatMoney(value_out)));
    const CAmount fee{value_in - value_out};
    if (!consensus_params.MoneyRange(fee)) return state.Invalid(TxValidationResult::TX_CONSENSUS, "bad-txns-fee-outofrange");
    txfee = fee;
    return true;
}