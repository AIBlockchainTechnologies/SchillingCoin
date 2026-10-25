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

#include "miner.h"

#include "amount.h"
#include "blockassembler.h"
#include "blocksignature.h"
#include "consensus/merkle.h"
#include "consensus/tx_verify.h"
#include "hash.h"
#include "main.h"
#include "masternode-payments.h"
#include "masternode-sync.h"
#include "net.h"
#include "policy/feerate.h"
#include "pow.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "timedata.h"
#include "util.h"
#include "util/system.h"
#include "utilmoneystr.h"
#include "validationinterface.h"

// Wallet subsystem (always include)
#include "wallet/db.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"

// Global wallet pointer from init.cpp
extern CWallet* pwalletMain;

#include <boost/thread.hpp>

//////////////////////////////////////////////////////////////////////////////
//
// SchillingCoinMiner
//

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
    if (!ProcessNewBlock(state, NULL, pblock)) {
        return error("SchillingCoinMiner : ProcessNewBlock, block not accepted");
    }

    for (CNode* node : vNodes) {
        node->PushInventory(CInv(MSG_BLOCK, pblock->GetHash()));
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
    const int64_t nSpacingMillis = Params().GetConsensus().nTargetSpacing * 1000;
    const int last_pow_block = Params().GetConsensus().height_last_PoW;

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

            while ((vNodes.empty() && Params().MiningRequiresPeers()) || pwallet->IsLocked() || !fStakeableCoins ||
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
        uint256 hashTarget = uint256().SetCompact(pblock->nBits);
        while (true) {
            unsigned int nHashesDone = 0;

            uint256 hash;
            while (true) {
                hash = pblock->GetHash();
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
            if (    (vNodes.empty() && Params().MiningRequiresPeers()) || // Regtest mode doesn't require peers
                    (pblock->nNonce >= 0xffff0000) ||
                    (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60) ||
                    (pindexPrev != chainActive.Tip())
                ) break;

            // Update nTime every few seconds
            UpdateTime(pblock, pindexPrev);
            if (Params().GetConsensus().fPowAllowMinDifficultyBlocks) {
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