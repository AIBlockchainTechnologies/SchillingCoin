// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_CONSENSUS_PARAMS_H
#define SCHILLINGCOIN_CONSENSUS_PARAMS_H

#include <consensus/amount.h>
#include <llmq/params.h>
#include <uint256.h>

#include <chrono>
#include <limits>
#include <string>
#include <vector>

namespace libzerocoin {
class ZerocoinParams;
} // namespace libzerocoin

namespace Consensus {

/**
 * A buried deployment is one where the height of the activation has been hardcoded into
 * the client implementation long after the consensus change has activated.
 */
enum BuriedDeployment : int16_t {
    // Buried deployments get negative values to avoid overlap with DeploymentPos.
    DEPLOYMENT_HEIGHTINCB = std::numeric_limits<int16_t>::min(),
    DEPLOYMENT_DERSIG,
    DEPLOYMENT_CLTV,
    DEPLOYMENT_BIP147,
    DEPLOYMENT_CSV,
    DEPLOYMENT_DIP0001,
    DEPLOYMENT_DIP0003,
    DEPLOYMENT_DIP0008,
    DEPLOYMENT_DIP0020,
    DEPLOYMENT_DIP0024,
    DEPLOYMENT_BRR,
    DEPLOYMENT_V19,
    DEPLOYMENT_V20,
    DEPLOYMENT_MN_RR,
    DEPLOYMENT_WITHDRAWALS,
};

constexpr bool ValidDeployment(BuriedDeployment dep)
{
    return dep <= DEPLOYMENT_WITHDRAWALS;
}

enum DeploymentPos : uint16_t {
    DEPLOYMENT_TESTDUMMY,
    DEPLOYMENT_V24,
    DEPLOYMENT_EVO_SHARES,

    // NOTE: Also add new deployments to VersionBitsDeploymentInfo in deploymentinfo.cpp.
    MAX_VERSION_BITS_DEPLOYMENTS
};

constexpr bool ValidDeployment(DeploymentPos dep)
{
    return dep < MAX_VERSION_BITS_DEPLOYMENTS;
}

/** Parameters for an individual version-bits deployment. */
struct BIP9Deployment {
    int bit{28};
    int64_t nStartTime{NEVER_ACTIVE};
    int64_t nTimeout{NEVER_ACTIVE};
    int min_activation_height{0};

    int64_t nWindowSize{0};
    int64_t nThresholdStart{0};
    int64_t nThresholdMin{0};
    int64_t nFalloffCoeff{0};

    bool useEHF{false};

    static constexpr int64_t NO_TIMEOUT = std::numeric_limits<int64_t>::max();
    static constexpr int64_t ALWAYS_ACTIVE = -1;
    static constexpr int64_t NEVER_ACTIVE = -2;
};

/**
 * Parameters that influence chain consensus.
 *
 * Modern deployment and LLMQ fields follow the authoritative Dash ownership
 * model. Historical SCH fields remain here because they are consensus-critical
 * for the existing Proof-of-Work/Proof-of-Stake chain.
 */
struct Params {
    uint256 hashGenesisBlock;
    uint256 hashDevnetGenesisBlock;

    /*
     * Modern buried-deployment activation heights.
     */
    int BIP34Height;
    uint256 BIP34Hash;

    int BIP65Height;
    int BIP66Height;
    int BIP147Height;
    int CSVHeight;

    int DIP0001Height;

    int DIP0003Height;
    int DIP0003EnforcementHeight;
    uint256 DIP0003EnforcementHash;

    int DIP0008Height;
    int BRRHeight;
    int DIP0020Height;
    int DIP0024Height;
    int DIP0024QuorumsHeight;
    int V19Height;
    int V20Height;
    int MN_RRHeight;
    int WithdrawalsHeight;

    int MinBIP9WarningHeight;

    uint32_t nRuleChangeActivationThreshold;
    uint32_t nMinerConfirmationWindow;

    BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];

    /*
     * Historical SCH Proof-of-Work and Proof-of-Stake consensus.
     */
    bool fPowAllowMinDifficultyBlocks;
    bool fPowNoRetargeting;

    uint256 powLimit;
    uint256 posLimitV1;
    uint256 posLimitV2;

    int nCoinbaseMaturity;
    int nFutureTimeDriftPoW;
    int nFutureTimeDriftPoS;

    CAmount nMaxMoneyOut;

    int nStakeMinAge;
    int nStakeMinDepth;

    int64_t nTargetTimespan;
    int64_t nTargetTimespanV2;
    int64_t nTargetSpacing;
    int nTimeSlotLength;

    std::chrono::seconds PowTargetSpacing() const
    {
        return std::chrono::seconds{nTargetSpacing};
    }

    /*
     * Historical SCH activation heights.
     */
    int height_last_PoW;
    int height_last_ZC_AccumCheckpoint;
    int height_start_BIP65;
    int height_start_MessSignaturesV2;
    int height_start_StakeModifierNewSelection;
    int height_start_StakeModifierV2;
    int height_start_TimeProtoV2;
    int height_start_ZC;
    int height_start_ZC_PublicSpends;
    int height_start_ZC_SerialRangeCheck;
    int height_start_ZC_SerialsV2;
    int height_RHF;

    /*
     * SCH transaction/reward modernization activations.
     */
    int DIP0002Height;
    int FeeRewardDistributionHeight;

    /*
     * Legacy SCH masternode/economic parameters still referenced by the
     * existing SCH implementation.
     */
    int nBudgetCycleBlocks;
    int nBudgetFeeConfirmations;
    int nMasternodeCountDrift;
    int nPoolMaxTransactions;
    int64_t nProposalEstablishmentTime;
    std::string strObfuscationPoolDummyAddress;

    /*
     * Minimum-chain-work and assume-valid infrastructure.
     */
    uint256 nMinimumChainWork;
    uint256 defaultAssumeValid;

    /*
     * LLMQ consensus configuration.
     */
    std::vector<LLMQParams> llmqs;

    LLMQType llmqTypeChainLocks{LLMQType::LLMQ_NONE};
    LLMQType llmqTypeDIP0024InstantSend{LLMQType::LLMQ_NONE};
    LLMQType llmqTypePlatform{LLMQType::LLMQ_NONE};
    LLMQType llmqTypeMnhf{LLMQType::LLMQ_NONE};

    /*
     * Historical SCH Proof-of-Stake helpers.
     */
    int64_t TargetTimespan(const bool fV2 = true) const
    {
        return fV2 ? nTargetTimespanV2 : nTargetTimespan;
    }

    uint256 ProofOfStakeLimit(const bool fV2) const
    {
        return fV2 ? posLimitV2 : posLimitV1;
    }

    bool MoneyRange(const CAmount& nValue) const
    {
        return nValue >= 0 && nValue <= nMaxMoneyOut;
    }

    bool IsMessSigV2(const int nHeight) const
    {
        return nHeight >= height_start_MessSignaturesV2;
    }

    bool IsPastRHFBlock(const int nHeight) const
    {
        return nHeight >= height_RHF;
    }

    bool IsDIP0002Active(const int nHeight) const
    {
        return nHeight >= DIP0002Height;
    }

    bool IsFeeRewardDistributionActive(const int nHeight) const
    {
        return nHeight >= FeeRewardDistributionHeight;
    }

    bool IsTimeProtocolV2(const int nHeight) const
    {
        return nHeight >= height_start_TimeProtoV2;
    }

    bool IsStakeModifierV2(const int nHeight) const
    {
        return nHeight >= height_start_StakeModifierV2;
    }

    int FutureBlockTimeDrift(const int nHeight) const
    {
        if (IsTimeProtocolV2(nHeight)) return nTimeSlotLength - 1;
        return nHeight > height_last_PoW ? nFutureTimeDriftPoS : nFutureTimeDriftPoW;
    }

    bool IsValidBlockTimeStamp(const int64_t nTime, const int nHeight) const
    {
        if (!IsTimeProtocolV2(nHeight)) return true;
        return (nTime % nTimeSlotLength) == 0;
    }

    bool HasStakeMinAgeOrDepth(
        const int contextHeight,
        const uint32_t contextTime,
        const int utxoFromBlockHeight,
        const uint32_t utxoFromBlockTime) const
    {
        if (!IsStakeModifierV2(contextHeight)) {
            return utxoFromBlockTime + nStakeMinAge <= contextTime;
        }

        return contextHeight - utxoFromBlockHeight >= nStakeMinDepth;
    }

    /*
     * Bare-minimum historical Zerocoin consensus compatibility.
     *
     * Runtime Zerocoin functionality remains retired. These values are retained
     * only where required for historical SCH chain interpretation.
     */
    std::string ZC_Modulus;
    int ZC_MaxPublicSpendsPerTx;
    int ZC_MaxSpendsPerTx;
    int ZC_MinMintConfirmations;
    CAmount ZC_MinMintFee;
    int ZC_MinStakeDepth;
    int ZC_TimeStart;

    libzerocoin::ZerocoinParams* Zerocoin_Params(bool) const
    {
        return nullptr;
    }

    /*
     * Authoritative buried-deployment height lookup.
     */
    int DeploymentHeight(BuriedDeployment dep) const
    {
        switch (dep) {
        case DEPLOYMENT_HEIGHTINCB:
            return BIP34Height;
        case DEPLOYMENT_DERSIG:
            return BIP66Height;
        case DEPLOYMENT_CLTV:
            return BIP65Height;
        case DEPLOYMENT_BIP147:
            return BIP147Height;
        case DEPLOYMENT_CSV:
            return CSVHeight;
        case DEPLOYMENT_DIP0001:
            return DIP0001Height;
        case DEPLOYMENT_DIP0003:
            return DIP0003Height;
        case DEPLOYMENT_DIP0008:
            return DIP0008Height;
        case DEPLOYMENT_DIP0020:
            return DIP0020Height;
        case DEPLOYMENT_DIP0024:
            return DIP0024Height;
        case DEPLOYMENT_BRR:
            return BRRHeight;
        case DEPLOYMENT_V19:
            return V19Height;
        case DEPLOYMENT_V20:
            return V20Height;
        case DEPLOYMENT_MN_RR:
            return MN_RRHeight;
        case DEPLOYMENT_WITHDRAWALS:
            return WithdrawalsHeight;
        }

        return std::numeric_limits<int>::max();
    }
};

} // namespace Consensus

#endif // SCHILLINGCOIN_CONSENSUS_PARAMS_H
