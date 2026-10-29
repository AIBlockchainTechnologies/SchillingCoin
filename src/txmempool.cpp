// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txmempool.h"

#include "clientversion.h"
#include "main.h"
#include "policy/fees.h"
#include "streams.h"
#include "util.h"
#include "utilmoneystr.h"
#include "util/system.h"
#include "version.h"

CTxMemPoolEntry::CTxMemPoolEntry() : nFee(0), nTxSize(0), nModSize(0), nTime(0), dPriority(0.0)
{
    nHeight = MEMPOOL_HEIGHT;
}

CTxMemPoolEntry::CTxMemPoolEntry(const CTransaction& _tx, const CAmount& _nFee, int64_t _nTime, double _dPriority, unsigned int _nHeight) : tx(_tx), nFee(_nFee), nTime(_nTime), dPriority(_dPriority), nHeight(_nHeight)
{
    nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);

    nModSize = tx.CalculateModifiedSize(nTxSize);
}

CTxMemPoolEntry::CTxMemPoolEntry(const CTxMemPoolEntry& other)
{
    *this = other;
}

double
CTxMemPoolEntry::GetPriority(unsigned int currentHeight) const
{
    CAmount nValueIn = tx.GetValueOut() + nFee;
    double deltaPriority = ((double)(currentHeight - nHeight) * nValueIn) / nModSize;
    double dResult = dPriority + deltaPriority;
    return dResult;
}

CTxMemPool::CTxMemPool(CBlockPolicyEstimator* estimator, const CFeeRate& _minRelayFee) :
    nTransactionsUpdated(0),
    minerPolicyEstimator(estimator),
    minRelayFee(_minRelayFee)
{
    // Sanity checks off by default for performance, because otherwise
    // accepting transactions becomes O(N^2) where N is the number
    // of transactions in the pool.
    fSanityCheck = false;
}

bool CTxMemPool::isSpent(const COutPoint& outpoint)
{
    LOCK(cs);
    return mapNextTx.count(outpoint);
}

unsigned int CTxMemPool::GetTransactionsUpdated() const
{
    LOCK(cs);
    return nTransactionsUpdated;
}

void CTxMemPool::AddTransactionsUpdated(unsigned int n)
{
    LOCK(cs);
    nTransactionsUpdated += n;
}

bool CTxMemPool::addUnchecked(const uint256& hash, const CTxMemPoolEntry& entry, bool validFeeEstimate)
{
    // Add to memory pool without checking anything.
    // Used by main.cpp AcceptToMemoryPool(), which DOES do
    // all the appropriate checks.
    LOCK(cs);

    mapTx[hash] = entry;

    const CTransaction& tx = mapTx[hash].GetTx();

    if (!tx.HasZerocoinSpendInputs()) {
        for (unsigned int i = 0; i < tx.vin.size(); ++i) {
            mapNextTx[tx.vin[i].prevout] = CInPoint(&tx, i);
        }
    }

    ++nTransactionsUpdated;
    totalTxSize += entry.GetTxSize();

    if (minerPolicyEstimator) {
        minerPolicyEstimator->processTransaction(
            mapTx[hash],
            validFeeEstimate);
    }

    return true;
}

void CTxMemPool::remove(const CTransaction& origTx, std::list<CTransaction>& removed, bool fRecursive)
{
    // Remove transaction from memory pool
    LOCK(cs);

    std::deque<uint256> txToRemove;
    txToRemove.push_back(origTx.GetHash());

    if (fRecursive && !mapTx.count(origTx.GetHash())) {
        // If recursively removing but origTx isn't in the mempool,
        // be sure to remove any children that are in the pool. This can
        // happen during chain re-orgs if origTx isn't re-accepted into
        // the mempool for any reason.
        for (unsigned int i = 0; i < origTx.vout.size(); ++i) {
            std::map<COutPoint, CInPoint>::iterator it =
                mapNextTx.find(COutPoint(origTx.GetHash(), i));

            if (it == mapNextTx.end())
                continue;

            txToRemove.push_back(it->second.ptx->GetHash());
        }
    }

    while (!txToRemove.empty()) {
        const uint256 hash = txToRemove.front();
        txToRemove.pop_front();

        std::map<uint256, CTxMemPoolEntry>::iterator entryIt =
            mapTx.find(hash);

        if (entryIt == mapTx.end())
            continue;

        const CTransaction& tx = entryIt->second.GetTx();

        if (fRecursive) {
            for (unsigned int i = 0; i < tx.vout.size(); ++i) {
                std::map<COutPoint, CInPoint>::iterator it =
                    mapNextTx.find(COutPoint(hash, i));

                if (it == mapNextTx.end())
                    continue;

                txToRemove.push_back(it->second.ptx->GetHash());
            }
        }

        for (const CTxIn& txin : tx.vin)
            mapNextTx.erase(txin.prevout);

        removed.push_back(tx);
        totalTxSize -= entryIt->second.GetTxSize();

        if (minerPolicyEstimator) {
            minerPolicyEstimator->removeTx(hash, false);
        }

        mapTx.erase(entryIt);
        ++nTransactionsUpdated;
    }
}

void CTxMemPool::removeCoinbaseSpends(const CCoinsViewCache* pcoins, unsigned int nMemPoolHeight)
{
    // Remove transactions spending a coinbase or coinstake which are now immature.
    LOCK(cs);

    std::list<CTransaction> transactionsToRemove;

    for (std::map<uint256, CTxMemPoolEntry>::const_iterator it = mapTx.begin();
         it != mapTx.end(); it++) {
        const CTransaction& tx = it->second.GetTx();

        for (const CTxIn& txin : tx.vin) {
            std::map<uint256, CTxMemPoolEntry>::const_iterator it2 =
                mapTx.find(txin.prevout.hash);

            if (it2 != mapTx.end())
                continue;

            const Coin& coin = pcoins->AccessCoin(txin.prevout);

            if (fSanityCheck)
                assert(!coin.IsSpent());

            if (coin.IsSpent() ||
                ((coin.IsCoinBase() || coin.IsCoinStake()) &&
                 (signed long)nMemPoolHeight - coin.nHeight <
                     Params().GetConsensus().nCoinbaseMaturity)) {
                transactionsToRemove.push_back(tx);
                break;
            }
        }
    }

    for (const CTransaction& tx : transactionsToRemove) {
        std::list<CTransaction> removed;
        remove(tx, removed, true);
    }
}

void CTxMemPool::removeConflicts(const CTransaction& tx, std::list<CTransaction>& removed)
{
    // Remove transactions which depend on inputs of tx, recursively
    std::list<CTransaction> result;
    LOCK(cs);
    for (const CTxIn& txin : tx.vin) {
        std::map<COutPoint, CInPoint>::iterator it = mapNextTx.find(txin.prevout);
        if (it != mapNextTx.end()) {
            const CTransaction& txConflict = *it->second.ptx;
            if (txConflict != tx) {
                remove(txConflict, removed, true);
            }
        }
    }
}

/**
 * Called when a block is connected. Removes from mempool and updates the miner fee estimator.
 */
void CTxMemPool::removeForBlock(const std::vector<CTransaction>& vtx, unsigned int nBlockHeight, std::list<CTransaction>& conflicts)
{
    LOCK(cs);

    std::vector<const CTxMemPoolEntry*> entries;
    entries.reserve(vtx.size());

    for (const CTransaction& tx : vtx) {
        const uint256 hash = tx.GetHash();

        std::map<uint256, CTxMemPoolEntry>::const_iterator entryIt =
            mapTx.find(hash);

        if (entryIt != mapTx.end())
            entries.push_back(&entryIt->second);
    }

    // Update policy estimates before removing the confirmed transactions
    // from the mempool.
    if (minerPolicyEstimator) {
        minerPolicyEstimator->processBlock(nBlockHeight, entries);
    }

    for (const CTransaction& tx : vtx) {
        std::list<CTransaction> dummy;
        remove(tx, dummy, false);
        removeConflicts(tx, conflicts);
        ClearPrioritisation(tx.GetHash());
    }
}

void CTxMemPool::clear()
{
    LOCK(cs);
    mapTx.clear();
    mapNextTx.clear();
    totalTxSize = 0;
    ++nTransactionsUpdated;
}

bool CTxMemPool::HasNoInputsOf(const CTransaction& tx) const
{
    if (tx.HasZerocoinSpendInputs())
        return true;

    LOCK(cs);

    for (const CTxIn& txin : tx.vin) {
        if (mapTx.count(txin.prevout.hash) != 0)
            return false;
    }

    return true;
}

void CTxMemPool::check(const CCoinsViewCache* pcoins) const
{
    if (!fSanityCheck)
        return;

    LogPrint(BCLog::MEMPOOL, "Checking mempool with %u transactions and %u inputs\n",
             (unsigned int)mapTx.size(),
             (unsigned int)mapNextTx.size());

    uint64_t checkTotal = 0;

    CCoinsViewCache mempoolDuplicate(const_cast<CCoinsViewCache*>(pcoins));

    LOCK(cs);
    std::list<const CTxMemPoolEntry*> waitingOnDependants;

    for (std::map<uint256, CTxMemPoolEntry>::const_iterator it = mapTx.begin();
         it != mapTx.end(); it++) {

        unsigned int i = 0;
        checkTotal += it->second.GetTxSize();

        const CTransaction& tx = it->second.GetTx();
        bool fDependsWait = false;

        for (const CTxIn& txin : tx.vin) {

            // Check that every mempool transaction's inputs refer to available coins,
            // or other mempool tx's.
            std::map<uint256, CTxMemPoolEntry>::const_iterator it2 =
                mapTx.find(txin.prevout.hash);

            if (it2 != mapTx.end()) {
                const CTransaction& tx2 = it2->second.GetTx();
                assert(tx2.vout.size() > txin.prevout.n &&
                       !tx2.vout[txin.prevout.n].IsNull());
                fDependsWait = true;
            } else if (!txin.IsZerocoinSpend()) {
                const Coin& coin = pcoins->AccessCoin(txin.prevout);
                assert(!coin.IsSpent());
            }

            // Check whether its inputs are marked in mapNextTx.
            if (!txin.IsZerocoinSpend()) {
                std::map<COutPoint, CInPoint>::const_iterator it3 =
                    mapNextTx.find(txin.prevout);

                assert(it3 != mapNextTx.end());
                assert(it3->second.ptx == &tx);
                assert(it3->second.n == i);
            } else {
                fDependsWait = false;
            }

            i++;
        }

        if (fDependsWait)
            waitingOnDependants.push_back(&it->second);
        else {
            CValidationState state;
            CTxUndo undo;
            PrecomputedTransactionData precomTxData(tx);

            assert(CheckInputs(
                tx,
                state,
                mempoolDuplicate,
                false,
                0,
                false,
                precomTxData,
                nullptr));

            UpdateCoins(tx, state, mempoolDuplicate, undo, 1000000);
        }
    }

    unsigned int stepsSinceLastRemove = 0;

    while (!waitingOnDependants.empty()) {
        const CTxMemPoolEntry* entry = waitingOnDependants.front();
        waitingOnDependants.pop_front();

        CValidationState state;

        if (!mempoolDuplicate.HaveInputs(entry->GetTx())) {
            waitingOnDependants.push_back(entry);
            stepsSinceLastRemove++;
            assert(stepsSinceLastRemove < waitingOnDependants.size());
        } else {
            PrecomputedTransactionData precomTxData(entry->GetTx());

            assert(CheckInputs(
                entry->GetTx(),
                state,
                mempoolDuplicate,
                false,
                0,
                false,
                precomTxData,
                nullptr));

            CTxUndo undo;
            UpdateCoins(entry->GetTx(), state, mempoolDuplicate, undo, 1000000);
            stepsSinceLastRemove = 0;
        }
    }

    for (std::map<COutPoint, CInPoint>::const_iterator it = mapNextTx.begin();
         it != mapNextTx.end(); it++) {

        uint256 hash = it->second.ptx->GetHash();

        std::map<uint256, CTxMemPoolEntry>::const_iterator it2 =
            mapTx.find(hash);

        assert(it2 != mapTx.end());

        const CTransaction& tx = it2->second.GetTx();

        assert(&tx == it->second.ptx);
        assert(tx.vin.size() > it->second.n);
        assert(it->first == it->second.ptx->vin[it->second.n].prevout);
    }

    assert(totalTxSize == checkTotal);
}

void CTxMemPool::queryHashes(std::vector<uint256>& vtxid)
{
    vtxid.clear();

    LOCK(cs);
    vtxid.reserve(mapTx.size());
    for (std::map<uint256, CTxMemPoolEntry>::iterator mi = mapTx.begin(); mi != mapTx.end(); ++mi)
        vtxid.push_back((*mi).first);
}

void CTxMemPool::getTransactions(std::set<uint256>& setTxid)
{
    setTxid.clear();

    LOCK(cs);
    for (std::map<uint256, CTxMemPoolEntry>::iterator mi = mapTx.begin(); mi != mapTx.end(); ++mi)
        setTxid.insert((*mi).first);
}

bool CTxMemPool::lookup(uint256 hash, CTransaction& result) const
{
    LOCK(cs);
    std::map<uint256, CTxMemPoolEntry>::const_iterator i = mapTx.find(hash);
    if (i == mapTx.end()) return false;
    result = i->second.GetTx();
    return true;
}

CFeeRate CTxMemPool::estimateFee(int nBlocks) const
{
    LOCK(cs);

    if (!minerPolicyEstimator) {
        return CFeeRate(0);
    }

    return minerPolicyEstimator->estimateFee(nBlocks);
}

CFeeRate CTxMemPool::estimateSmartFee(int nBlocks, int* answerFoundAtBlocks) const
{
    LOCK(cs);

    if (!minerPolicyEstimator) {
        if (answerFoundAtBlocks) {
            *answerFoundAtBlocks = 0;
        }
        return CFeeRate(0);
    }

    FeeCalculation feeCalc;
    const CFeeRate estimate = minerPolicyEstimator->estimateSmartFee(
        nBlocks,
        &feeCalc,
        true);

    if (answerFoundAtBlocks) {
        *answerFoundAtBlocks = feeCalc.returnedTarget;
    }

    return estimate;
}

bool CTxMemPool::WriteFeeEstimates(CAutoFile& fileout) const
{
    LOCK(cs);

    if (!minerPolicyEstimator) {
        return false;
    }

    return minerPolicyEstimator->Write(fileout);
}

bool CTxMemPool::ReadFeeEstimates(CAutoFile& filein)
{
    LOCK(cs);

    if (!minerPolicyEstimator) {
        return false;
    }

    return minerPolicyEstimator->Read(filein);
}

void CTxMemPool::PrioritiseTransaction(const uint256 hash, const std::string strHash, double dPriorityDelta, const CAmount& nFeeDelta)
{
    {
        LOCK(cs);
        std::pair<double, CAmount>& deltas = mapDeltas[hash];
        deltas.first += dPriorityDelta;
        deltas.second += nFeeDelta;
    }
    LogPrintf("PrioritiseTransaction: %s priority += %f, fee += %d\n", strHash, dPriorityDelta, FormatMoney(nFeeDelta));
}

void CTxMemPool::ApplyDeltas(const uint256 hash, double& dPriorityDelta, CAmount& nFeeDelta)
{
    LOCK(cs);
    std::map<uint256, std::pair<double, CAmount> >::iterator pos = mapDeltas.find(hash);
    if (pos == mapDeltas.end())
        return;
    const std::pair<double, CAmount>& deltas = pos->second;
    dPriorityDelta += deltas.first;
    nFeeDelta += deltas.second;
}

void CTxMemPool::ClearPrioritisation(const uint256 hash)
{
    LOCK(cs);
    mapDeltas.erase(hash);
}

CCoinsViewMemPool::CCoinsViewMemPool(CCoinsView* baseIn, CTxMemPool& mempoolIn)
    : CCoinsViewBacked(baseIn),
      mempool(mempoolIn)
{
}

bool CCoinsViewMemPool::GetCoin(const COutPoint& outpoint, Coin& coin) const
{
    // If an entry in the mempool exists, always return that one, as it's guaranteed to never
    // conflict with the underlying cache, and it cannot have pruned entries (as it contains full)
    // transactions. First checking the underlying cache risks returning a pruned entry instead.
    CTransaction tx;
    if (mempool.lookup(outpoint.hash, tx)) {
        if (outpoint.n < tx.vout.size()) {
            coin = Coin(tx.vout[outpoint.n], MEMPOOL_HEIGHT, false, false);
            return true;
        } else {
            return false;
        }
    }

    return (base->GetCoin(outpoint, coin) && !coin.IsSpent());
}

bool CCoinsViewMemPool::HaveCoin(const COutPoint& outpoint) const
{
    return mempool.exists(outpoint) || base->HaveCoin(outpoint);
}