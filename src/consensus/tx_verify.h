// Copyright (c) 2017-2017 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_CONSENSUS_TX_VERIFY_H
#define SCHILLINGCOIN_CONSENSUS_TX_VERIFY_H

#include <consensus/amount.h>

#include <cstdint>
#include <utility>
#include <vector>

class CBlockIndex;
class CCoinsViewCache;
class CTransaction;
class TxValidationState;

namespace Consensus {
struct Params;

/**
 * Check whether all inputs of this transaction are valid.
 *
 * This does not modify the UTXO set and does not check scripts or signatures.
 * Historical SCH Zerocoin spends are valued through GetZerocoinSpent() and do
 * not consume ordinary UTXOs.
 *
 * @param[out] txfee Set to the transaction fee if successful.
 *
 * Preconditions: tx.IsCoinBase() is false.
 */
[[nodiscard]] bool CheckTxInputs(
    const CTransaction& tx,
    TxValidationState& state,
    const CCoinsViewCache& inputs,
    int nSpendHeight,
    const Params& consensus_params,
    CAmount& txfee);
} // namespace Consensus

/** Auxiliary functions for transaction validation. */

/**
 * Count ECDSA signature operations the old-fashioned way.
 */
unsigned int GetLegacySigOpCount(const CTransaction& tx);

/**
 * Count ECDSA signature operations in pay-to-script-hash inputs.
 */
unsigned int GetP2SHSigOpCount(
    const CTransaction& tx,
    const CCoinsViewCache& inputs);

/**
 * Count total signature operations for a transaction.
 */
unsigned int GetTransactionSigOpCount(
    const CTransaction& tx,
    const CCoinsViewCache& inputs,
    uint32_t flags);

/**
 * Check if transaction is final and can be included in a block with the
 * specified height and time.
 */
bool IsFinalTx(
    const CTransaction& tx,
    int nBlockHeight,
    int64_t nBlockTime);

/**
 * Calculate BIP68 sequence locks.
 */
std::pair<int, int64_t> CalculateSequenceLocks(
    const CTransaction& tx,
    int flags,
    std::vector<int>& prevHeights,
    const CBlockIndex& block);

bool EvaluateSequenceLocks(
    const CBlockIndex& block,
    std::pair<int, int64_t> lockPair);

/**
 * Check whether transaction sequence locks permit inclusion in block.
 */
bool SequenceLocks(
    const CTransaction& tx,
    int flags,
    std::vector<int>& prevHeights,
    const CBlockIndex& block);

#endif // SCHILLINGCOIN_CONSENSUS_TX_VERIFY_H