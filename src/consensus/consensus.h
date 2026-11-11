// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2019 The PIVX developers
// Copyright (c) 2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_CONSENSUS_CONSENSUS_H
#define SCHILLINGCOIN_CONSENSUS_CONSENSUS_H

#include "consensus/amount.h"

#include <cstddef>
#include <cstdint>

inline constexpr int WITNESS_SCALE_FACTOR = 4;

/** The maximum allowed size for a serialized block, in bytes */
inline constexpr unsigned int MAX_BLOCK_SIZE_CURRENT{2'000'000};
inline constexpr unsigned int MAX_BLOCK_SIZE_LEGACY{1'000'000};

/** The maximum allowed weight for a block after the block-size increase */
inline constexpr unsigned int MAX_BLOCK_WEIGHT{MAX_BLOCK_SIZE_CURRENT * WITNESS_SCALE_FACTOR};

constexpr unsigned int MaxBlockSize(bool fDIP0002Active = true)
{
    return fDIP0002Active ? MAX_BLOCK_SIZE_CURRENT : MAX_BLOCK_SIZE_LEGACY;
}

/** The maximum allowed number of signature check operations in a block (network rule) */
static const unsigned int MAX_BLOCK_SIGOPS_CURRENT = MAX_BLOCK_SIZE_CURRENT / 50;
static const unsigned int MAX_BLOCK_SIGOPS_LEGACY = MAX_BLOCK_SIZE_LEGACY / 50;

constexpr unsigned int MaxBlockSigOps(bool fDIP0002Active = true)
{
    return MaxBlockSize(fDIP0002Active) / 50;
}

/** The maximum number of sigops we're willing to relay/mine in a single tx */
static const unsigned int MAX_TX_SIGOPS_CURRENT = MAX_BLOCK_SIGOPS_CURRENT / 5;
static const unsigned int MAX_TX_SIGOPS_LEGACY = MAX_BLOCK_SIGOPS_LEGACY / 5;

/** The maximum allowed size of a DIP0002 extra payload, in bytes */
static const unsigned int MAX_TX_EXTRA_PAYLOAD = 10000;

/** The minimum amount for the value of a P2CS output */
static const CAmount MIN_COLDSTAKING_AMOUNT = 1 * COIN;

/** The default maximum reorganization depth **/
static const int DEFAULT_MAX_REORG_DEPTH = 100;

inline constexpr size_t MIN_TRANSACTION_WEIGHT = WITNESS_SCALE_FACTOR * 60; // 60 is the lower bound for the size of a valid serialized CTransaction
inline constexpr size_t MIN_SERIALIZABLE_TRANSACTION_WEIGHT = WITNESS_SCALE_FACTOR * 10; // 10 is the lower bound for the size of a serialized CTransaction

/** Flags for nSequence and nLockTime locks */
enum {
    /* Interpret sequence numbers as relative lock-time constraints. */
    LOCKTIME_VERIFY_SEQUENCE = (1 << 0),

    /* Use GetMedianTimePast() instead of nTime for end point timestamp. */
    LOCKTIME_MEDIAN_TIME_PAST = (1 << 1),
};

#endif // SCHILLINGCOIN_CONSENSUS_CONSENSUS_H