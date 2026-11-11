// Copyright (c) 2021-2022 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_TXORPHANAGE_H
#define SCHILLINGCOIN_TXORPHANAGE_H

#include <net.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <sync.h>

#include <map>
#include <set>
#include <vector>

/** A class to track orphan transactions (failed on TX_MISSING_INPUTS)
 * Since we cannot distinguish orphans from bad transactions with
 * non-existent inputs, we heavily limit the number of orphans
 * we keep and the duration we keep them for.
 */
class TxOrphanage {
public:
    /** Add a new orphan transaction. */
    bool AddTx(const CTransactionRef& tx, NodeId peer) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

    /** Check if we already have an orphan transaction, by wtxid only. */
    bool HaveTx(const Wtxid& wtxid) const EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

    /** Extract a transaction from a peer's work set.
     * Returns nullptr if there are no transactions to work on.
     * Otherwise returns the transaction reference and removes it from the work set.
     */
    CTransactionRef GetTxToReconsider(NodeId peer) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

    /** Determine orphan transactions that may be reconsidered after a block is connected. */
    void SetCandidatesByBlock(const CBlock& block) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

    /** Erase an orphan by wtxid. */
    int EraseTx(const Wtxid& wtxid) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

    /** Erase all orphans announced by a peer. */
    void EraseForPeer(NodeId peer) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

    /** Erase all orphans included in or invalidated by a new block. */
    void EraseForBlock(const CBlock& block) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

    /** Check whether a peer has more orphan transactions to reconsider. */
    bool HaveMoreWork(NodeId peer) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

    /** Limit total orphan transaction storage to the given number of bytes. */
    void LimitOrphans(unsigned int max_orphans_size) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

    /** Add orphan children of a transaction to their source peers' work sets. */
    void AddChildrenToWorkSet(const CTransaction& tx) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

    /** Check whether a peer has an orphan ready for reconsideration. */
    bool HaveTxToReconsider(NodeId peer) EXCLUSIVE_LOCKS_REQUIRED(!m_mutex);

    /** Return the number of orphan transactions. */
    size_t Size() EXCLUSIVE_LOCKS_REQUIRED(!m_mutex)
    {
        LOCK(m_mutex);
        return m_orphans.size();
    }

protected:
    /** Guards orphan transactions. */
    mutable Mutex m_mutex;

    struct OrphanTx {
        CTransactionRef tx;
        NodeId fromPeer;
        int64_t nTimeExpire;
        size_t list_pos;
        size_t nTxSize;
    };

    /** Map from wtxid to orphan transaction record. */
    std::map<Wtxid, OrphanTx> m_orphans GUARDED_BY(m_mutex);

    /** Orphan wtxids queued for reconsideration, grouped by source peer. */
    std::map<NodeId, std::set<Wtxid>> m_peer_work_set GUARDED_BY(m_mutex);

    using OrphanMap = decltype(m_orphans);

    struct IteratorComparator
    {
        template <typename I>
        bool operator()(const I& a, const I& b) const
        {
            return &(*a) < &(*b);
        }
    };

    /** Index from parent outpoints to orphan-map entries. */
    std::map<COutPoint, std::set<OrphanMap::iterator, IteratorComparator>> m_outpoint_to_orphan_it GUARDED_BY(m_mutex);

    /** Orphan transactions in a vector for quick random eviction. */
    std::vector<OrphanMap::iterator> m_orphan_list GUARDED_BY(m_mutex);

    /** Cumulative serialized size of all transactions in the orphan map. */
    size_t m_orphan_tx_size GUARDED_BY(m_mutex){0};

    /** Erase an orphan by wtxid while m_mutex is held. */
    int EraseTxNoLock(const Wtxid& wtxid) EXCLUSIVE_LOCKS_REQUIRED(m_mutex);
};

#endif // SCHILLINGCOIN_TXORPHANAGE_H