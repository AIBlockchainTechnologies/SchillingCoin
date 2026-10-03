// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coins.h"

#include "consensus/consensus.h"
#include "memusage.h"
#include "random.h"
#include "util.h"

#include <assert.h>
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>

bool CCoinsView::GetCoin(const COutPoint& outpoint, Coin& coin) const { return false; }
bool CCoinsView::HaveCoin(const COutPoint& outpoint) const { return false; }
uint256 CCoinsView::GetBestBlock() const { return UINT256_ZERO; }
bool CCoinsView::BatchWrite(CCoinsMap& mapCoins, const uint256& hashBlock) { return false; }
CCoinsViewCursor* CCoinsView::Cursor() const { return nullptr; }

CCoinsViewBacked::CCoinsViewBacked(CCoinsView* viewIn) : base(viewIn) {}
bool CCoinsViewBacked::GetCoin(const COutPoint& outpoint, Coin& coin) const { return base->GetCoin(outpoint, coin); }
bool CCoinsViewBacked::HaveCoin(const COutPoint& outpoint) const { return base->HaveCoin(outpoint); }
uint256 CCoinsViewBacked::GetBestBlock() const { return base->GetBestBlock(); }
void CCoinsViewBacked::SetBackend(CCoinsView& viewIn) { base = &viewIn; }
bool CCoinsViewBacked::BatchWrite(CCoinsMap& mapCoins, const uint256& hashBlock) { return base->BatchWrite(mapCoins, hashBlock); }
CCoinsViewCursor* CCoinsViewBacked::Cursor() const { return base->Cursor(); }
size_t CCoinsViewBacked::EstimateSize() const { return base->EstimateSize(); }

SaltedOutpointHasher::SaltedOutpointHasher()
    : k0(GetRand(std::numeric_limits<uint64_t>::max())),
      k1(GetRand(std::numeric_limits<uint64_t>::max()))
{
}

CCoinsViewCache::CCoinsViewCache(CCoinsView* baseIn)
    : CCoinsViewBacked(baseIn),
      cachedCoinsUsage(0)
{
}

size_t CCoinsViewCache::DynamicMemoryUsage() const
{
    return memusage::DynamicUsage(cacheCoins) + cachedCoinsUsage;
}

CCoinsMap::iterator CCoinsViewCache::FetchCoin(const COutPoint& outpoint) const
{
    CCoinsMap::iterator it = cacheCoins.find(outpoint);
    if (it != cacheCoins.end())
        return it;

    Coin tmp;
    if (!base->GetCoin(outpoint, tmp))
        return cacheCoins.end();

    CCoinsMap::iterator ret = cacheCoins.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(outpoint),
        std::forward_as_tuple(std::move(tmp))).first;

    if (ret->second.coin.IsSpent()) {
        // The parent only has an empty entry for this outpoint; we can consider
        // our version as fresh.
        ret->second.flags = CCoinsCacheEntry::FRESH;
    }

    cachedCoinsUsage += ret->second.coin.DynamicMemoryUsage();
    return ret;
}

bool CCoinsViewCache::GetCoin(const COutPoint& outpoint, Coin& coin) const
{
    CCoinsMap::const_iterator it = FetchCoin(outpoint);
    if (it != cacheCoins.end()) {
        coin = it->second.coin;
        return true;
    }

    return false;
}

void CCoinsViewCache::AddCoin(const COutPoint& outpoint, Coin&& coin, bool possibleOverwrite)
{
    assert(!coin.IsSpent());

    if (coin.out.scriptPubKey.IsUnspendable())
        return;

    CCoinsMap::iterator it;
    bool inserted;

    std::tie(it, inserted) = cacheCoins.emplace(
        std::piecewise_construct,
        std::forward_as_tuple(outpoint),
        std::tuple<>());

    bool fresh = false;

    if (!inserted)
        cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();

    if (!possibleOverwrite) {
        if (!it->second.coin.IsSpent())
            throw std::logic_error("Adding new coin that replaces non-pruned entry");

        fresh = !(it->second.flags & CCoinsCacheEntry::DIRTY);
    }

    it->second.coin = std::move(coin);
    it->second.flags |= CCoinsCacheEntry::DIRTY |
                        (fresh ? CCoinsCacheEntry::FRESH : 0);

    cachedCoinsUsage += it->second.coin.DynamicMemoryUsage();
}

void AddCoins(CCoinsViewCache& cache, const CTransaction& tx, int nHeight)
{
    const bool fCoinBase = tx.IsCoinBase();
    const bool fCoinStake = tx.IsCoinStake();
    const uint256& txid = tx.GetHash();

    for (size_t i = 0; i < tx.vout.size(); ++i) {
        cache.AddCoin(
            COutPoint(txid, i),
            Coin(tx.vout[i], nHeight, fCoinBase, fCoinStake),
            false);
    }
}

void CCoinsViewCache::SpendCoin(const COutPoint& outpoint, Coin* moveout)
{
    CCoinsMap::iterator it = FetchCoin(outpoint);
    if (it == cacheCoins.end())
        return;

    cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();

    if (moveout)
        *moveout = std::move(it->second.coin);

    if (it->second.flags & CCoinsCacheEntry::FRESH) {
        cacheCoins.erase(it);
    } else {
        it->second.flags |= CCoinsCacheEntry::DIRTY;
        it->second.coin.Clear();
    }
}

static const Coin coinEmpty;

const Coin& CCoinsViewCache::AccessCoin(const COutPoint& outpoint) const
{
    CCoinsMap::const_iterator it = FetchCoin(outpoint);

    if (it == cacheCoins.end())
        return coinEmpty;

    return it->second.coin;
}

bool CCoinsViewCache::HaveCoin(const COutPoint& outpoint) const
{
    CCoinsMap::const_iterator it = FetchCoin(outpoint);

    return it != cacheCoins.end() &&
           !it->second.coin.IsSpent();
}

bool CCoinsViewCache::HaveCoinInCache(const COutPoint& outpoint) const
{
    return cacheCoins.find(outpoint) != cacheCoins.end();
}

uint256 CCoinsViewCache::GetBestBlock() const
{
    if (hashBlock.IsNull())
        hashBlock = base->GetBestBlock();
    return hashBlock;
}

void CCoinsViewCache::SetBestBlock(const uint256& hashBlockIn)
{
    hashBlock = hashBlockIn;
}

bool CCoinsViewCache::BatchWrite(CCoinsMap& mapCoins, const uint256& hashBlockIn)
{
    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            CCoinsMap::iterator itUs = cacheCoins.find(it->first);

            if (itUs == cacheCoins.end()) {
                if (!(it->second.flags & CCoinsCacheEntry::FRESH &&
                      it->second.coin.IsSpent())) {
                    CCoinsCacheEntry& entry = cacheCoins[it->first];

                    entry.coin = std::move(it->second.coin);
                    cachedCoinsUsage += entry.coin.DynamicMemoryUsage();
                    entry.flags = CCoinsCacheEntry::DIRTY;

                    if (it->second.flags & CCoinsCacheEntry::FRESH)
                        entry.flags |= CCoinsCacheEntry::FRESH;
                }
            } else {
                if ((it->second.flags & CCoinsCacheEntry::FRESH) &&
                    !itUs->second.coin.IsSpent()) {
                    throw std::logic_error(
                        "FRESH flag misapplied to cache entry for base transaction with spendable outputs");
                }

                if ((itUs->second.flags & CCoinsCacheEntry::FRESH) &&
                    it->second.coin.IsSpent()) {
                    cachedCoinsUsage -= itUs->second.coin.DynamicMemoryUsage();
                    cacheCoins.erase(itUs);
                } else {
                    cachedCoinsUsage -= itUs->second.coin.DynamicMemoryUsage();

                    itUs->second.coin = std::move(it->second.coin);

                    cachedCoinsUsage += itUs->second.coin.DynamicMemoryUsage();
                    itUs->second.flags |= CCoinsCacheEntry::DIRTY;
                }
            }
        }

        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
    }

    hashBlock = hashBlockIn;
    return true;
}

bool CCoinsViewCache::Flush()
{
    const bool fOk = base->BatchWrite(cacheCoins, hashBlock);
    cacheCoins.clear();
    cachedCoinsUsage = 0;
    return fOk;
}

void CCoinsViewCache::Uncache(const COutPoint& outpoint)
{
    CCoinsMap::iterator it = cacheCoins.find(outpoint);

    if (it != cacheCoins.end() && it->second.flags == 0) {
        cachedCoinsUsage -= it->second.coin.DynamicMemoryUsage();
        cacheCoins.erase(it);
    }
}

unsigned int CCoinsViewCache::GetCacheSize() const
{
    return cacheCoins.size();
}

CAmount CCoinsViewCache::GetValueIn(const CTransaction& tx) const
{
    if (tx.IsCoinBase())
        return 0;

    if (tx.HasZerocoinSpendInputs())
        return tx.GetZerocoinSpent();

    CAmount nResult = 0;

    for (const CTxIn& txin : tx.vin)
        nResult += AccessCoin(txin.prevout).out.nValue;

    return nResult;
}

bool CCoinsViewCache::HaveInputs(const CTransaction& tx) const
{
    if (tx.IsCoinBase() || tx.HasZerocoinSpendInputs())
        return true;

    for (const CTxIn& txin : tx.vin) {
        if (!HaveCoin(txin.prevout))
            return false;
    }

    return true;
}

double CCoinsViewCache::GetPriority(const CTransaction& tx, int nHeight) const
{
    if (tx.IsCoinBase() || tx.IsCoinStake())
        return 0.0;

    double dResult = 0.0;

    for (const CTxIn& txin : tx.vin) {
        const Coin& coin = AccessCoin(txin.prevout);

        if (coin.IsSpent())
            continue;

        if (coin.nHeight < static_cast<unsigned int>(nHeight)) {
            dResult += coin.out.nValue *
                       (nHeight - coin.nHeight);
        }
    }

    return tx.ComputePriority(dResult);
}

int CCoinsViewCache::GetCoinDepthAtHeight(
    const COutPoint& output,
    int nHeight) const
{
    const Coin& coin = AccessCoin(output);

    if (!coin.IsSpent())
        return nHeight - coin.nHeight + 1;

    return -1;
}

static const size_t MAX_OUTPUTS_PER_BLOCK =
    MAX_BLOCK_SIZE_CURRENT /
    ::GetSerializeSize(
        CTxOut(),
        SER_NETWORK,
        PROTOCOL_VERSION);

const Coin& AccessByTxid(
    const CCoinsViewCache& view,
    const uint256& txid)
{
    COutPoint iter(txid, 0);

    while (iter.n < MAX_OUTPUTS_PER_BLOCK) {
        const Coin& coin = view.AccessCoin(iter);

        if (!coin.IsSpent())
            return coin;

        ++iter.n;
    }

    return coinEmpty;
}