// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2020 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_NODE_MINER_H
#define SCHILLINGCOIN_NODE_MINER_H

#include <policy/policy.h>
#include <primitives/block.h>
#include <txmempool.h>

#include <memory>
#include <optional>
#include <stdint.h>

#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index_container.hpp>

class ArgsManager;
class CBlockIndex;
class CChainParams;
class CChainstateHelper;
class CConnman;
class CEvoDB;
class CReserveKey;
class CScript;
class CWallet;
class Chainstate;
struct LLMQContext;

namespace chainlock
{
class Chainlocks;
class ChainlockHandler;
} // namespace chainlock

namespace Consensus {
struct Params;
}

namespace llmq {
class CQuorumBlockProcessor;
} // namespace llmq

namespace node {

struct NodeContext;

static const bool DEFAULT_PRINTPRIORITY = false;

struct CBlockTemplate
{
    CBlock block;
    std::vector<CAmount> vTxFees;
    std::vector<int64_t> vTxSigOps;
    uint32_t nPrevBits;
    std::vector<CTxOut> voutMasternodePayments;
};

// Container for tracking updates to ancestor feerate as we include parent
// transactions in a block.
struct CTxMemPoolModifiedEntry
{
    explicit CTxMemPoolModifiedEntry(CTxMemPool::txiter entry)
    {
        iter = entry;
        nSizeWithAncestors = entry->GetSizeWithAncestors();
        nModFeesWithAncestors = entry->GetModFeesWithAncestors();
        nSigOpCountWithAncestors = entry->GetSigOpCountWithAncestors();
    }

    CAmount GetModifiedFee() const { return iter->GetModifiedFee(); }
    uint64_t GetSizeWithAncestors() const { return nSizeWithAncestors; }
    CAmount GetModFeesWithAncestors() const { return nModFeesWithAncestors; }
    size_t GetTxSize() const { return iter->GetTxSize(); }
    const CTransaction& GetTx() const { return iter->GetTx(); }

    CTxMemPool::txiter iter;
    uint64_t nSizeWithAncestors;
    CAmount nModFeesWithAncestors;
    unsigned int nSigOpCountWithAncestors;
};

struct CompareCTxMemPoolIter
{
    bool operator()(const CTxMemPool::txiter& a, const CTxMemPool::txiter& b) const
    {
        return &(*a) < &(*b);
    }
};

struct modifiedentry_iter
{
    using result_type = CTxMemPool::txiter;

    result_type operator()(const CTxMemPoolModifiedEntry& entry) const
    {
        return entry.iter;
    }
};

struct CompareTxIterByAncestorCount
{
    bool operator()(const CTxMemPool::txiter& a, const CTxMemPool::txiter& b) const
    {
        if (a->GetCountWithAncestors() != b->GetCountWithAncestors()) {
            return a->GetCountWithAncestors() < b->GetCountWithAncestors();
        }
        return CompareIteratorByHash()(a, b);
    }
};

using indexed_modified_transaction_set = boost::multi_index_container<
    CTxMemPoolModifiedEntry,
    boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<
            modifiedentry_iter,
            CompareCTxMemPoolIter
        >,
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<ancestor_score>,
            boost::multi_index::identity<CTxMemPoolModifiedEntry>,
            CompareTxMemPoolEntryByAncestorFee
        >
    >
>;

using modtxiter = indexed_modified_transaction_set::nth_index<0>::type::iterator;
using modtxscoreiter = indexed_modified_transaction_set::index<ancestor_score>::type::iterator;

struct update_for_parent_inclusion
{
    explicit update_for_parent_inclusion(CTxMemPool::txiter it) : iter(it) {}

    void operator()(CTxMemPoolModifiedEntry& entry)
    {
        entry.nModFeesWithAncestors -= iter->GetModifiedFee();
        entry.nSizeWithAncestors -= iter->GetTxSize();
        entry.nSigOpCountWithAncestors -= iter->GetSigOpCount();
    }

    CTxMemPool::txiter iter;
};

/** Generate a new block, without valid proof-of-work or proof-of-stake. */
class BlockAssembler
{
private:
    std::unique_ptr<CBlockTemplate> pblocktemplate;

    uint64_t nBlockSize;
    uint64_t nBlockTx;
    unsigned int nBlockSigOps;
    CAmount nFees;
    CTxMemPool::setEntries inBlock;

    int nHeight;
    int64_t m_lock_time_cutoff;

    CChainstateHelper& m_chain_helper;
    Chainstate& m_chainstate;
    CEvoDB& m_evoDb;
    const chainlock::Chainlocks& m_chainlocks;
    chainlock::ChainlockHandler& m_clhandler;
    const CChainParams& chainparams;
    const CTxMemPool* const m_mempool;
    const llmq::CQuorumBlockProcessor& m_quorum_block_processor;

public:
    struct Options {
        mutable size_t nBlockMaxSize{DEFAULT_BLOCK_MAX_SIZE};
        mutable size_t nBlockMaxSigOps{0};
        CFeeRate blockMinFeeRate{DEFAULT_BLOCK_MIN_TX_FEE};
        bool test_block_validity{true};
    };

    explicit BlockAssembler(
        Chainstate& chainstate,
        const NodeContext& node,
        const CTxMemPool* mempool);

    explicit BlockAssembler(
        Chainstate& chainstate,
        const NodeContext& node,
        const CTxMemPool* mempool,
        const Options& options);

    /**
     * Construct a new block template.
     *
     * When fProofOfStake is true, pwallet is used to construct and sign
     * SCH's historical coinstake transaction. The default arguments preserve
     * the modern donor call form used by non-staking block-template callers.
     */
    std::unique_ptr<CBlockTemplate> CreateNewBlock(
        const CScript& scriptPubKeyIn,
        CWallet* pwallet = nullptr,
        bool fProofOfStake = false);

    inline static std::optional<int64_t> m_last_block_num_txs{};
    inline static std::optional<int64_t> m_last_block_size{};

private:
    const Options m_options;

    void resetBlock();
    void AddToBlock(CTxMemPool::txiter iter);

    void addPackageTxs(
        const CTxMemPool& mempool,
        int& nPackagesSelected,
        int& nDescendantsUpdated,
        const CBlockIndex* pindexPrev)
        EXCLUSIVE_LOCKS_REQUIRED(::cs_main, mempool.cs);

    void onlyUnconfirmed(CTxMemPool::setEntries& testSet);
    bool TestPackage(uint64_t packageSize, unsigned int packageSigOps) const;

    bool TestPackageTransactions(
        const CTxMemPool::setEntries& package) const
        EXCLUSIVE_LOCKS_REQUIRED(::cs_main);

    void SortForBlock(
        const CTxMemPool::setEntries& package,
        std::vector<CTxMemPool::txiter>& sortedEntries);
};

int64_t UpdateTime(
    CBlockHeader* pblock,
    const Consensus::Params& consensusParams,
    const CBlockIndex* pindexPrev);

void ApplyArgsManOptions(
    const ArgsManager& args,
    BlockAssembler::Options& options);

} // namespace node

/** Modify the extranonce and update the Merkle root. */
void IncrementExtraNonce(
    CBlock* pblock,
    CBlockIndex* pindexPrev,
    unsigned int& nExtraNonce);

#ifdef ENABLE_WALLET
void GenerateBitcoins(
    node::NodeContext& node,
    bool fGenerate,
    CWallet* pwallet,
    int nThreads);

void BitcoinMiner(
    node::NodeContext& node,
    CWallet* pwallet,
    bool fProofOfStake);

void ThreadStakeMinter(node::NodeContext& node);
#endif // ENABLE_WALLET

extern double dHashesPerSec;
extern int64_t nHPSTimerStart;

#endif // SCHILLINGCOIN_NODE_MINER_H