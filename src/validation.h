// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2014-2025 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_VALIDATION_H
#define SCHILLINGCOIN_VALIDATION_H

class CBlock;
class CBlockIndex;
class CBlockLocator;
class CChain;
class CTransaction;
class CTxMemPool;
class CValidationState;
class uint256;

struct CDiskBlockPos;

/**
 * Process an incoming block through SCH's existing validation pipeline.
 *
 * The implementation remains in main.cpp during the limited validation split
 * and will move during the later complete validation modernization.
 */
bool ProcessNewBlock(
    CValidationState& state,
    CBlock* pblock,
    CDiskBlockPos* dbp = nullptr);

/**
 * Attempt to add a transaction to the memory pool.
 *
 * The implementation remains in main.cpp during the limited validation split
 * and will move during the later complete validation modernization.
 */
bool AcceptToMemoryPool(
    CTxMemPool& pool,
    CValidationState& state,
    const CTransaction& tx,
    bool fLimitFree,
    bool* pfMissingInputs,
    bool fRejectInsaneFee = false,
    bool ignoreFees = false);

/**
 * Retrieve a transaction from the memory pool, transaction index, or block
 * data when slow lookup is permitted.
 *
 * The implementation remains in main.cpp during the limited validation split
 * and will move during the later complete validation modernization.
 */
bool GetTransaction(
    const uint256& hash,
    CTransaction& tx,
    uint256& hashBlock,
    bool fAllowSlow = false,
    CBlockIndex* blockIndex = nullptr);

/** Return whether the node is currently in initial block download. */
bool IsInitialBlockDownload();

/** Find the last common block between the parameter chain and a locator. */
CBlockIndex* FindForkInGlobalIndex(
    const CChain& chain,
    const CBlockLocator& locator);

#endif // SCHILLINGCOIN_VALIDATION_H
