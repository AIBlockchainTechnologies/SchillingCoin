// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Copyright (c) 2014-2025 The Dash Core developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "validation.h"

#include "chain.h"
#include "checkpoints.h"
#include "main.h"
#include "sync.h"
#include "util.h"

CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator)
{
    // Find the first block the caller has in the main chain
    for (const uint256& hash : locator.vHave) {
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end()) {
            CBlockIndex* pindex = (*mi).second;
            if (chain.Contains(pindex))
                return pindex;
        }
    }
    return chain.Genesis();
}

bool IsInitialBlockDownload()
{
    LOCK(cs_main);
    const int chainHeight = chainActive.Height();
    if (fImporting || fReindex || fVerifyingBlocks || chainHeight < Checkpoints::GetTotalBlocksEstimate())
        return true;
    static bool lockIBDState = false;
    if (lockIBDState)
        return false;
    bool state = (chainHeight < pindexBestHeader->nHeight - 24 * 6 ||
            pindexBestHeader->GetBlockTime() < GetTime() - nMaxTipAge);
    if (!state)
        lockIBDState = true;
    return state;
}
