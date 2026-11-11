// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_TXMEMPOOL_H
#define SCHILLINGCOIN_TXMEMPOOL_H

#include <atomic>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <kernel/mempool_limits.h>
#include <kernel/mempool_options.h>

#include <arith_uint256.h>
#include <coins.h>
#include <consensus/amount.h>
#include <evo/netinfo.h>
#include <index/addressindex_types.h>
#include <index/spentindex_types.h>
#include <indirectmap.h>
#include <kernel/cs_main.h>
#include <kernel/mempool_entry.h>
#include <netaddress.h>
#include <policy/feerate.h>
#include <policy/packages.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <random.h>
#include <sync.h>
#include <txgraph.h>
#include <util/feefrac.h>
#include <util/hasher.h>
#include <util/result.h>
#include <util/time.h>

#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container.hpp>

class CBlockIndex;
class CChain;
class Chainstate;

namespace Consensus {
struct Params;
}

// Forward declaration for CBLSLazyPublicKey:
template<typename T> class CBLSLazyWrapper;
class CBLSPublicKey;
using CBLSLazyPublicKey = CBLSLazyWrapper<CBLSPublicKey>;

/** Fake height value used in Coin to signify they are only in the memory pool (since 0.8) */
inline constexpr uint32_t MEMPOOL_HEIGHT = 0x7FFFFFFF;

/** How much linearization cost required for TxGraph clusters to have
 * "acceptable" quality, if they cannot be optimally linearized with less cost. */
inline constexpr uint64_t ACCEPTABLE_COST = 75'000;

/** How much work we ask TxGraph to do after a mempool change occurs (either
 * due to a changeset being applied, a new block being found, or a reorg). */
inline constexpr uint64_t POST_CHANGE_COST = 5 * ACCEPTABLE_COST;

/**
 * Test whether the LockPoints height and time are still valid on the current chain
 */
bool TestLockPointValidity(CChain& active_chain, const LockPoints& lp) EXCLUSIVE_LOCKS_REQUIRED(cs_main);

// extracts a transaction hash from CTxMemPoolEntry or CTransactionRef
struct mempoolentry_txid
{
    typedef Txid result_type;
    result_type operator() (const CTxMemPoolEntry &entry) const
    {
        return entry.GetTx().GetHash();
    }

    result_type operator() (const CTransactionRef& tx) const
    {
        return tx->GetHash();
    }
};

// extracts a transaction witness-hash from CTxMemPoolEntry or CTransactionRef
struct mempoolentry_wtxid
{
    typedef Wtxid result_type;
    result_type operator() (const CTxMemPoolEntry &entry) const
    {
        return entry.GetTx().GetWitnessHash();
    }

    result_type operator() (const CTransactionRef& tx) const
    {
        return tx->GetWitnessHash();
    }
};

class CompareTxMemPoolEntryByEntryTime
{
public:
    bool operator()(const CTxMemPoolEntry& a, const CTxMemPoolEntry& b) const
    {
        return a.GetTime() < b.GetTime();
    }
};

// Multi_index tag names
struct entry_time {};
struct index_by_wtxid {};

class CBlockPolicyEstimator;
class CDeterministicMNManager;

namespace llmq {
class CInstantSendManager;
};

/**
 * Information about a mempool transaction.
 */
struct TxMempoolInfo
{
    /** The transaction itself */
    CTransactionRef tx;

    /** Time the transaction entered the mempool. */
    std::chrono::seconds m_time;

    /** Fee of the transaction. */
    CAmount fee;

    /** Virtual size of the transaction. */
    size_t vsize;

    /** The fee delta. */
    int64_t nFeeDelta;
};

/** Reason why a transaction was removed from the mempool,
 * this is passed to the notification signal.
 */
enum class MemPoolRemovalReason {
    EXPIRY,      //!< Expired from mempool
    SIZELIMIT,   //!< Removed in size limiting
    REORG,       //!< Removed for reorganization
    BLOCK,       //!< Removed for block
    CONFLICT,    //!< Removed for conflict with in-block transaction
    MANUAL       //!< Removed manually
};

std::string RemovalReasonToString(const MemPoolRemovalReason& r) noexcept;

/**
 * CTxMemPool stores valid-according-to-the-current-best-chain transactions
 * that may be included in the next block.
 *
 * Transactions are added when they are seen on the network (or created by the
 * local node), but not all transactions seen are added to the pool. For
 * example, the following new transactions will not be added to the mempool:
 * - a transaction which doesn't meet the minimum fee requirements.
 * - a new transaction that double-spends an input of a transaction already in
 * the pool.
 * - a non-standard transaction.
 *
 * CTxMemPool::mapTx, and CTxMemPoolEntry bookkeeping:
 *
 * mapTx is a boost::multi_index that sorts the mempool on 4 criteria:
 * - transaction hash
 * - descendant feerate [we use max(feerate of tx, feerate of tx with all descendants)]
 * - time in mempool
 * - ancestor feerate [we use min(feerate of tx, feerate of tx with all unconfirmed ancestors)]
 *
 * Note: the term "descendant" refers to in-mempool transactions that depend on
 * this one, while "ancestor" refers to in-mempool transactions that a given
 * transaction depends on.
 *
 * In order for the feerate sort to remain correct, we must update transactions
 * in the mempool when new descendants arrive.  To facilitate this, we track
 * the set of in-mempool direct parents and direct children in mapLinks.  Within
 * each CTxMemPoolEntry, we track the size and fees of all descendants.
 *
 * Usually when a new transaction is added to the mempool, it has no in-mempool
 * children (because any such children would be an orphan).  So in
 * addUnchecked(), we:
 * - update a new entry's setMemPoolParents to include all in-mempool parents
 * - update the new entry's direct parents to include the new tx as a child
 * - update all ancestors of the transaction to include the new tx's size/fee
 *
 * When a transaction is removed from the mempool, we must:
 * - update all in-mempool parents to not track the tx in setMemPoolChildren
 * - update all ancestors to not include the tx's size/fees in descendant state
 * - update all in-mempool children to not include it as a parent
 *
 * These happen in UpdateForRemoveFromMempool().  (Note that when removing a
 * transaction along with its descendants, we must calculate that set of
 * transactions to be removed before doing the removal, or else the mempool can
 * be in an inconsistent state where it's impossible to walk the ancestors of
 * a transaction.)
 *
 * In the event of a reorg, the assumption that a newly added tx has no
 * in-mempool children is false.  In particular, the mempool is in an
 * inconsistent state while new transactions are being added, because there may
 * be descendant transactions of a tx coming from a disconnected block that are
 * unreachable from just looking at transactions in the mempool (the linking
 * transactions may also be in the disconnected block, waiting to be added).
 * Because of this, there's not much benefit in trying to search for in-mempool
 * children in addUnchecked().  Instead, in the special case of transactions
 * being added from a disconnected block, we require the caller to clean up the
 * state, to account for in-mempool, out-of-block descendants for all the
 * in-block transactions by calling UpdateTransactionsFromBlock().  Note that
 * until this is called, the mempool state is not consistent, and in particular
 * mapLinks may not be correct (and therefore functions like
 * CalculateMemPoolAncestors() and CalculateDescendants() that rely
 * on them to walk the mempool are not generally safe to use).
 *
 * Computational limits:
 *
 * Updating all in-mempool ancestors of a newly added transaction can be slow,
 * if no bound exists on how many in-mempool ancestors there may be.
 * CalculateMemPoolAncestors() takes configurable limits that are designed to
 * prevent these calculations from being too CPU intensive.
 *
 */
class CTxMemPool
{
protected:
    const int m_check_ratio; //!< Value n means that 1 times in n we check.
    std::atomic<unsigned int> nTransactionsUpdated{0}; //!< Used by getblocktemplate to trigger CreateNewBlock() invocation
    CBlockPolicyEstimator* const minerPolicyEstimator;
    CDeterministicMNManager& m_dmnman;
    llmq::CInstantSendManager& m_isman;

    uint64_t totalTxSize GUARDED_BY(cs){0};      //!< sum of all mempool tx' byte sizes
    CAmount m_total_fee GUARDED_BY(cs){0};       //!< sum of all mempool tx's fees (NOT modified fee)
    uint64_t cachedInnerUsage GUARDED_BY(cs){0}; //!< sum of dynamic memory usage of all the map elements (NOT the maps themselves)

    mutable int64_t lastRollingFeeUpdate GUARDED_BY(cs){GetTime()};
    mutable bool blockSinceLastRollingFeeBump GUARDED_BY(cs){false};
    mutable double rollingMinimumFeeRate GUARDED_BY(cs){0}; //!< minimum fee to get into the pool, decreases exponentially

    // In-memory counter for external mempool tracking purposes.
    // This number is incremented once every time a transaction
    // is added or removed from the mempool for any reason.
    mutable uint64_t m_sequence_number GUARDED_BY(cs){1};

    void trackPackageRemoved(const CFeeRate& rate) EXCLUSIVE_LOCKS_REQUIRED(cs);

    bool m_load_tried GUARDED_BY(cs){false};

    CFeeRate GetMinFee(size_t sizelimit) const;

public:

    static const int ROLLING_FEE_HALFLIFE = 60 * 60 * 12; // public only for testing

    using indexed_transaction_set = boost::multi_index_container<
        CTxMemPoolEntry,
        boost::multi_index::indexed_by<
            // sorted by txid
            boost::multi_index::hashed_unique<mempoolentry_txid, SaltedTxidHasher>,
            // sorted by wtxid
            boost::multi_index::hashed_unique<
                boost::multi_index::tag<index_by_wtxid>,
                mempoolentry_wtxid,
                SaltedWtxidHasher
            >,
            // sorted by entry time
            boost::multi_index::ordered_non_unique<
                boost::multi_index::tag<entry_time>,
                boost::multi_index::identity<CTxMemPoolEntry>,
                CompareTxMemPoolEntryByEntryTime
            >
        >
    >;

    /**
     * This mutex needs to be locked when accessing `mapTx` or other members
     * that are guarded by it.
     *
     * @par Consistency guarantees
     * By design, it is guaranteed that:
     * 1. Locking both `cs_main` and `mempool.cs` will give a view of mempool
     *    that is consistent with current chain tip (`ActiveChain()` and
     *    `CoinsTip()`) and is fully populated. Fully populated means that if the
     *    current active chain is missing transactions that were present in a
     *    previously active chain, all the missing transactions will have been
     *    re-added to the mempool and should be present if they meet size and
     *    consistency constraints.
     * 2. Locking `mempool.cs` without `cs_main` will give a view of a mempool
     *    consistent with some chain that was active since `cs_main` was last
     *    locked, and that is fully populated as described above. It is ok for
     *    code that only needs to query or remove transactions from the mempool
     *    to lock just `mempool.cs` without `cs_main`.
     *
     * To provide these guarantees, it is necessary to lock both `cs_main` and
     * `mempool.cs` whenever adding transactions to the mempool and whenever
     * changing the chain tip. It's necessary to keep both mutexes locked until
     * the mempool is consistent with the new chain tip and fully populated.
     */
    mutable RecursiveMutex cs ACQUIRED_AFTER(::cs_main);
    std::unique_ptr<TxGraph> m_txgraph GUARDED_BY(cs);
    mutable std::unique_ptr<TxGraph::BlockBuilder> m_builder GUARDED_BY(cs);
    indexed_transaction_set mapTx GUARDED_BY(cs);

    using txiter = indexed_transaction_set::nth_index<0>::type::const_iterator;
    std::vector<std::pair<Wtxid, txiter>> txns_randomized GUARDED_BY(cs); //!< All transactions in mapTx with their wtxids, in arbitrary order

    typedef std::set<txiter, CompareIteratorByHash> setEntries;

    using Limits = kernel::MemPoolLimits;

    std::tuple<size_t, size_t, CAmount> CalculateAncestorData(const CTxMemPoolEntry& entry) const EXCLUSIVE_LOCKS_REQUIRED(cs);
    std::tuple<size_t, size_t, CAmount> CalculateDescendantData(const CTxMemPoolEntry& entry) const EXCLUSIVE_LOCKS_REQUIRED(cs);
    int64_t GetDescendantCount(txiter it) const { LOCK(cs); return m_txgraph->GetDescendants(*it, TxGraph::Level::MAIN).size(); }
    int64_t GetDescendantCount(const CTxMemPoolEntry& entry) const { LOCK(cs); return m_txgraph->GetDescendants(entry, TxGraph::Level::MAIN).size(); }
    int64_t GetAncestorCount(const CTxMemPoolEntry& entry) const { LOCK(cs); return m_txgraph->GetAncestors(entry, TxGraph::Level::MAIN).size(); }
    std::vector<CTxMemPoolEntry::CTxMemPoolEntryRef> GetChildren(const CTxMemPoolEntry& entry) const;
    std::vector<CTxMemPoolEntry::CTxMemPoolEntryRef> GetParents(const CTxMemPoolEntry& entry) const;

private:
    std::vector<indexed_transaction_set::const_iterator> GetSortedScoreWithTopology() const EXCLUSIVE_LOCKS_REQUIRED(cs);

    std::map<CMempoolAddressDeltaKey, CMempoolAddressDelta, CMempoolAddressDeltaKeyCompare> mapAddress;
    std::map<uint256, std::vector<CMempoolAddressDeltaKey>> mapAddressInserted;
    std::map<CSpentIndexKey, CSpentIndexValue, CSpentIndexKeyCompare> mapSpent;
    std::map<uint256, std::vector<CSpentIndexKey>> mapSpentInserted;

    std::multimap<uint256, uint256> mapProTxRefs; // proTxHash -> transaction (all TXs that refer to an existing proTx)
    std::map<NetInfoEntry, uint256> mapProTxAddresses;
    std::map<CKeyID, uint256> mapProTxPubKeyIDs;
    std::map<uint256, uint256> mapProTxBlsPubKeyHashes;
    std::map<COutPoint, uint256> mapProTxCollaterals;

    /**
     * Track locally submitted transactions to periodically retry initial broadcast.
     */
    std::set<Txid> m_unbroadcast_txids GUARDED_BY(cs);

    static TxMempoolInfo GetInfo(CTxMemPool::indexed_transaction_set::const_iterator it)
    {
        return TxMempoolInfo{it->GetSharedTx(), it->GetTime(), it->GetFee(), it->GetTxSize(), it->GetModifiedFee() - it->GetFee()};
    }

public:
    indirectmap<COutPoint, txiter> mapNextTx GUARDED_BY(cs);

    std::map<Txid, CAmount> mapDeltas GUARDED_BY(cs);

    using Options = kernel::MemPoolOptions;

    const int64_t m_max_size_bytes;
    const std::chrono::seconds m_expiry;
    const CFeeRate m_incremental_relay_feerate;
    const CFeeRate m_min_relay_feerate;
    const CFeeRate m_dust_relay_feerate;
    const bool m_permit_bare_multisig;
    const std::optional<unsigned> m_max_datacarrier_bytes;
    const bool m_require_standard;
    const bool m_address_index_enabled;
    const bool m_spent_index_enabled;

    const Limits m_limits;

    /** Create a new CTxMemPool.
     * Sanity checks will be off by default for performance, because otherwise
     * accepting transactions becomes O(N^2) where N is the number of transactions
     * in the pool.
     */
    explicit CTxMemPool(const Options& opts);

    /**
     * If sanity-checking is turned on, check makes sure the pool is
     * consistent (does not contain two transactions that spend the same inputs,
     * all inputs are in the mapNextTx array). If sanity-checking is turned off,
     * check does nothing.
     */
    void check(const CCoinsViewCache& active_coins_tip, int64_t spendheight, const Consensus::Params& consensus_params) const EXCLUSIVE_LOCKS_REQUIRED(::cs_main);

    void addAddressIndex(const CTxMemPoolEntry& entry, const CCoinsViewCache& view);
    void getAddressIndex(const std::vector<CMempoolAddressDeltaKey>& addresses,
                         std::vector<CMempoolAddressDeltaEntry>& results) const;
    void removeAddressIndex(const uint256 txhash);

    void addSpentIndex(const CTxMemPoolEntry& entry, const CCoinsViewCache& view);
    bool getSpentIndex(const CSpentIndexKey& key, CSpentIndexValue& value) const;
    void removeSpentIndex(const uint256 txhash);

    void removeRecursive(const CTransaction& tx, MemPoolRemovalReason reason) EXCLUSIVE_LOCKS_REQUIRED(cs);
    /** After reorg, filter the entries that would no longer be valid in the next block, and update
     * the entries' cached LockPoints if needed.  The mempool does not have any knowledge of
     * consensus rules. It just applies the callable function and removes the ones for which it
     * returns true.
     * @param[in]   filter_final_and_mature   Predicate that checks the relevant validation rules
     *                                        and updates an entry's LockPoints.
     * */
    void removeForReorg(CChain& chain, std::function<bool(txiter)> filter_final_and_mature) EXCLUSIVE_LOCKS_REQUIRED(cs, cs_main);
    void removeConflicts(const CTransaction& tx) EXCLUSIVE_LOCKS_REQUIRED(cs);
    void removeProTxPubKeyConflicts(const CTransaction &tx, const CKeyID &keyId) EXCLUSIVE_LOCKS_REQUIRED(cs);
    void removeProTxPubKeyConflicts(const CTransaction &tx, const CBLSLazyPublicKey &pubKey) EXCLUSIVE_LOCKS_REQUIRED(cs);
    void removeProTxCollateralConflicts(const CTransaction &tx, const COutPoint &collateralOutpoint) EXCLUSIVE_LOCKS_REQUIRED(cs);
    /** Remove every mempool TX that refers to proTxHash. Used when the MN it names ceases to exist
     *  (collateral spent, or collateral reused by a replacement ProRegTx), since such TXs can never
     *  be mined afterwards and would abort block assembly with "bad-protx-hash". */
    void removeProTxReferences(const uint256& proTxHash) EXCLUSIVE_LOCKS_REQUIRED(cs);
    void removeProTxSpentCollateralConflicts(const CTransaction &tx) EXCLUSIVE_LOCKS_REQUIRED(cs);
    void removeProTxKeyChangedConflicts(const CTransaction &tx, const uint256& proTxHash, const uint256& newKeyHash) EXCLUSIVE_LOCKS_REQUIRED(cs);
    /** Remove pending TXs of refType for proTxHash that pair the P2PKH destination of keyIDVoting with a
     *  share reward script: ProUpShareTxs paying it, or ProUpSharedRegTxs setting it as the voting key. */
    void removeProTxVotingPayeeConflicts(const uint256& proTxHash, const CKeyID& keyIDVoting, uint16_t refType) EXCLUSIVE_LOCKS_REQUIRED(cs);
    void removeProTxConflicts(const CTransaction &tx) EXCLUSIVE_LOCKS_REQUIRED(cs);
    void removeForBlock(const std::vector<CTransactionRef>& vtx, unsigned int nBlockHeight) EXCLUSIVE_LOCKS_REQUIRED(cs);

    bool CompareMiningScoreWithTopology(const Wtxid& hasha, const Wtxid& hashb) const;
    bool isSpent(const COutPoint& outpoint) const;
    unsigned int GetTransactionsUpdated() const;
    void AddTransactionsUpdated(unsigned int n);

    /**
     * Check that none of this transaction's inputs are in the mempool, and thus
     * the transaction is not dependent on another mempool transaction.
     */
    bool HasNoInputsOf(const CTransaction& tx) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    /** Affect CreateNewBlock prioritisation of transactions. */
    void PrioritiseTransaction(const Txid& txid, const CAmount& nFeeDelta);
    void ApplyDelta(const Txid& txid, CAmount& nFeeDelta) const EXCLUSIVE_LOCKS_REQUIRED(cs);
    void ClearPrioritisation(const Txid& txid) EXCLUSIVE_LOCKS_REQUIRED(cs);

    /** Get the transaction in the pool that spends the same prevout. */
    const CTransaction* GetConflictTx(const COutPoint& prevout) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    /** Return an iterator to the transaction identified by txid, if found. */
    std::optional<txiter> GetIter(const Txid& txid) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    /** Translate a set of transaction identifiers into mempool iterators. */
    setEntries GetIterSet(const std::set<Txid>& txids) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    /** Remove a set of transactions from the mempool.
     *  If a transaction is in this set, then all in-mempool descendants must
     *  also be in the set, unless this transaction is being removed for being
     *  in a block.
     *  Set updateDescendants to true when removing a tx that was in a block, so
     *  that any in-mempool descendants have their ancestor state updated.
     */
    void RemoveStaged(setEntries& stage, MemPoolRemovalReason reason) EXCLUSIVE_LOCKS_REQUIRED(cs);

    /** UpdateTransactionsFromBlock is called when adding transactions from a
     * disconnected block back to the mempool, new mempool entries may have
     * children in the mempool (which is generally not the case when otherwise
     * adding transactions).
     *  @post updated descendant state for descendants of each transaction in
     *        vHashesToUpdate (excluding any child transactions present in
     *        vHashesToUpdate, which are already accounted for). Updated state
     *        includes add fee/size information for such descendants to the
     *        parent and updated ancestor state to include the parent.
     *
     * @param[in] vHashesToUpdate          The set of txids from the
     *     disconnected block that have been accepted back into the mempool.
     */
    void UpdateTransactionsFromBlock(const std::vector<Txid>& vHashesToUpdate) EXCLUSIVE_LOCKS_REQUIRED(cs, cs_main);

    /**
     * Calculate all in-mempool ancestors of entry (not including the tx itself)
     *
     * @param[in]   entry               CTxMemPoolEntry of which all in-mempool ancestors are calculated
     *
     * @return all in-mempool ancestors
     */
    setEntries CalculateMemPoolAncestors(const CTxMemPoolEntry& entry) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    bool HasDescendants(const Txid& txid) const;

    /** Collect the entire cluster of connected transactions for each transaction in txids.
     * All txids must correspond to transaction entries in the mempool, otherwise this returns an
     * empty vector. This call will also exit early and return an empty vector if it collects 500 or
     * more transactions as a DoS protection. */
    std::vector<txiter> GatherClusters(const std::vector<Txid>& txids) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    /** Calculate all in-mempool ancestors of a set of transactions not already in the mempool and
     * check ancestor and descendant limits. Heuristics are used to estimate the ancestor and
     * descendant count of all entries if the package were to be added to the mempool.  The limits
     * are applied to the union of all package transactions. For example, if the package has 3
     * transactions and limits.ancestor_count = 25, the union of all 3 sets of ancestors (including the
     * transactions themselves) must be <= 22.
     * @param[in]       package                 Transaction package being evaluated for acceptance
     *                                          to mempool. The transactions need not be direct
     *                                          ancestors/descendants of each other.
     * @param[in]       limits                  Maximum number and size of ancestors and descendants
     * @returns {} or the error reason if a limit is hit.
     */
    util::Result<void> CheckPackageLimits(const Package& package,
                                          const Limits& limits) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    /** Populate setDescendants with all in-mempool descendants of hash.
     *  Assumes that setDescendants includes all in-mempool descendants of anything
     *  already in it.  */
    void CalculateDescendants(txiter it, setEntries& setDescendants) const EXCLUSIVE_LOCKS_REQUIRED(cs);

    /** The minimum fee to get into the mempool, which may itself not be enough
     *  for larger-sized transactions.
     *  The m_incremental_relay_feerate policy variable is used to bound the time it
     *  takes the fee rate to go back down all the way to 0. When the feerate
     *  would otherwise be half of this, it is set to 0 instead.
     */
    CFeeRate GetMinFee() const {
        return GetMinFee(m_max_size_bytes);
    }

    /** Remove transactions from the mempool until its dynamic size is <= sizelimit.
      *  pvNoSpendsRemaining, if set, will be populated with the list of outpoints
      *  which are not in mempool which no longer have any spends in this mempool.
      */
    void TrimToSize(size_t sizelimit, std::vector<COutPoint>* pvNoSpendsRemaining = nullptr) EXCLUSIVE_LOCKS_REQUIRED(cs);

    /** Expire all transaction (and their dependencies) in the mempool older than time. Return the number of removed transactions. */
    int Expire(std::chrono::seconds time) EXCLUSIVE_LOCKS_REQUIRED(cs);

    /**
     * Calculate the ancestor and descendant count for the given transaction.
     * The counts include the transaction itself.
     * When ancestors is non-zero (ie, the transaction itself is in the mempool),
     * ancestorsize and ancestorfees will also be set to the appropriate values.
     */
    void GetTransactionAncestry(const Txid& txid, size_t& ancestors, size_t& descendants, size_t* ancestorsize = nullptr, CAmount* ancestorfees = nullptr) const;
    /**
     * @returns true if we've made an attempt to load the mempool regardless of
     *          whether the attempt was successful or not
     */
    bool GetLoadTried() const;

    /**
     * Set whether or not we've made an attempt to load the mempool (regardless
     * of whether the attempt was successful or not)
     */
    void SetLoadTried(bool load_tried);

    unsigned long size() const
    {
        LOCK(cs);
        return mapTx.size();
    }

    uint64_t GetTotalTxSize() const EXCLUSIVE_LOCKS_REQUIRED(cs)
    {
        AssertLockHeld(cs);
        return totalTxSize;
    }

    CAmount GetTotalFee() const EXCLUSIVE_LOCKS_REQUIRED(cs)
    {
        AssertLockHeld(cs);
        return m_total_fee;
    }

    bool exists(const Txid& txid) const
    {
        LOCK(cs);
        return mapTx.count(txid) != 0;
    }

    bool CompareDepthAndScore(const Txid& hasha, const Txid& hashb);

    CTransactionRef get(const Txid& txid) const;
    TxMempoolInfo info(const Txid& txid) const;
    std::vector<TxMempoolInfo> infoAll() const;

    bool existsProviderTxConflict(const CTransaction &tx) const;

    /**
     * Does another in-flight transaction already claim this transaction's operator key under the
     * *other* BLS encoding?
     *
     * mapProTxBlsPubKeyHashes is keyed by CBLSLazyPublicKey::GetHash(), which is scheme-sensitive,
     * so existsProviderTxConflict() above compares operator keys per encoding rather than per key
     * and cannot see this.
     *
     * This is mempool policy and is intentionally invoked unconditionally (not gated on the
     * deployment that makes cross-scheme reuse a consensus error). Gating it would leave a pair
     * admitted before activation resident afterwards: block assembly does not revalidate special
     * transactions cumulatively, so both would still be selected and the resulting block rejected,
     * stalling an honest miner. Policy may be stricter than consensus here, since a node that rejects
     * the second transaction still accepts a block containing it.
     */
    bool existsProviderTxCrossSchemeConflict(const CTransaction& tx) const;

    size_t DynamicMemoryUsage() const;

    /** Adds a transaction to the unbroadcast set */
    void AddUnbroadcastTx(const Txid& txid)
    {
        LOCK(cs);
        // Sanity check the transaction is in the mempool & insert into
        // unbroadcast set.
        if (exists(txid)) m_unbroadcast_txids.insert(txid);
    }

    /** Removes a transaction from the unbroadcast set */
    void RemoveUnbroadcastTx(const Txid& txid, bool unchecked = false);

    /** Returns transactions in unbroadcast set */
    std::set<Txid> GetUnbroadcastTxs() const
    {
        LOCK(cs);
        return m_unbroadcast_txids;
    }

    /** Returns whether a txid is in the unbroadcast set */
    bool IsUnbroadcastTx(const Txid& txid) const EXCLUSIVE_LOCKS_REQUIRED(cs)
    {
        AssertLockHeld(cs);
        return m_unbroadcast_txids.contains(txid);
    }

    /** Guards this internal counter for external reporting */
    uint64_t GetAndIncrementSequence() const EXCLUSIVE_LOCKS_REQUIRED(cs) {
        return m_sequence_number++;
    }

    uint64_t GetSequence() const EXCLUSIVE_LOCKS_REQUIRED(cs) {
        return m_sequence_number;
    }

public:
    /*
     * CTxMemPool::ChangeSet:
     *
     * This class is used for all mempool additions and associated removals (eg
     * due to rbf). Removals that don't need to be evaluated for acceptance,
     * such as removing transactions that appear in a block, or due to reorg,
     * or removals related to mempool limiting or expiry do not need to use
     * this.
     *
     * Callers can interleave calls to StageAddition()/StageRemoval(), and
     * removals may be invoked in any order, but additions must be done in a
     * topological order in the case of transaction packages (ie, parents must
     * be added before children).
     *
     * CalculateChunksForRBF() can be used to calculate the feerate diagram of
     * the proposed set of new transactions and compare with the existing
     * mempool.
     *
     * CalculateMemPoolAncestors() calculates the in-mempool (not including
     * what is in the change set itself) ancestors of a given transaction.
     *
     * Apply() will apply the removals and additions that are staged into the
     * mempool.
     *
     * Only one changeset may exist at a time. While a changeset is
     * outstanding, no removals or additions may be made directly to the
     * mempool.
     */
    class ChangeSet {
    public:
        explicit ChangeSet(CTxMemPool* pool) : m_pool(pool) { m_pool->m_txgraph->StartStaging(); }

        ~ChangeSet() EXCLUSIVE_LOCKS_REQUIRED(m_pool->cs)
        {
            AssertLockHeld(m_pool->cs);
            if (m_pool->m_txgraph->HaveStaging()) {
                m_pool->m_txgraph->AbortStaging();
            }
            m_pool->m_have_changeset = false;
        }

        ChangeSet(const ChangeSet&) = delete;
        ChangeSet& operator=(const ChangeSet&) = delete;

        using TxHandle = CTxMemPool::txiter;

        TxHandle StageAddition(const CTransactionRef& tx, CAmount fee, int64_t time, unsigned int entry_height, uint64_t entry_sequence, bool spends_coinbase, int64_t sigops_cost, LockPoints lp);

        void StageRemoval(CTxMemPool::txiter it);

        const CTxMemPool::setEntries& GetRemovals() const { return m_to_remove; }

        /** Check if any cluster limits are exceeded. Returns true if pass, false if fail. */
        bool CheckMemPoolPolicyLimits();

        CTxMemPool::setEntries CalculateMemPoolAncestors(TxHandle tx)
        {
            // Look up transaction in our cache first
            auto it = m_ancestors.find(tx);
            if (it != m_ancestors.end()) return it->second;

            // If not found, try to have the mempool calculate it, and cache
            // for later.
            LOCK(m_pool->cs);
            auto ret = m_pool->CalculateMemPoolAncestors(*tx);
            m_ancestors.try_emplace(tx, ret);
            return ret;
        }

        std::vector<CTransactionRef> GetAddedTxns() const
        {
            std::vector<CTransactionRef> ret;
            ret.reserve(m_entry_vec.size());
            for (const auto& entry : m_entry_vec) {
                ret.emplace_back(entry->GetSharedTx());
            }
            return ret;
        }

        /**
         * Calculate the sorted chunks for the old and new mempool relating to the
         * clusters that would be affected by a potential replacement transaction.
         *
         * @return old and new diagram pair respectively, or an error string if the conflicts don't match a calculable topology
         */
        util::Result<std::pair<std::vector<FeeFrac>, std::vector<FeeFrac>>> CalculateChunksForRBF();

        size_t GetTxCount() const { return m_entry_vec.size(); }

        const CTransaction& GetAddedTxn(size_t index) const { return m_entry_vec.at(index)->GetTx(); }

        void Apply() EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    private:
        void ProcessDependencies();

        CTxMemPool* m_pool;
        CTxMemPool::indexed_transaction_set m_to_add;
        std::vector<CTxMemPool::txiter> m_entry_vec;
        std::map<CTxMemPool::txiter, CTxMemPool::setEntries, CompareIteratorByHash> m_ancestors;
        CTxMemPool::setEntries m_to_remove;
        bool m_dependencies_processed{false};

        friend class CTxMemPool;
    };

    std::unique_ptr<ChangeSet> GetChangeSet() EXCLUSIVE_LOCKS_REQUIRED(cs)
    {
        Assume(!m_have_changeset);
        m_have_changeset = true;
        return std::make_unique<ChangeSet>(this);
    }

    bool m_have_changeset GUARDED_BY(cs){false};

    friend class CTxMemPool::ChangeSet;

private:
    // Apply the given changeset to the mempool, by removing transactions in
    // the to_remove set and adding transactions in the to_add set.
    void Apply(CTxMemPool::ChangeSet* changeset) EXCLUSIVE_LOCKS_REQUIRED(cs);

    // addNewTransaction must update state for all ancestors of a given transaction.
    // Note that addNewTransaction is ONLY called (via Apply()) from ATMP outside
    // of tests.
    void addNewTransaction(CTxMemPool::txiter it) EXCLUSIVE_LOCKS_REQUIRED(cs);

    /**
     * addNewTransaction extension for SCH-approved ProTx transactions.
     */
    void addUncheckedProTx(indexed_transaction_set::iterator& newit, const CTransaction& tx) EXCLUSIVE_LOCKS_REQUIRED(cs);

    void removeUnchecked(txiter entry, MemPoolRemovalReason reason) EXCLUSIVE_LOCKS_REQUIRED(cs);
    void removeUncheckedProTx(const CTransaction& tx) EXCLUSIVE_LOCKS_REQUIRED(cs);

public:
    void StartBlockBuilding() const EXCLUSIVE_LOCKS_REQUIRED(cs)
    {
        assert(!m_builder);
        m_builder = m_txgraph->GetBlockBuilder();
    }

    FeePerWeight GetBlockBuilderChunk(std::vector<CTxMemPoolEntry::CTxMemPoolEntryRef>& entries) const EXCLUSIVE_LOCKS_REQUIRED(cs)
    {
        if (!m_builder) return {};

        auto res = m_builder->GetCurrentChunk();
        if (!res) return {};

        auto [chunk_entries, chunk_feerate] = *res;
        for (TxGraph::Ref* ref : chunk_entries) {
            entries.emplace_back(static_cast<const CTxMemPoolEntry&>(*ref));
        }
        return chunk_feerate;
    }

    void IncludeBuilderChunk() const EXCLUSIVE_LOCKS_REQUIRED(cs) { m_builder->Include(); }
    void SkipBuilderChunk() const EXCLUSIVE_LOCKS_REQUIRED(cs) { m_builder->Skip(); }
    void StopBlockBuilding() const EXCLUSIVE_LOCKS_REQUIRED(cs) { m_builder.reset(); }
};

/**
 * CCoinsView that brings transactions from a mempool into view.
 * It does not check for spendings by memory pool transactions.
 * Instead, it provides access to all Coins which are either unspent in the
 * base CCoinsView, are outputs from any mempool transaction, or are
 * tracked temporarily to allow transaction dependencies in package validation.
 * This allows transaction replacement to work as expected, as you want to
 * have all inputs "available" to check signatures, and any cycles in the
 * dependency graph are checked directly in AcceptToMemoryPool.
 * It also allows you to sign a double-spend directly in
 * signrawtransactionwithkey and signrawtransactionwithwallet,
 * as long as the conflicting transaction is not yet confirmed.
 */
class CCoinsViewMemPool : public CCoinsViewBacked
{
    /**
    * Coins made available by transactions being validated. Tracking these allows for package
    * validation, since we can access transaction outputs without submitting them to mempool.
    */
    std::unordered_map<COutPoint, Coin, SaltedOutpointHasher> m_temp_added;
protected:
    const CTxMemPool& mempool;

public:
    CCoinsViewMemPool(CCoinsView* baseIn, const CTxMemPool& mempoolIn);
    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    /** Add the coins created by this transaction. These coins are only temporarily stored in
     * m_temp_added and cannot be flushed to the back end. Only used for package validation. */
    void PackageAddTransaction(const CTransactionRef& tx);
};
#endif // SCHILLINGCOIN_TXMEMPOOL_H