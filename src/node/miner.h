// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2020 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_NODE_MINER_H
#define SCHILLINGCOIN_NODE_MINER_H

#include "consensus/amount.h"
#include "primitives/block.h"

#include <cstdint>
#include <vector>

class CBlock;
class CBlockHeader;
class CBlockIndex;
class CReserveKey;
class CScript;
class CWallet;

struct CBlockTemplate {
    CBlock block;
    std::vector<CAmount> vTxFees;
    std::vector<int64_t> vTxSigOps;
};

/** Generate a new block template without valid proof of work. */
CBlockTemplate* CreateNewBlock(
    const CScript& scriptPubKeyIn,
    CWallet* pwallet,
    bool fProofOfStake);

/** Modify the extranonce and update the Merkle root. */
void IncrementExtraNonce(
    CBlock* pblock,
    CBlockIndex* pindexPrev,
    unsigned int& nExtraNonce);

/** Update block time and testnet difficulty when required. */
void UpdateTime(
    CBlockHeader* block,
    const CBlockIndex* pindexPrev);

#ifdef ENABLE_WALLET

/** Run the miner threads. */
void GenerateBitcoins(
    bool fGenerate,
    CWallet* pwallet,
    int nThreads);

/** Generate a new block using a wallet reserve key. */
CBlockTemplate* CreateNewBlockWithKey(
    CReserveKey& reservekey,
    CWallet* pwallet);

void BitcoinMiner(CWallet* pwallet, bool fProofOfStake);
void ThreadStakeMinter();

#endif // ENABLE_WALLET

extern uint64_t nLastBlockTx;
extern uint64_t nLastBlockSize;

extern double dHashesPerSec;
extern int64_t nHPSTimerStart;

#endif // SCHILLINGCOIN_NODE_MINER_H