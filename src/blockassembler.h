// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2020 The PIVX developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_BLOCKASSEMBLER_H
#define SCHILLINGCOIN_BLOCKASSEMBLER_H

#include "amount.h"
#include "primitives/block.h"

#include <cstdint>
#include <vector>

class CBlock;
class CBlockHeader;
class CBlockIndex;
class CScript;
class CWallet;

struct CBlockTemplate {
    CBlock block;
    std::vector<CAmount> vTxFees;
    std::vector<int64_t> vTxSigOps;
};

/** Construct the coinstake transaction for a proof-of-stake block. */
bool SolveProofOfStake(CBlock* pblock, const CBlockIndex* pindexPrev, CWallet* pwallet);

/** Generate a new block template without valid proof of work. */
CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn, CWallet* pwallet, bool fProofOfStake);

/** Modify the extranonce and update the merkle root. */
void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce);

/** Update block time and testnet difficulty when required. */
void UpdateTime(CBlockHeader* block, const CBlockIndex* pindexPrev);

extern uint64_t nLastBlockTx;
extern uint64_t nLastBlockSize;

#endif // SCHILLINGCOIN_BLOCKASSEMBLER_H