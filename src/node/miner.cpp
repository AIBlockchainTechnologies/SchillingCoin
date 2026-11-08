// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2011-2013 The PPCoin developers
// Copyright (c) 2013-2014 The NovaCoin Developers
// Copyright (c) 2014-2018 The BlackCoin Developers
// Copyright (c) 2015-2020 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/miner.h>

#include <arith_uint256.h>
#include <blocksignature.h>
#include <chain.h>
#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/merkle.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <deploymentstatus.h>
#include <hash.h>
#include <masternode-payments.h>
#include <net.h>
#include <node/context.h>
#include <policy/feerate.h>
#include <policy/policy.h>
#include <pow.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <timedata.h>
#include <util.h>
#include <util/moneystr.h>
#include <util/system.h>
#include <validation.h>
#include <validationinterface.h>

#include <chainlock/chainlock.h>
#include <chainlock/handler.h>
#include <evo/cbtx.h>
#include <evo/chainhelper.h>
#include <evo/deterministicmns.h>
#include <evo/mnhftx.h>
#include <evo/simplifiedmns.h>
#include <evo/specialtx.h>
#include <evo/specialtxman.h>
#include <llmq/blockprocessor.h>
#include <llmq/context.h>
#include <llmq/options.h>
#include <llmq/snapshot.h>

#ifdef ENABLE_WALLET
#include <wallet/db.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>
#endif

#include <algorithm>
#include <cassert>
#include <chrono>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include <boost/thread.hpp>

// Global wallet pointer from init.cpp
extern CWallet* pwalletMain;

namespace node {

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    const int64_t nOldTime{pblock->nTime};
    const int64_t nNewTime{std::max<int64_t>(
        pindexPrev->GetMedianTimePast() + 1,
        TicksSinceEpoch<std::chrono::seconds>(NodeClock::now()))};

    if (nOldTime < nNewTime) {
        pblock->nTime = nNewTime;
    }

    if (consensusParams.fPowAllowMinDifficultyBlocks) {
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);
    }

    return nNewTime - nOldTime;
}

static BlockAssembler::Options ClampOptions(BlockAssembler::Options options)
{
    options.nBlockMaxSize =
        std::clamp<size_t>(options.nBlockMaxSize, 1000, DEFAULT_BLOCK_MAX_SIZE);
    return options;
}

BlockAssembler::BlockAssembler(
    Chainstate& chainstate,
    const NodeContext& node,
    const CTxMemPool* mempool,
    const Options& options) :
    m_chain_helper(chainstate.ChainHelper()),
    m_chainstate{chainstate},
    m_evoDb(*Assert(node.evodb)),
    m_chainlocks(*Assert(node.chainlocks)),
    m_clhandler(*Assert(node.clhandler)),
    chainparams(chainstate.m_chainman.GetParams()),
    m_mempool{mempool},
    m_quorum_block_processor(*Assert(Assert(node.llmq_ctx)->quorum_block_processor)),
    m_options{ClampOptions(options)}
{
}

void ApplyArgsManOptions(const ArgsManager& args, BlockAssembler::Options& options)
{
    options.nBlockMaxSize = args.GetIntArg("-blockmaxsize", options.nBlockMaxSize);

    if (const auto blockmintxfee{args.GetArg("-blockmintxfee")}) {
        if (const auto parsed{ParseMoney(*blockmintxfee)}) {
            options.blockMinFeeRate = CFeeRate{*parsed};
        }
    }
}

static BlockAssembler::Options ConfiguredOptions()
{
    BlockAssembler::Options options;
    ApplyArgsManOptions(gArgs, options);
    return options;
}

BlockAssembler::BlockAssembler(
    Chainstate& chainstate,
    const NodeContext& node,
    const CTxMemPool* mempool) :
    BlockAssembler(chainstate, node, mempool, ConfiguredOptions())
{
}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    // Reserve space for coinbase/coinstake and block overhead.
    nBlockSize = 1000;
    nBlockSigOps = 100;

    nBlockTx = 0;
    nFees = 0;
}

static bool SolveProofOfStake(
    Chainstate& chainstate,
    CBlock* pblock,
    const CBlockIndex* pindexPrev,
    CWallet* pwallet,
    const Consensus::Params& consensusParams)
{
    assert(pblock);
    assert(pindexPrev);
    assert(pwallet);

    boost::this_thread::interruption_point();

    pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);

    std::vector<CStakeableOutput> availableCoins;
    if (!pwallet->StakeableCoins(&availableCoins)) {
        LogPrint(
            BCLog::STAKING,
            "%s : no stakeable coins available\n",
            __func__);
        return false;
    }

    CMutableTransaction txCoinStake;
    int64_t nTxNewTime = 0;

    if (!pwallet->CreateCoinStake(
            chainstate,
            pindexPrev,
            pblock->nBits,
            txCoinStake,
            nTxNewTime,
            &availableCoins,
            true)) {
        LogPrint(
            BCLog::STAKING,
            "%s : stake not found\n",
            __func__);
        return false;
    }

    if (!pwallet->SignCoinStake(txCoinStake)) {
        LogPrintf(
            "%s : failed to sign coinstake\n",
            __func__);
        return false;
    }

    pblock->nTime = nTxNewTime;
    pblock->vtx[0].vout[0].SetEmpty();
    pblock->vtx.push_back(CTransaction(txCoinStake));

    return true;
}

static bool CalcCbTxBestChainlock(
    const chainlock::Chainlocks& chainlocks,
    const CBlockIndex* pindexPrev,
    uint32_t& bestCLHeightDiff,
    CBLSSignature& bestCLSignature)
{
    auto best_clsig = chainlocks.GetBestChainLock();

    if (best_clsig.getHeight() <
        Params().GetConsensus().DeploymentHeight(Consensus::DEPLOYMENT_V19)) {
        best_clsig = chainlock::ChainLockSig{};
    }

    if (best_clsig.getHeight() == pindexPrev->nHeight) {
        bestCLHeightDiff = 0;
        bestCLSignature = best_clsig.getSig();
        return true;
    }

    auto prevBlockCoinbaseChainlock = GetNonNullCoinbaseChainlock(pindexPrev);

    if (prevBlockCoinbaseChainlock.has_value()) {
        if (best_clsig.IsNull()) {
            bestCLHeightDiff = prevBlockCoinbaseChainlock->second + 1;
            bestCLSignature = prevBlockCoinbaseChainlock->first;
            return true;
        }

        const int curCLHeight = best_clsig.getHeight();
        const int prevCLHeight =
            pindexPrev->nHeight -
            static_cast<int>(prevBlockCoinbaseChainlock->second) - 1;

        if (curCLHeight < prevCLHeight) {
            bestCLHeightDiff = prevBlockCoinbaseChainlock->second + 1;
            bestCLSignature = prevBlockCoinbaseChainlock->first;
        } else {
            bestCLHeightDiff =
                pindexPrev->nHeight - best_clsig.getHeight();
            bestCLSignature = best_clsig.getSig();
        }

        return true;
    }

    if (best_clsig.IsNull()) {
        bestCLHeightDiff = 0;
        bestCLSignature.Reset();
        return false;
    }

    bestCLHeightDiff =
        pindexPrev->nHeight - best_clsig.getHeight();
    bestCLSignature = best_clsig.getSig();
    return true;
}

CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn, CWallet* pwallet, bool fProofOfStake)
{
    // Create new block
    std::unique_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if (!pblocktemplate.get()) return nullptr;
    CBlock* pblock = &pblocktemplate->block; // pointer for convenience

    // Tip
    CBlockIndex* pindexPrev = GetChainTip();
    if (!pindexPrev) return nullptr;
    const int nHeight = pindexPrev->nHeight + 1;

    // Make sure to create the correct block version
    const Consensus::Params& consensus = Params().GetConsensus();
    bool isAfterRHF = consensus.IsPastRHFBlock(nHeight);
    if (nHeight < consensus.height_start_ZC)
        pblock->nVersion = 3; // Pre-Zerocoin activation
    else if (!isAfterRHF)
        pblock->nVersion = 4; // Post-Zerocoin activation
    else
        pblock->nVersion = 5; // The reverse hardfork, complete zerocoin deactivation and more.
                              // (Activations & Removals documented in primitives/block.h)

    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (Params().IsRegTestNet()) {
        if (nHeight < consensus.height_start_ZC) pblock->nVersion = 3;
        pblock->nVersion = gArgs.GetArg("-blockversion", pblock->nVersion);
    }

    // Create coinbase tx
    CMutableTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vout.resize(1);
    txNew.vout[0].scriptPubKey = scriptPubKeyIn;
    pblock->vtx.push_back(txNew);
    pblocktemplate->vTxFees.push_back(-1);   // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    if (fProofOfStake && !SolveProofOfStake(m_chainstate, pblock, pindexPrev, pwallet, consensus)) {
        return nullptr;
    }

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = gArgs.GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:

    unsigned int nBlockMaxSizeNetwork = MAX_BLOCK_SIZE_CURRENT;
    nBlockMaxSize = std::max((unsigned int)1000, std::min((nBlockMaxSizeNetwork - 1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = gArgs.GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = gArgs.GetArg("-blockminsize", DEFAULT_BLOCK_MIN_SIZE);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Collect memory pool transactions into the block
    CAmount nFees = 0;

    {
        LOCK2(cs_main, mempool.cs);
        CCoinsViewCache view(pcoinsTip);

        // Priority order to process transactions
        std::list<COrphan> vOrphan; // list memory doesn't move
        std::map<uint256, std::vector<COrphan*> > mapDependers;
        bool fPrintPriority = gArgs.GetBoolArg("-printpriority", false);

        // This vector will be sorted into a priority queue:
        std::vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (std::map<uint256, CTxMemPoolEntry>::iterator mi = mempool.mapTx.begin();
             mi != mempool.mapTx.end(); ++mi) {
            const CTransaction& tx = mi->second.GetTx();
            if (tx.IsCoinBase() || tx.IsCoinStake() || !IsFinalTx(tx, nHeight)){
                continue;
            }

            COrphan* porphan = NULL;
            double dPriority = 0;
            CAmount nTotalIn = 0;
            bool fMissingInputs = false;

            for (const CTxIn& txin : tx.vin) {
                // Read the exact previous output.
                if (!view.HaveCoin(txin.prevout)) {
                    // This should never happen; all transactions in the memory
                    // pool should connect to either transactions in the chain
                    // or other transactions in the memory pool.
                    if (!mempool.mapTx.count(txin.prevout.hash)) {
                        LogPrintf("ERROR: mempool transaction missing input\n");
                        if (fDebug) assert("mempool transaction missing input" == 0);
                        fMissingInputs = true;
                        if (porphan)
                            vOrphan.pop_back();
                        break;
                    }

                    // Has to wait for dependencies
                    if (!porphan) {
                        // Use list for automatic deletion
                        vOrphan.push_back(COrphan(&tx));
                        porphan = &vOrphan.back();
                    }
                    mapDependers[txin.prevout.hash].push_back(porphan);
                    porphan->setDependsOn.insert(txin.prevout.hash);
                    nTotalIn += mempool.mapTx[txin.prevout.hash].GetTx().vout[txin.prevout.n].nValue;
                    continue;
                }

                const Coin& coin = view.AccessCoin(txin.prevout);
                assert(!coin.IsSpent());

                const CAmount nValueIn = coin.out.nValue;
                nTotalIn += nValueIn;

                const int nConf = nHeight - coin.nHeight;

                // zSCH spends can have (had) very large priority, use non-overflowing safe functions
                dPriority = double_safe_addition(dPriority, ((double)nValueIn * nConf));
            }
            if (fMissingInputs) continue;

            // Priority is sum(valuein * age) / modified_txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority = tx.ComputePriority(dPriority, nTxSize);

            uint256 hash = tx.GetHash();
            mempool.ApplyDeltas(hash, dPriority, nTotalIn);

            CFeeRate feeRate(nTotalIn - tx.GetValueOut(), nTxSize);

            if (porphan) {
                porphan->dPriority = dPriority;
                porphan->feeRate = feeRate;
            } else
                vecPriority.push_back(TxPriority(dPriority, feeRate, &mi->second.GetTx()));
        }

        // Collect transactions into block
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int nBlockSigOps = 100;
        bool fSortedByFee = (nBlockPrioritySize <= 0);

        TxPriorityCompare comparer(fSortedByFee);
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);

        std::vector<CBigNum> vBlockSerials;
        std::vector<CBigNum> vTxSerials;
        while (!vecPriority.empty()) {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            CFeeRate feeRate = vecPriority.front().get<1>();
            const CTransaction& tx = *(vecPriority.front().get<2>());

            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Legacy limits on sigOps:
            unsigned int nMaxBlockSigOps = MAX_BLOCK_SIGOPS_CURRENT;
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= nMaxBlockSigOps)
                continue;

            // Skip free transactions if we're past the minimum block size:
            const uint256& hash = tx.GetHash();
            double dPriorityDelta = 0;
            CAmount nFeeDelta = 0;
            mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
            if (fSortedByFee && (dPriorityDelta <= 0) && (nFeeDelta <= 0) && (feeRate < ::minRelayTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritise by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize >= nBlockPrioritySize) || !AllowFree(dPriority))) {
                fSortedByFee = true;
                comparer = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }

            if (!view.HaveInputs(tx))
                continue;

            CAmount nTxFees = view.GetValueIn(tx) - tx.GetValueOut();

            nTxSigOps += GetP2SHSigOpCount(tx, view);
            if (nBlockSigOps + nTxSigOps >= nMaxBlockSigOps)
                continue;

            // Note that flags: we don't want to set mempool/IsStandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.

            CValidationState state;
            PrecomputedTransactionData precomTxData(tx);

            if (!CheckInputs(
                    tx,
                    state,
                    view,
                    true,
                    MANDATORY_SCRIPT_VERIFY_FLAGS,
                    true,
                    precomTxData,
                    nullptr))
                continue;

            CTxUndo txundo;
            UpdateCoins(tx, state, view, txundo, nHeight);

            // Added
            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOps.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;
            nFees += nTxFees;

            for (const CBigNum& bnSerial : vTxSerials)
                vBlockSerials.emplace_back(bnSerial);

            if (fPrintPriority) {
                LogPrintf("priority %.1f fee %s txid %s\n",
                    dPriority,
                    feeRate.ToString(),
                    tx.GetHash().ToString());
            }

            // Add transactions that depend on this one to the priority queue
            if (mapDependers.count(hash)) {
                for (COrphan* porphan : mapDependers[hash]) {
                    if (!porphan->setDependsOn.empty()) {
                        porphan->setDependsOn.erase(hash);

                        if (porphan->setDependsOn.empty()) {
                            vecPriority.push_back(
                                TxPriority(
                                    porphan->dPriority,
                                    porphan->feeRate,
                                    porphan->ptx));

                            std::push_heap(
                                vecPriority.begin(),
                                vecPriority.end(),
                                comparer);
                        }
                    }
                }
            }
        }

        if (fProofOfStake) {
            CMutableTransaction txCoinStake(pblock->vtx[1]);

            // Before activation, PoS transaction fees retain their historical
            // behavior and are not added to the coinstake reward.
            //
            // After activation, add all ordinary transaction fees to the
            // staking outputs first. FillBlockPayee then transfers the
            // masternode's required 80% fee share to its payment output,
            // leaving the remaining 20% with the staker.
            if (consensus.IsFeeRewardDistributionActive(nHeight) &&
                nFees > 0 &&
                txCoinStake.vout.size() > 1) {
                txCoinStake.vout.back().nValue += nFees;
            }

            // Apply the masternode subsidy payment and, after activation,
            // the masternode's 80% share of ordinary transaction fees.
            FillBlockPayee(txCoinStake, nFees, pindexPrev, true);

            if (!pwallet->SignCoinStake(txCoinStake)) {
                LogPrintf("%s: failed to sign finalized coinstake\n", __func__);
                return nullptr;
            }

            pblock->vtx[1] = CTransaction(txCoinStake);

            if (txCoinStake.vout.size() > 1) {
                pblock->payee = txCoinStake.vout.back().scriptPubKey;
            }
        } else {
            // Masternode payment
            FillBlockPayee(txNew, nFees, pindexPrev, false);

            // Make payee
            if (txNew.vout.size() > 1) {
                pblock->payee = txNew.vout[1].scriptPubKey;
            } else {
                const CAmount blockSubsidy = GetBlockSubsidyInner(
                    pindexPrev->nBits,
                    pindexPrev->nHeight,
                    consensus,
                    false);
                const CAmount blockValue = nFees + blockSubsidy;
                txNew.vout[0].nValue = blockValue;
                txNew.vin[0].scriptSig = CScript() << nHeight << OP_0;
            }
        }

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;
        LogPrintf("%s : total size %u\n", __func__, nBlockSize);

        // Compute final coinbase transaction.
        pblock->vtx[0].vin[0].scriptSig = CScript() << nHeight << OP_0;
        if (!fProofOfStake) {
            pblock->vtx[0] = txNew;
            pblocktemplate->vTxFees[0] = -nFees;
        }

        // Fill in header
        pblock->hashPrevBlock = pindexPrev->GetBlockHash();
        if (!fProofOfStake)
            UpdateTime(pblock, consensus, pindexPrev);
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensus);
        pblock->nNonce = 0;

        if (pblock->nVersion == 4) // Accumulators only exist on version 4 blocks
            pblock->nAccumulatorCheckpoint = pindexPrev->nAccumulatorCheckpoint;

        pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(pblock->vtx[0]);

        if (fProofOfStake) {
            unsigned int nExtraNonce = 0;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

            LogPrintf("CPUMiner : proof-of-stake block found %s \n", pblock->GetHash().GetHex());

            if (!SignBlock(*pblock, *pwallet)) {
                LogPrintf("%s: Signing new block with UTXO key failed \n", __func__);
                return nullptr;
            }
        }

        CValidationState state;
        if (!TestBlockValidity(state, *pblock, pindexPrev, false, false)) {
            LogPrintf("CreateNewBlock() : TestBlockValidity failed\n");
            mempool.clear();
            return nullptr;
        }
    }

    return pblocktemplate.release();
}

void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock) {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }

    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight + 1; // Height first in coinbase required for block.version=2

    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = txCoinbase;
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}

#ifdef ENABLE_WALLET

//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//
double dHashesPerSec = 0.0;
int64_t nHPSTimerStart = 0;

CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey, CWallet* pwallet)
{
    CPubKey pubkey;
    if (!reservekey.GetReservedKey(pubkey, false))
        return nullptr;

    const int nHeightNext = chainActive.Tip()->nHeight + 1;

    // If we're building a late PoW block, don't continue
    // PoS blocks are built directly with CreateNewBlock
    if ((nHeightNext > Params().GetConsensus().height_last_PoW)) {
        LogPrintf("%s: Aborting PoW block creation during PoS phase\n", __func__);
        // sleep 1/2 a block time so we don't go into a tight loop.
        MilliSleep((Params().GetConsensus().nTargetSpacing * 1000) >> 1);
        return nullptr;
    }

    CScript scriptPubKey = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
    return CreateNewBlock(scriptPubKey, pwallet, false);
}

bool ProcessBlockFound(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        WAIT_LOCK(g_best_block_mutex, lock);
        if (pblock->hashPrevBlock != g_best_block)
            return error("SchillingCoinMiner : generated block is stale");
    }

    // Remove key from key pool
    reservekey.KeepKey();

    // Track how many getdata requests this block gets
    {
        LOCK(wallet.cs_wallet);
        wallet.mapRequestCount[pblock->GetHash()] = 0;
    }

    // Inform about the new block
    GetMainSignals().BlockFound(pblock->GetHash());

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(state, pblock)) {
        return error("SchillingCoinMiner : ProcessNewBlock, block not accepted");
    }

    if (g_connman) {
        g_connman->ForEachNode([&pblock](CNode* node) {
            node->PushInventory(CInv(MSG_BLOCK, pblock->GetHash()));
        });
    }

    return true;
}

bool fGenerateBitcoins = false;
bool fStakeableCoins = false;
int nMintableLastCheck = 0;

void CheckForCoins(CWallet* pwallet, const int minutes)
{
    //control the amount of times the client will check for mintable coins
    int nTimeNow = GetTime();
    if ((nTimeNow - nMintableLastCheck > minutes * 60)) {
        nMintableLastCheck = nTimeNow;
        fStakeableCoins = pwallet->StakeableCoins();
    }
}

void BitcoinMiner(CWallet* pwallet, bool fProofOfStake)
{
    LogPrintf("SchillingCoinMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    util::ThreadRename("schillingcoin-miner");
    const Consensus::Params& consensus = Params().GetConsensus();
    const int64_t nSpacingMillis = consensus.nTargetSpacing * 1000;
    const int last_pow_block = consensus.height_last_PoW;

    // Each thread has its own key and counter
    CReserveKey reservekey(pwallet);
    unsigned int nExtraNonce = 0;

    while (fGenerateBitcoins || fProofOfStake) {
        CBlockIndex* pindexPrev = GetChainTip();
        if (!pindexPrev) {
            MilliSleep(nSpacingMillis);       // sleep a block
            continue;
        }
        if (fProofOfStake) {
            if (pindexPrev->nHeight < last_pow_block) {
                // The last PoW block hasn't even been mined yet.
                MilliSleep(nSpacingMillis);       // sleep a block
                continue;
            }

            // update fStakeableCoins (5 minute check time);
            CheckForCoins(pwallet, 5);

            while (((!g_connman || g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0) && Params().MiningRequiresPeers()) || pwallet->IsLocked() || !fStakeableCoins ||
				(masternodeSync.NotCompleted() && (1040000 <= pindexPrev->nHeight && pindexPrev->nHeight <= 1040120))) {
                MilliSleep(5000);
                // Do a separate 1 minute check here to ensure fStakeableCoins is updated
                if (!fStakeableCoins) CheckForCoins(pwallet, 1);
            }

            //search our map of hashed blocks, see if bestblock has been hashed yet
            if (pwallet->pStakerStatus &&
                    pwallet->pStakerStatus->GetLastHash() == pindexPrev->GetBlockHash() &&
                    pwallet->pStakerStatus->GetLastTime() >= GetCurrentTimeSlot()) {
                MilliSleep(2000);
                continue;
            }

        } else if ((pindexPrev->nHeight - 6) > last_pow_block) {
            // Late PoW: run for a little while longer, just in case there is a rewind on the chain.
            LogPrintf("%s: Exiting PoW Mining Thread at height: %d\n", __func__, pindexPrev->nHeight);
            return;
       }

        //
        // Create new block
        //
        unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();

        std::unique_ptr<CBlockTemplate> pblocktemplate((fProofOfStake ?
                                                        CreateNewBlock(CScript(), pwallet, fProofOfStake) :
                                                        CreateNewBlockWithKey(reservekey, pwallet)));
        if (!pblocktemplate.get()) continue;
        CBlock* pblock = &pblocktemplate->block;

        // POS - block found: process it
        if (fProofOfStake) {
            LogPrintf("%s : proof-of-stake block was signed %s \n", __func__, pblock->GetHash().ToString().c_str());
            SetThreadPriority(THREAD_PRIORITY_NORMAL);
            if (!ProcessBlockFound(pblock, *pwallet, reservekey)) {
                LogPrintf("%s: New block orphaned\n", __func__);
                continue;
            }
            SetThreadPriority(THREAD_PRIORITY_LOWEST);
            continue;
        }

        // POW - miner main
        IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

        LogPrintf("Running SchillingCoinMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
            ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));

        //
        // Search
        //
        int64_t nStart = GetTime();
        arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);
        while (true) {
            unsigned int nHashesDone = 0;

            arith_uint256 hash;
            while (true) {
                hash = UintToArith256(pblock->GetHash());
                if (hash <= hashTarget) {
                    // Found a solution
                    SetThreadPriority(THREAD_PRIORITY_NORMAL);
                    LogPrintf("%s:\n", __func__);
                    LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex(), hashTarget.GetHex());
                    ProcessBlockFound(pblock, *pwallet, reservekey);
                    SetThreadPriority(THREAD_PRIORITY_LOWEST);

                    // In regression test mode, stop mining after a block is found. This
                    // allows developers to controllably generate a block on demand.
                    if (Params().IsRegTestNet())
                        throw boost::thread_interrupted();

                    break;
                }
                pblock->nNonce += 1;
                nHashesDone += 1;
                if ((pblock->nNonce & 0xFF) == 0)
                    break;
            }

            // Meter hashes/sec
            static int64_t nHashCounter;
            if (nHPSTimerStart == 0) {
                nHPSTimerStart = GetTimeMillis();
                nHashCounter = 0;
            } else
                nHashCounter += nHashesDone;
            if (GetTimeMillis() - nHPSTimerStart > 4000) {
                static RecursiveMutex cs;
                {
                    LOCK(cs);
                    if (GetTimeMillis() - nHPSTimerStart > 4000) {
                        dHashesPerSec = 1000.0 * nHashCounter / (GetTimeMillis() - nHPSTimerStart);
                        nHPSTimerStart = GetTimeMillis();
                        nHashCounter = 0;
                        static int64_t nLogTime;
                        if (GetTime() - nLogTime > 30 * 60) {
                            nLogTime = GetTime();
                            LogPrintf("hashmeter %6.0f khash/s\n", dHashesPerSec / 1000.0);
                        }
                    }
                }
            }

            // Check for stop or if block needs to be rebuilt
            boost::this_thread::interruption_point();
            if (    ((!g_connman || g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0) && Params().MiningRequiresPeers()) || // Regtest mode doesn't require peers
                    (pblock->nNonce >= 0xffff0000) ||
                    (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60) ||
                    (pindexPrev != chainActive.Tip())
                ) break;

            // Update nTime every few seconds
            UpdateTime(pblock, consensus, pindexPrev);
            if (consensus.fPowAllowMinDifficultyBlocks) {
                // Changing pblock->nTime can change work required on testnet:
                hashTarget.SetCompact(pblock->nBits);
            }

        }
    }
}

void static ThreadBitcoinMiner(void* parg)
{
    boost::this_thread::interruption_point();
    CWallet* pwallet = (CWallet*)parg;
    try {
        BitcoinMiner(pwallet, false);
        boost::this_thread::interruption_point();
    } catch (const std::exception& e) {
        LogPrintf("SchillingCoinMiner exception");
    } catch (...) {
        LogPrintf("SchillingCoinMiner exception");
    }

    LogPrintf("SchillingCoinMiner exiting\n");
}

void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads)
{
    static boost::thread_group* minerThreads = NULL;
    fGenerateBitcoins = fGenerate;

    if (minerThreads != NULL) {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&ThreadBitcoinMiner, pwallet));
}

// SCH stake minter thread
void ThreadStakeMinter()
{
    boost::this_thread::interruption_point();
    LogPrintf("ThreadStakeMinter started\n");
    CWallet* pwallet = pwalletMain;
    try {
        BitcoinMiner(pwallet, true);
        boost::this_thread::interruption_point();
    } catch (const std::exception& e) {
        LogPrintf("ThreadStakeMinter() exception \n");
    } catch (...) {
        LogPrintf("ThreadStakeMinter() error \n");
    }
    LogPrintf("ThreadStakeMinter exiting,\n");
}

#endif // ENABLE_WALLET