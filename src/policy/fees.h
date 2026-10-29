// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_POLICY_FEES_H
#define SCHILLINGCOIN_POLICY_FEES_H

#include "consensus/amount.h"
#include "policy/feerate.h"
#include "random.h"
#include "sync.h"
#include "uint256.h"
#include "util/fees.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class CAutoFile;
class CFeeRate;
class CTxMemPool;
class CTxMemPoolEntry;
class TxConfirmStats;

/*
 * Identifier for each of the three TxConfirmStats instances that track
 * fee-estimation history over different time horizons.
 */
enum class FeeEstimateHorizon {
    SHORT_HALFLIFE = 0,
    MED_HALFLIFE = 1,
    LONG_HALFLIFE = 2,
};

std::string StringForFeeEstimateHorizon(FeeEstimateHorizon horizon);

/* Enumeration of reason for returned fee estimate */
enum class FeeReason {
    NONE,
    HALF_ESTIMATE,
    FULL_ESTIMATE,
    DOUBLE_ESTIMATE,
    CONSERVATIVE,
    MEMPOOL_MIN,
    PAYTXFEE,
    FALLBACK,
    REQUIRED,
};

/** Used to return detailed information about a feerate bucket. */
struct EstimatorBucket
{
    double start{-1};
    double end{-1};
    double withinTarget{0};
    double totalConfirmed{0};
    double inMempool{0};
    double leftMempool{0};
};

/** Used to return detailed information about a fee-estimate calculation. */
struct EstimationResult
{
    EstimatorBucket pass;
    EstimatorBucket fail;
    double decay{0};
    unsigned int scale{0};
};

struct FeeCalculation
{
    EstimationResult est;
    FeeReason reason{FeeReason::NONE};
    int desiredTarget{0};
    int returnedTarget{0};
};

/**
 * The block-policy estimator tracks how long transactions with different
 * feerates take to confirm. It maintains short, medium, and long estimation
 * horizons so callers can request either economical or conservative fee
 * estimates.
 */
class CBlockPolicyEstimator
{
private:
    /** Track confirmation delays up to 12 blocks for the short horizon. */
    static constexpr unsigned int SHORT_BLOCK_PERIODS = 12;
    static constexpr unsigned int SHORT_SCALE = 1;

    /** Track confirmation delays up to 48 blocks for the medium horizon. */
    static constexpr unsigned int MED_BLOCK_PERIODS = 24;
    static constexpr unsigned int MED_SCALE = 2;

    /** Track confirmation delays up to 1008 blocks for the long horizon. */
    static constexpr unsigned int LONG_BLOCK_PERIODS = 42;
    static constexpr unsigned int LONG_SCALE = 24;

    /** Historical estimates older than this are not considered valid. */
    static constexpr unsigned int OLDEST_ESTIMATE_HISTORY = 6 * 1008;

    /** Short-horizon decay, approximately an 18-block half-life. */
    static constexpr double SHORT_DECAY = .962;

    /** Medium-horizon decay, approximately a 144-block half-life. */
    static constexpr double MED_DECAY = .9952;

    /** Long-horizon decay, approximately a 1008-block half-life. */
    static constexpr double LONG_DECAY = .99931;

    /** Require at least 60% confirmation success within half the target. */
    static constexpr double HALF_SUCCESS_PCT = .6;

    /** Require at least 85% confirmation success within the target. */
    static constexpr double SUCCESS_PCT = .85;

    /** Require at least 95% confirmation success within twice the target. */
    static constexpr double DOUBLE_SUCCESS_PCT = .95;

    /** Required average transaction count for statistical significance. */
    static constexpr double SUFFICIENT_FEETXS = .1;

    /** Higher sample requirement for the short-decay estimator. */
    static constexpr double SUFFICIENT_TXS_SHORT = .5;

    /**
     * Minimum and maximum feerates tracked by the estimator.
     *
     * The minimum remains 1000 to preserve the donor estimator's historical
     * bucket boundary and fee-estimate file behavior.
     */
    static constexpr double MIN_BUCKET_FEERATE = 1000;
    static constexpr double MAX_BUCKET_FEERATE = 1e7;

    /** Exponential spacing used between estimator feerate buckets. */
    static constexpr double FEE_SPACING = 1.05;

public:
    /** Construct the estimator and initialize its tracking horizons. */
    CBlockPolicyEstimator();

    ~CBlockPolicyEstimator();

    /** Process transactions included in a newly connected block. */
    void processBlock(
        unsigned int nBlockHeight,
        std::vector<const CTxMemPoolEntry*>& entries)
        EXCLUSIVE_LOCKS_REQUIRED(!m_cs_fee_estimator);

    /** Process a transaction accepted into the mempool. */
    void processTransaction(
        const CTxMemPoolEntry& entry,
        bool validFeeEstimate)
        EXCLUSIVE_LOCKS_REQUIRED(!m_cs_fee_estimator);

    /**
     * Remove a transaction from estimator tracking.
     *
     * The inBlock argument distinguishes a mined transaction from a
     * transaction that left the mempool without confirmation.
     */
    bool removeTx(uint256 hash, bool inBlock)
        EXCLUSIVE_LOCKS_REQUIRED(!m_cs_fee_estimator);

    /** Return the legacy feerate estimate for a confirmation target. */
    CFeeRate estimateFee(int confTarget) const
        EXCLUSIVE_LOCKS_REQUIRED(!m_cs_fee_estimator);

    /**
     * Estimate the feerate required for confirmation within confTarget blocks.
     *
     * Conservative estimates remain valid over longer estimation horizons.
     */
    CFeeRate estimateSmartFee(
        int confTarget,
        FeeCalculation* feeCalc,
        bool conservative) const
        EXCLUSIVE_LOCKS_REQUIRED(!m_cs_fee_estimator);

    /**
     * Return a raw estimate for a target, success threshold, and horizon.
     */
    CFeeRate estimateRawFee(
        int confTarget,
        double successThreshold,
        FeeEstimateHorizon horizon,
        EstimationResult* result = nullptr) const
        EXCLUSIVE_LOCKS_REQUIRED(!m_cs_fee_estimator);

    /** Write estimator data to disk. */
    bool Write(CAutoFile& fileout) const
        EXCLUSIVE_LOCKS_REQUIRED(!m_cs_fee_estimator);

    /** Read estimator data from disk. */
    bool Read(CAutoFile& filein)
        EXCLUSIVE_LOCKS_REQUIRED(!m_cs_fee_estimator);

    /**
     * Record currently unconfirmed tracked transactions as having failed to
     * confirm before estimator shutdown.
     */
    void FlushUnconfirmed()
        EXCLUSIVE_LOCKS_REQUIRED(!m_cs_fee_estimator);

    /** Return the highest confirmation target tracked by a horizon. */
    unsigned int HighestTargetTracked(FeeEstimateHorizon horizon) const
        EXCLUSIVE_LOCKS_REQUIRED(!m_cs_fee_estimator);

private:
    mutable Mutex m_cs_fee_estimator;

    unsigned int nBestSeenHeight GUARDED_BY(m_cs_fee_estimator);
    unsigned int firstRecordedHeight GUARDED_BY(m_cs_fee_estimator);
    unsigned int historicalFirst GUARDED_BY(m_cs_fee_estimator);
    unsigned int historicalBest GUARDED_BY(m_cs_fee_estimator);

    struct TxStatsInfo
    {
        unsigned int blockHeight;
        unsigned int bucketIndex;

        TxStatsInfo() : blockHeight(0), bucketIndex(0) {}
    };

    /** Map transaction IDs to their estimator tracking information. */
    std::map<uint256, TxStatsInfo> mapMemPoolTxs
        GUARDED_BY(m_cs_fee_estimator);

    /** Medium, short, and long confirmation-statistics trackers. */
    std::unique_ptr<TxConfirmStats> feeStats
        PT_GUARDED_BY(m_cs_fee_estimator);

    std::unique_ptr<TxConfirmStats> shortStats
        PT_GUARDED_BY(m_cs_fee_estimator);

    std::unique_ptr<TxConfirmStats> longStats
        PT_GUARDED_BY(m_cs_fee_estimator);

    unsigned int trackedTxs GUARDED_BY(m_cs_fee_estimator);
    unsigned int untrackedTxs GUARDED_BY(m_cs_fee_estimator);

    /** Upper boundary of each estimator feerate bucket. */
    std::vector<double> buckets GUARDED_BY(m_cs_fee_estimator);

    /** Map bucket upper boundaries to their vector indexes. */
    std::map<double, unsigned int> bucketMap
        GUARDED_BY(m_cs_fee_estimator);

    /** Process a transaction confirmed in a block. */
    bool processBlockTx(
        unsigned int nBlockHeight,
        const CTxMemPoolEntry* entry)
        EXCLUSIVE_LOCKS_REQUIRED(m_cs_fee_estimator);

    /** Helper used by estimateSmartFee(). */
    double estimateCombinedFee(
        unsigned int confTarget,
        double successThreshold,
        bool checkShorterHorizon,
        EstimationResult* result) const
        EXCLUSIVE_LOCKS_REQUIRED(m_cs_fee_estimator);

    /** Conservative-estimation helper used by estimateSmartFee(). */
    double estimateConservativeFee(
        unsigned int doubleTarget,
        EstimationResult* result) const
        EXCLUSIVE_LOCKS_REQUIRED(m_cs_fee_estimator);

    /** Number of blocks represented by the current estimator data. */
    unsigned int BlockSpan() const
        EXCLUSIVE_LOCKS_REQUIRED(m_cs_fee_estimator);

    /** Number of blocks represented by loaded historical data. */
    unsigned int HistoricalBlockSpan() const
        EXCLUSIVE_LOCKS_REQUIRED(m_cs_fee_estimator);

    /** Highest target for which the available data is usable. */
    unsigned int MaxUsableEstimate() const
        EXCLUSIVE_LOCKS_REQUIRED(m_cs_fee_estimator);

    /** Non-locking helper called after acquiring the estimator mutex. */
    bool _removeTx(const uint256& hash, bool inBlock)
        EXCLUSIVE_LOCKS_REQUIRED(m_cs_fee_estimator);
};

#endif // SCHILLINGCOIN_POLICY_FEES_H