// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2020 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_MINER_H
#define SCHILLINGCOIN_MINER_H

#include "blockassembler.h"
#include "primitives/block.h"

#include <stdint.h>

class CReserveKey;
class CWallet;

#ifdef ENABLE_WALLET
    /** Run the miner threads */
    void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads);
    /** Generate a new block, without valid proof-of-work */
    CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey, CWallet* pwallet);

    void BitcoinMiner(CWallet* pwallet, bool fProofOfStake);
    void ThreadStakeMinter();
#endif // ENABLE_WALLET

extern double dHashesPerSec;
extern int64_t nHPSTimerStart;

#endif // SCHILLINGCOIN_MINER_H