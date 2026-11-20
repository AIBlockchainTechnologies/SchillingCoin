// Copyright (c) 2014-2025 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <masternode/payments.h>

#include <chain.h>
#include <consensus/amount.h>
#include <deploymentstatus.h>
#include <evo/deterministicmns.h>
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

CAmount GetMasternodePayment(int nHeight, CAmount blockValue, const Consensus::Params& consensus_params)
{
    (void)consensus_params;
    if (nHeight <= 100) return 0;
    return blockValue / 100 * 80;
}

[[nodiscard]] bool CMNPaymentsProcessor::GetBlockTxOuts(const CBlockIndex* pindexPrev, const CAmount blockSubsidy,
                                                         const CAmount feeReward, std::vector<CTxOut>& ret)
{
    ret.clear();
    const int height = pindexPrev == nullptr ? 0 : pindexPrev->nHeight + 1;
    CAmount ownerReward = GetMasternodePayment(height, blockSubsidy + feeReward, m_consensus_params);
    const auto mnList = m_dmnman.GetListForBlock(pindexPrev);
    if (mnList.GetCounts().total() == 0) return true;
    const auto payee = mnList.GetMNPayee(pindexPrev);
    if (!payee) return false;

    CAmount operatorReward{0};
    if (payee->nOperatorReward != 0 && payee->pdmnState->scriptOperatorPayout != CScript()) {
        operatorReward = (ownerReward * payee->nOperatorReward) / 10000;
        ownerReward -= operatorReward;
    }
    if (payee->pdmnState->IsShared()) {
        const auto& shares = payee->pdmnState->shares;
        const auto amounts = SplitAmountByShares(ownerReward, shares);
        for (size_t i = 0; i < shares.size(); ++i) if (amounts[i] > 0) ret.emplace_back(amounts[i], shares[i].RewardScript());
    } else {
        const auto payouts = GetOwnerPayouts(*payee->pdmnState);
        CAmount paid{0};
        for (size_t i = 0; i < payouts.size(); ++i) {
            const bool last = i + 1 == payouts.size();
            const CAmount amount = last ? ownerReward - paid : (ownerReward * payouts[i].reward) / MasternodePayoutShare::MAX_REWARD;
            paid += amount;
            if (amount > 0) ret.emplace_back(amount, payouts[i].scriptPayout);
        }
    }
    if (operatorReward > 0) ret.emplace_back(operatorReward, payee->pdmnState->scriptOperatorPayout);
    return true;
}

[[nodiscard]] bool CMNPaymentsProcessor::GetMasternodeTxOuts(const CBlockIndex* pindexPrev, const CAmount blockSubsidy,
                                                              const CAmount feeReward, std::vector<CTxOut>& ret)
{
    ret.clear();
    if (!GetBlockTxOuts(pindexPrev, blockSubsidy, feeReward, ret)) return false;
    return true;
}

[[nodiscard]] bool CMNPaymentsProcessor::IsTransactionValid(const CTransaction& txNew, const CBlockIndex* pindexPrev,
                                                             const CAmount blockSubsidy, const CAmount feeReward,
                                                             bool strict_multiplicity)
{
    const int height = pindexPrev == nullptr ? 0 : pindexPrev->nHeight + 1;
    if (!DeploymentDIP0003Enforced(height, m_consensus_params)) return true;
    std::vector<CTxOut> expected;
    if (!GetBlockTxOuts(pindexPrev, blockSubsidy, feeReward, expected)) return true;
    return FindUnmatchedMasternodePayment(expected, txNew.vout, strict_multiplicity) < 0;
}

bool CMNPaymentsProcessor::IsBlockValueValid(const CBlock& block, const CBlockIndex* pindexPrev,
                                              const CAmount blockReward, std::string& strErrorRet)
{
    const int height = pindexPrev == nullptr ? 0 : pindexPrev->nHeight + 1;
    const CAmount actual = block.vtx[0]->GetValueOut();
    strErrorRet.clear();
    if (actual > blockReward) {
        strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d)", height, actual, blockReward);
        return false;
    }
    return true;
}

bool CMNPaymentsProcessor::IsBlockPayeeValid(const CTransaction& txNew, const CBlockIndex* pindexPrev,
                                              const CAmount blockSubsidy, const CAmount feeReward,
                                              bool strict_multiplicity)
{
    return IsTransactionValid(txNew, pindexPrev, blockSubsidy, feeReward, strict_multiplicity);
}

static void SubtractMasternodePaymentFromCoinstake(CMutableTransaction& txCoinStake, CAmount payment, size_t stakerOutputs)
{
    assert(stakerOutputs >= 2);
    assert(txCoinStake.vout.size() >= stakerOutputs);
    if (stakerOutputs == 2) {
        txCoinStake.vout[1].nValue -= payment;
        return;
    }
    const size_t outputs = stakerOutputs - 1;
    const CAmount split = payment / outputs;
    const CAmount remainder = payment - split * outputs;
    for (size_t i = 1; i <= outputs; ++i) txCoinStake.vout[i].nValue -= split;
    txCoinStake.vout[outputs].nValue -= remainder;
}

void CMNPaymentsProcessor::FillBlockPayments(CMutableTransaction& txNew, const CBlockIndex* pindexPrev,
                                              const CAmount blockSubsidy, const CAmount feeReward,
                                              bool fProofOfStake, std::vector<CTxOut>& ret)
{
    if (!GetMasternodeTxOuts(pindexPrev, blockSubsidy, feeReward, ret)) return;
    CAmount payment{0};
    for (const auto& out : ret) payment += out.nValue;
    const size_t originalOutputs = txNew.vout.size();
    txNew.vout.insert(txNew.vout.end(), ret.begin(), ret.end());
    if (payment > 0) {
        if (fProofOfStake) SubtractMasternodePaymentFromCoinstake(txNew, payment, originalOutputs);
        else {
            assert(!txNew.vout.empty());
            txNew.vout[0].nValue -= payment;
        }
    }
}