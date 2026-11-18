// Copyright (c) 2014-2025 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <masternode/payments.h>

#include <evo/deterministicmns.h>

#include <chain.h>
#include <consensus/amount.h>
#include <deploymentstatus.h>
#include <key_io.h>
#include <logging.h>
#include <primitives/block.h>
#include <script/standard.h>
#include <tinyformat.h>

#include <cassert>
#include <ranges>
#include <string>

int FindUnmatchedMasternodePayment(const std::vector<CTxOut>& expected,
                                   const std::vector<CTxOut>& actual,
                                   bool strict_multiplicity)
{
    if (!strict_multiplicity) {
        for (size_t i = 0; i < expected.size(); ++i) {
            const auto& txout = expected[i];
            if (!std::ranges::any_of(actual, [&txout](const auto& txout2) { return txout == txout2; })) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    std::vector<bool> consumed(actual.size(), false);
    for (size_t i = 0; i < expected.size(); ++i) {
        const auto& txout = expected[i];
        bool found = false;
        for (size_t j = 0; j < actual.size(); ++j) {
            if (!consumed[j] && actual[j] == txout) {
                consumed[j] = true;
                found = true;
                break;
            }
        }
        if (!found) return static_cast<int>(i);
    }
    return -1;
}

CAmount PlatformShare(const CAmount reward)
{
    const CAmount platformReward = reward * 375 / 1000;
    bool ok = MoneyRange(platformReward);
    assert(ok);
    return platformReward;
}

CAmount GetMasternodePayment(int nHeight, CAmount blockValue, const Consensus::Params& consensus_params, MnRewardEra era)
{
    // SCH consensus: no masternode payment during the historical PoW phase.
    // After PoW, 80% of the applicable block reward is paid to the masternode.
    // Transaction-fee distribution is supplied separately through feeReward.
    (void)consensus_params;
    (void)era;

    if (nHeight <= 100) {
        return 0;
    }

    return blockValue / 100 * 80;
}

[[nodiscard]] bool CMNPaymentsProcessor::GetBlockTxOuts(const CBlockIndex* pindexPrev, const CAmount blockSubsidy, const CAmount feeReward,
                                                        MnRewardEra era, std::vector<CTxOut>& voutMasternodePaymentsRet)
{
    voutMasternodePaymentsRet.clear();

    const int nBlockHeight = pindexPrev  == nullptr ? 0 : pindexPrev->nHeight + 1;

    CAmount masternodeReward = GetMasternodePayment(nBlockHeight, blockSubsidy + feeReward, m_consensus_params, era);

    // Credit Pool doesn't exist before V20. If any part of reward will re-allocated to credit pool before v20
    // activation these fund will be just permanently lost. Applicable for devnets, regtest, testnet
    if (era == MnRewardEra::EvoReward) {
        CAmount masternodeSubsidyReward = GetMasternodePayment(nBlockHeight, blockSubsidy, m_consensus_params, era);
        const CAmount platformReward = PlatformShare(masternodeSubsidyReward);
        masternodeReward -= platformReward;

        assert(MoneyRange(masternodeReward));

        LogPrint(BCLog::MNPAYMENTS, "CMNPaymentsProcessor::%s -- MN reward %lld reallocated to credit pool\n", __func__, platformReward);
        voutMasternodePaymentsRet.emplace_back(platformReward, CScript() << OP_RETURN);
    }
    const auto mnList = m_dmnman.GetListForBlock(pindexPrev);
    if (mnList.GetCounts().total() == 0) {
        LogPrint(BCLog::MNPAYMENTS, "CMNPaymentsProcessor::%s -- no masternode registered to receive a payment\n", __func__);
        return true;
    }
    const auto dmnPayee = mnList.GetMNPayee(pindexPrev);
    if (!dmnPayee) {
        return false;
    }

    CAmount operatorReward = 0;

    if (dmnPayee->nOperatorReward != 0 && dmnPayee->pdmnState->scriptOperatorPayout != CScript()) {
        // This calculation might eventually turn out to result in 0 even if an operator reward percentage is given.
        // This will however only happen in a few years when the block rewards drops very low.
        operatorReward = (masternodeReward * dmnPayee->nOperatorReward) / 10000;
        masternodeReward -= operatorReward;
    }

    if (dmnPayee->pdmnState->IsShared()) {
        // Shared masternodes split the owner reward by recorded collateral contribution, paying
        // each share's reward script (or its refund script when no reward script is set)
        const auto& shares = dmnPayee->pdmnState->shares;
        const auto amounts = SplitAmountByShares(masternodeReward, shares);
        for (size_t i = 0; i < shares.size(); ++i) {
            if (amounts[i] > 0) {
                voutMasternodePaymentsRet.emplace_back(amounts[i], shares[i].RewardScript());
            }
        }
    } else {
        const auto owner_payouts = GetOwnerPayouts(*dmnPayee->pdmnState);
        CAmount paid_owner_reward{0};
        for (size_t i = 0; i < owner_payouts.size(); ++i) {
            const bool last = i + 1 == owner_payouts.size();
            const CAmount payout_amount = last ? masternodeReward - paid_owner_reward
                                               : (masternodeReward * owner_payouts[i].reward) / MasternodePayoutShare::MAX_REWARD;
            paid_owner_reward += payout_amount;
            if (payout_amount > 0) {
                voutMasternodePaymentsRet.emplace_back(payout_amount, owner_payouts[i].scriptPayout);
            }
        }
    }
    if (operatorReward > 0) {
        voutMasternodePaymentsRet.emplace_back(operatorReward, dmnPayee->pdmnState->scriptOperatorPayout);
    }

    return true;
}

/**
*   GetMasternodeTxOuts
*
*   Get masternode payment tx outputs
*/
[[nodiscard]] bool CMNPaymentsProcessor::GetMasternodeTxOuts(const CBlockIndex* pindexPrev, const CAmount blockSubsidy, const CAmount feeReward,
                                                             MnRewardEra era, std::vector<CTxOut>& voutMasternodePaymentsRet)
{
    // make sure it's not filled yet
    voutMasternodePaymentsRet.clear();

    if(!GetBlockTxOuts(pindexPrev, blockSubsidy, feeReward, era, voutMasternodePaymentsRet)) {
        LogPrintf("CMNPaymentsProcessor::%s -- ERROR Failed to get payee\n", __func__);
        return false;
    }

    for (const auto& txout : voutMasternodePaymentsRet) {
        CTxDestination dest;
        ExtractDestination(txout.scriptPubKey, dest);

        LogPrintf("CMNPaymentsProcessor::%s -- Masternode payment %lld to %s\n", __func__, txout.nValue, EncodeDestination(dest));
    }

    return true;
}

[[nodiscard]] bool CMNPaymentsProcessor::IsTransactionValid(const CTransaction& txNew, const CBlockIndex* pindexPrev, const CAmount blockSubsidy,
                                                            const CAmount feeReward, MnRewardEra era, bool strict_multiplicity)
{
    const int nBlockHeight = pindexPrev  == nullptr ? 0 : pindexPrev->nHeight + 1;
    if (!DeploymentDIP0003Enforced(nBlockHeight, m_consensus_params)) {
        // can't verify historical blocks here
        return true;
    }

    std::vector<CTxOut> voutMasternodePayments;
    if (!GetBlockTxOuts(pindexPrev, blockSubsidy, feeReward, era, voutMasternodePayments)) {
        LogPrintf("CMNPaymentsProcessor::%s -- ERROR! Failed to get payees for block at height %s\n", __func__, nBlockHeight);
        return true;
    }

    // With strict multiplicity (v24 active, computed by the caller) each expected payment must be
    // matched by a distinct coinbase output: duplicate expected outputs require duplicate coinbase
    // outputs. Pre-v24 retains the legacy existence-only check to avoid tightening historical
    // validation.
    const int unmatched_idx = FindUnmatchedMasternodePayment(voutMasternodePayments, txNew.vout, strict_multiplicity);
    if (unmatched_idx >= 0) {
        const auto& txout = voutMasternodePayments[unmatched_idx];
        std::string str_payout;
        if (CTxDestination dest; ExtractDestination(txout.scriptPubKey, dest)) {
            str_payout = "address=" + EncodeDestination(dest);
        } else {
            str_payout = "scriptPubKey=" + HexStr(txout.scriptPubKey);
        }
        LogPrintf("CMNPaymentsProcessor::%s -- ERROR! Failed to find expected payee %s amount=%lld height=%d\n",
                  __func__, str_payout, txout.nValue, nBlockHeight);
        return false;
    }
    return true;
}

bool CMNPaymentsProcessor::IsBlockValueValid(const CBlock& block, const CBlockIndex* pindexPrev, const CAmount blockReward, std::string& strErrorRet)
{
    const int nBlockHeight = pindexPrev == nullptr ? 0 : pindexPrev->nHeight + 1;
    const CAmount actual_value = block.vtx[0]->GetValueOut();

    strErrorRet.clear();

    if (actual_value > blockReward) {
        strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d)",
                                nBlockHeight, actual_value, blockReward);
        return false;
    }

    return true;
}

bool CMNPaymentsProcessor::IsBlockPayeeValid(const CTransaction& txNew, const CBlockIndex* pindexPrev,
                                             const CAmount blockSubsidy, const CAmount feeReward,
                                             MnRewardEra era, bool strict_multiplicity)
{
    const int nBlockHeight = pindexPrev == nullptr ? 0 : pindexPrev->nHeight + 1;

    if (!IsTransactionValid(txNew, pindexPrev, blockSubsidy, feeReward, era, strict_multiplicity)) {
        LogPrintf("CMNPaymentsProcessor::%s -- ERROR! Invalid masternode payment detected at height %d: %s",
                  __func__, nBlockHeight, txNew.ToString()); /* Continued */
        return false;
    }

    LogPrint(BCLog::MNPAYMENTS,
             "CMNPaymentsProcessor::%s -- Valid masternode payment at height %d: %s",
             __func__, nBlockHeight, txNew.ToString()); /* Continued */
    return true;
}

void CMNPaymentsProcessor::FillBlockPayments(CMutableTransaction& txNew, const CBlockIndex* pindexPrev,
                                             const CAmount blockSubsidy, const CAmount feeReward,
                                             MnRewardEra era,
                                             std::vector<CTxOut>& voutMasternodePaymentsRet)
{
    const int nBlockHeight = pindexPrev == nullptr ? 0 : pindexPrev->nHeight + 1;

    if (!GetMasternodeTxOuts(pindexPrev, blockSubsidy, feeReward, era, voutMasternodePaymentsRet)) {
        LogPrint(BCLog::MNPAYMENTS,
                 "CMNPaymentsProcessor::%s -- No masternode to pay (MN list probably empty)\n",
                 __func__);
    }

    txNew.vout.insert(txNew.vout.end(),
                      voutMasternodePaymentsRet.begin(),
                      voutMasternodePaymentsRet.end());

    std::string voutMasternodeStr;
    for (const auto& txout : voutMasternodePaymentsRet) {
        txNew.vout[0].nValue -= txout.nValue;

        if (!voutMasternodeStr.empty()) {
            voutMasternodeStr += ",";
        }
        voutMasternodeStr += txout.ToString();
    }

    LogPrint(BCLog::MNPAYMENTS,
             "CMNPaymentsProcessor::%s -- nBlockHeight %d blockReward %lld voutMasternodePaymentsRet \"%s\" txNew %s",
             __func__, nBlockHeight, blockSubsidy + feeReward,
             voutMasternodeStr, txNew.ToString()); /* Continued */
}