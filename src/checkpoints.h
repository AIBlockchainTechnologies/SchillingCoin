// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_CHECKPOINTS_H
#define SCHILLINGCOIN_CHECKPOINTS_H

#include "uint256.h"

#include <map>

class CBlockIndex;

/** 
 * Block-chain checkpoints are compiled-in sanity checks.
 * They are updated every release or three.
 */
namespace Checkpoints
{
using MapCheckpoints = std::map<int, uint256>;

struct CCheckpointData {
    const MapCheckpoints* mapCheckpoints;
    int64_t nTimeLastCheckpoint;
    int64_t nTransactionsLastCheckpoint;
    double fTransactionsPerDay;
};

//! Returns true if block passes checkpoint checks
bool CheckBlock(int nHeight, const uint256& hash, bool fMatchesCheckpoint = false);

//! Return conservative estimate of total number of blocks, 0 if unknown
int GetTotalBlocksEstimate();

double GuessVerificationProgress(const CBlockIndex* pindex, bool fSigchecks = true);

extern bool fEnabled;

} //namespace Checkpoints

#endif // SCHILLINGCOIN_CHECKPOINTS_H