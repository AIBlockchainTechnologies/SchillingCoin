// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2011-2013 The PPCoin developers
// Copyright (c) 2013-2014 The NovaCoin Developers
// Copyright (c) 2014-2018 The BlackCoin Developers
// Copyright (c) 2015-2020 The PIVX developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "blockassembler.h"

#include "main.h"
#include "pow.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "util.h"
#include "wallet/wallet.h"

#include <cassert>
#include <vector>

#include <boost/thread.hpp>

bool SolveProofOfStake(
    CBlock* pblock,
    const CBlockIndex* pindexPrev,
    CWallet* pwallet)
{
    assert(pblock);
    assert(pindexPrev);
    assert(pwallet);

    boost::this_thread::interruption_point();

    pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);

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

    // Stake found.
    pblock->nTime = nTxNewTime;
    pblock->vtx[0].vout[0].SetEmpty();
    pblock->vtx.push_back(CTransaction(txCoinStake));

    return true;
}