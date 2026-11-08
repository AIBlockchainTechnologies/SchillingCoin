// Copyright (c) 2011-2013 The PPCoin developers
// Copyright (c) 2013-2014 The NovaCoin Developers
// Copyright (c) 2014-2018 The BlackCoin Developers
// Copyright (c) 2015-2020 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_KERNEL_H
#define SCHILLINGCOIN_KERNEL_H

#include "stakeinput.h"
#include "validation.h"

#include <optional>

class CCoinsViewCache;

class CStakeKernel
{
public:
    /**
     * Construct a complete SCH stake kernel for the candidate parent branch.
     *
     * Returns std::nullopt when the historical or current stake modifier,
     * stake origin, or another required kernel component cannot be resolved.
     */
    static std::optional<CStakeKernel> Create(const CBlockIndex* pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int nTimeTx);

    // Return stake kernel hash.
    uint256 GetHash() const;

    // Check that the kernel hash meets the target required.
    bool CheckKernelHash(bool fSkipLog = false) const;

private:
    CStakeKernel() = default;

    // Kernel message hashed.
    CDataStream stakeModifier{CDataStream(SER_GETHASH, 0)};
    int nTimeBlockFrom{0};
    CDataStream stakeUniqueness{CDataStream(SER_GETHASH, 0)};
    int nTime{0};

    // Hash target.
    unsigned int nBits{0};
    CAmount stakeValue{0};
};

/* PoS Validation */

/*
 * Stake                Check if stakeInput can stake a block on top of pindexPrev
 *
 * @param[in]   pindexPrev      index of the parent block of the block being staked
 * @param[in]   stakeInput      input for the coinstake
 * @param[in]   nBits           target difficulty bits
 * @param[in]   nTimeTx         new blocktime
 * @return      bool            true if stake kernel hash meets target protocol
 */
bool Stake(Chainstate& chainstate, const CBlockIndex* pindexPrev, CStakeInput* stakeInput, unsigned int nBits, int64_t& nTimeTx);

/*
 * CheckProofOfStake    Check if block has valid proof of stake
 *
 * @param[in]   block           block with the proof being verified
 * @param[out]  strError        string returning error message (if any, else empty)
 * @param[in]   pindexPrev      index of the parent block
 *                              (if nullptr, it will be searched in mapBlockIndex)
 * @return      bool            true if the block has a valid proof of stake
 */
bool CheckProofOfStake(const CBlock& block, std::string& strError, const CBlockIndex* pindexPrev, const CCoinsViewCache& view);

/*
 * GetStakeKernelHash   Return stake kernel of a block
 *
 * @param[out]  hashRet         hash of the kernel (set by this function)
 * @param[in]   block           block with the kernel to return
 * @param[in]   pindexPrev      index of the parent block
 *                              (if nullptr, it will be searched in mapBlockIndex)
 * @return      bool            false if kernel cannot be initialized, true otherwise
 */
bool GetStakeKernelHash(uint256& hashRet, const CBlock& block, const CBlockIndex* pindexPrev = nullptr);

#endif // SCHILLINGCOIN_KERNEL_H