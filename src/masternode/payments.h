// Copyright (c) 2014-2024 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_MASTERNODE_PAYMENTS_H
#define SCHILLINGCOIN_MASTERNODE_PAYMENTS_H

#include <consensus/amount.h>

#include <string>
#include <vector>

class CBlock;
class CBlockIndex;
class CChain;
class CDeterministicMNManager;
class CTransaction;
class CTxOut;

/**
 * Match the list of expected masternode payment outputs against the coinbase
 * outputs.
 *
 * When @p strict_multiplicity is true, every expected output must be matched
 * by a distinct actual output (multiplicity-correct matching): two identical
 * expected outputs require two identical actual outputs.
 *
 * When false, the legacy behaviour is used where each expected output only
 * has to appear at least once in the actual outputs. This is preserved for
 * pre-v24 historical validation.
 *
 * @return -1 if every expected output is matched, otherwise the index in
 * @p expected of the first output that could not be matched.
 */
int FindUnmatchedMasternodePayment(const std::vector<CTxOut>& expected,
                                   const std::vector<CTxOut>& actual,
                                   bool strict_multiplicity);

struct CMutableTransaction;

namespace Consensus { struct Params; }

CAmount GetMasternodePayment(int nHeight, CAmount blockValue, const Consensus::Params& consensus_params);

class CMNPaymentsProcessor
{
private:
    CDeterministicMNManager& m_dmnman;
    const Consensus::Params& m_consensus_params;

private:
    [[nodiscard]] bool GetBlockTxOuts(const CBlockIndex* pindexPrev, const CAmount blockSubsidy, const CAmount feeReward,
                                      std::vector<CTxOut>& voutMasternodePaymentsRet);
    [[nodiscard]] bool GetMasternodeTxOuts(const CBlockIndex* pindexPrev, const CAmount blockSubsidy, const CAmount feeReward,
                                           std::vector<CTxOut>& voutMasternodePaymentsRet);
    [[nodiscard]] bool IsTransactionValid(const CTransaction& txNew, const CBlockIndex* pindexPrev, const CAmount blockSubsidy,
                                          const CAmount feeReward, bool strict_multiplicity);

public:
    explicit CMNPaymentsProcessor(CDeterministicMNManager& dmnman,
                                  const Consensus::Params& consensus_params) :
        m_dmnman{dmnman},
        m_consensus_params{consensus_params}
    {
    }

    bool IsBlockValueValid(const CBlock& block, const CBlockIndex* pindexPrev, const CAmount blockReward, std::string& strErrorRet);
    bool IsBlockPayeeValid(const CTransaction& txNew, const CBlockIndex* pindexPrev, const CAmount blockSubsidy, const CAmount feeReward,
                           bool strict_multiplicity);
    void FillBlockPayments(CMutableTransaction& txNew, const CBlockIndex* pindexPrev, const CAmount blockSubsidy, const CAmount feeReward,
                           bool fProofOfStake, std::vector<CTxOut>& voutMasternodePaymentsRet);
};

#endif // SCHILLINGCOIN_MASTERNODE_PAYMENTS_H