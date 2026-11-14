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
#include <util/feefrac.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

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
    /** Package fee rates in the order packages were selected for the block. */
    std::vector<FeePerVSize> m_package_feerates;
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

    /** Add transactions using the TxGraph block-builder chunk ordering. */
    void addChunks() EXCLUSIVE_LOCKS_REQUIRED(m_mempool->cs);

    /** Test whether a TxGraph chunk fits within the remaining block limits. */
    bool TestChunkBlockLimits(int64_t chunk_weight, int64_t chunk_sigops_cost) const;

    /** Perform finality and lock-time checks on every transaction in a chunk. */
    bool TestChunkTransactions(
        const std::vector<CTxMemPoolEntry::CTxMemPoolEntryRef>& transactions) const;
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