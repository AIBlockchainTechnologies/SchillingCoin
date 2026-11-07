// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2018-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sigcache.h"

#include "cuckoocache.h"
#include "crypto/sha256.h"
#include "pubkey.h"
#include "random.h"
#include "uint256.h"
#include "util.h"
#include "util/system.h"

#include <algorithm>
#include <boost/thread.hpp>
#include <cstdint>

namespace {

/**
 * Valid signature cache, to avoid doing expensive ECDSA signature checking
 * twice for every transaction (once when accepted into memory pool, and
 * again when accepted into the block chain)
 */
class CSignatureCache
{
private:
    //! Entries are SHA256(nonce || signature hash || public key || signature):
    uint256 nonce;

    typedef CuckooCache::cache<uint256, SignatureCacheHasher> map_type;
    map_type setValid;

    boost::shared_mutex cs_sigcache;

public:
    CSignatureCache()
    {
        GetRandBytes(nonce.begin(), 32);
    }

    void ComputeEntry(
        uint256& entry,
        const uint256& hash,
        const std::vector<unsigned char>& vchSig,
        const CPubKey& pubkey) const
    {
        CSHA256 hasher;

        hasher.Write(nonce.begin(), 32);
        hasher.Write(hash.begin(), 32);

        if (pubkey.size() != 0)
            hasher.Write(pubkey.begin(), pubkey.size());

        if (!vchSig.empty())
            hasher.Write(vchSig.data(), vchSig.size());

        hasher.Finalize(entry.begin());
    }

    bool Get(const uint256& entry, bool erase)
    {
        boost::shared_lock<boost::shared_mutex> lock(cs_sigcache);
        return setValid.contains(entry, erase);
    }

    void Set(const uint256& entry)
    {
        boost::unique_lock<boost::shared_mutex> lock(cs_sigcache);
        setValid.insert(entry);
    }

    std::optional<std::pair<uint32_t, size_t>> setup_bytes(size_t n)
    {
        return setValid.setup_bytes(n);
    }
};

/* In previous versions of this code, signatureCache was a local static variable
 * in CachingTransactionSignatureChecker::VerifySignature. We initialize
 * signatureCache outside of VerifySignature to avoid the atomic operation per
 * call overhead associated with local static variables even though
 * signatureCache could be made local to VerifySignature.
 */
static CSignatureCache signatureCache;

} // namespace

bool InitSignatureCache(size_t max_size_bytes)
{
    const auto setup_results = signatureCache.setup_bytes(max_size_bytes);
    if (!setup_results) {
        return false;
    }

    const auto [num_elems, approx_size_bytes] = *setup_results;
    LogPrintf(
        "Using %zu MiB out of %zu MiB requested for signature cache, able to store %zu elements\n",
        approx_size_bytes >> 20,
        max_size_bytes >> 20,
        num_elems);
    return true;
}

bool CachingTransactionSignatureChecker::VerifySignature(
    const std::vector<unsigned char>& vchSig,
    const CPubKey& pubkey,
    const uint256& sighash) const
{
    uint256 entry;
    signatureCache.ComputeEntry(entry, sighash, vchSig, pubkey);

    if (signatureCache.Get(entry, !store))
        return true;

    if (!TransactionSignatureChecker::VerifySignature(vchSig, pubkey, sighash))
        return false;

    if (store)
        signatureCache.Set(entry);

    return true;
}