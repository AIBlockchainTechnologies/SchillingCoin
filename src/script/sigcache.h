// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_SCRIPT_SIGCACHE_H
#define SCHILLINGCOIN_SCRIPT_SIGCACHE_H

#include "script/interpreter.h"
#include "util/hasher.h"

#include <vector>

// DoS prevention: limit cache size to 32MiB (over 1000000 entries on 64-bit
// systems). Due to how we count cache size, actual memory usage is slightly
// more (~32.25 MiB)
static constexpr size_t DEFAULT_MAX_SIG_CACHE_BYTES{32 << 20};

class CPubKey;

class CachingTransactionSignatureChecker : public TransactionSignatureChecker
{
private:
    bool store;

public:
    CachingTransactionSignatureChecker(const CTransaction* txToIn, unsigned int nInIn, const CAmount& amountIn, bool storeIn, PrecomputedTransactionData& cachedHashesIn) :
        TransactionSignatureChecker(txToIn, nInIn, amountIn, cachedHashesIn),
        store(storeIn)
    {
    }

    bool VerifySignature(const std::vector<unsigned char>& vchSig, const CPubKey& vchPubKey, const uint256& sighash) const override;
};

[[nodiscard]] bool InitSignatureCache(size_t max_size_bytes);

#endif // SCHILLINGCOIN_SCRIPT_SIGCACHE_H