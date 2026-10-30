// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2020 The PIVX developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_BLOCKASSEMBLER_H
#define SCHILLINGCOIN_BLOCKASSEMBLER_H

class CBlock;
class CBlockIndex;
class CWallet;

/** Construct the coinstake transaction for a proof-of-stake block. */
bool SolveProofOfStake(
    CBlock* pblock,
    const CBlockIndex* pindexPrev,
    CWallet* pwallet);

#endif // SCHILLINGCOIN_BLOCKASSEMBLER_H