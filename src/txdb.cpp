// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "txdb.h"

#include "main.h"
#include "pow.h"
#include "uint256.h"

#include <stdint.h>

#include <boost/thread.hpp>

static const char DB_COIN = 'C';
static const char DB_BEST_BLOCK = 'B';

namespace {

struct CoinEntry
{
    COutPoint* outpoint;
    char key;

    explicit CoinEntry(const COutPoint* ptr)
        : outpoint(const_cast<COutPoint*>(ptr)),
          key(DB_COIN)
    {
    }

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        return ::GetSerializeSize(key, nType, nVersion) +
               ::GetSerializeSize(outpoint->hash, nType, nVersion) +
               ::GetSerializeSize(VARINT(outpoint->n), nType, nVersion);
    }

    template <typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const
    {
        ::Serialize(s, key, nType, nVersion);
        ::Serialize(s, outpoint->hash, nType, nVersion);
        ::Serialize(s, VARINT(outpoint->n), nType, nVersion);
    }

    template <typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion)
    {
        ::Unserialize(s, key, nType, nVersion);
        ::Unserialize(s, outpoint->hash, nType, nVersion);
        ::Unserialize(s, VARINT(outpoint->n), nType, nVersion);
    }
};

}

static void BatchWriteHashBestChain(CDBBatch& batch, const uint256& hash)
{
    batch.Write(DB_BEST_BLOCK, hash);
}

CCoinsViewDB::CCoinsViewDB(size_t nCacheSize, bool fMemory, bool fWipe) : db(GetDataDir() / "chainstate", nCacheSize, fMemory, fWipe)
{
}

bool CCoinsViewDB::GetCoin(
    const COutPoint& outpoint,
    Coin& coin) const
{
    return db.Read(
        CoinEntry(&outpoint),
        coin);
}

bool CCoinsViewDB::HaveCoin(
    const COutPoint& outpoint) const
{
    return db.Exists(
        CoinEntry(&outpoint));
}

uint256 CCoinsViewDB::GetBestBlock() const
{
    uint256 hashBestChain;

    if (!db.Read(DB_BEST_BLOCK, hashBestChain))
        return UINT256_ZERO;

    return hashBestChain;
}

bool CCoinsViewDB::BatchWrite(CCoinsMap& mapCoins, const uint256& hashBlock)
{
    CDBBatch batch;
    size_t count = 0;
    size_t changed = 0;

    for (CCoinsMap::iterator it = mapCoins.begin(); it != mapCoins.end();) {
        if (it->second.flags & CCoinsCacheEntry::DIRTY) {
            const CoinEntry entry(&it->first);

            if (it->second.coin.IsSpent())
                batch.Erase(entry);
            else
                batch.Write(entry, it->second.coin);

            ++changed;
        }

        ++count;

        CCoinsMap::iterator itOld = it++;
        mapCoins.erase(itOld);
    }

    if (!hashBlock.IsNull())
        BatchWriteHashBestChain(batch, hashBlock);

    LogPrint(
        "coindb",
        "Committing %u changed transaction outputs (out of %u) to coin database...\n",
        static_cast<unsigned int>(changed),
        static_cast<unsigned int>(count));

    return db.WriteBatch(batch);
}

CCoinsViewCursor* CCoinsViewDB::Cursor() const
{
    CCoinsViewDBCursor* cursor =
        new CCoinsViewDBCursor(
            const_cast<CDBWrapper*>(&db)->NewIterator(),
            GetBestBlock());

    COutPoint outpoint;
    CoinEntry seekEntry(&outpoint);

    cursor->pcursor->Seek(seekEntry);

    if (cursor->pcursor->Valid()) {
        CoinEntry entry(&cursor->keyTmp.second);

        if (cursor->pcursor->GetKey(entry))
            cursor->keyTmp.first = entry.key;
        else
            cursor->keyTmp.first = 0;
    } else {
        cursor->keyTmp.first = 0;
    }

    return cursor;
}

bool CCoinsViewDBCursor::GetKey(COutPoint& key) const
{
    if (keyTmp.first != DB_COIN)
        return false;

    key = keyTmp.second;
    return true;
}

bool CCoinsViewDBCursor::GetValue(Coin& coin) const
{
    if (!Valid())
        return false;

    return pcursor->GetValue(coin);
}

unsigned int CCoinsViewDBCursor::GetValueSize() const
{
    if (!Valid())
        return 0;

    return pcursor->GetValueSize();
}

bool CCoinsViewDBCursor::Valid() const
{
    return pcursor->Valid() &&
           keyTmp.first == DB_COIN;
}

void CCoinsViewDBCursor::Next()
{
    pcursor->Next();

    if (!pcursor->Valid()) {
        keyTmp.first = 0;
        return;
    }

    CoinEntry entry(&keyTmp.second);

    if (pcursor->GetKey(entry))
        keyTmp.first = entry.key;
    else
        keyTmp.first = 0;
}

size_t CCoinsViewDB::EstimateSize() const
{
    return db.EstimateSize(DB_COIN, (char)(DB_COIN+1));
}

CBlockTreeDB::CBlockTreeDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "blocks" / "index", nCacheSize, fMemory, fWipe)
{
}

bool CBlockTreeDB::WriteBlockIndex(const CDiskBlockIndex& blockindex)
{
    return Write(std::make_pair('b', blockindex.GetBlockHash()), blockindex);
}

bool CBlockTreeDB::ReadBlockFileInfo(int nFile, CBlockFileInfo& info)
{
    return Read(std::make_pair('f', nFile), info);
}

bool CBlockTreeDB::WriteReindexing(bool fReindexing)
{
    if (fReindexing)
        return Write('R', '1');
    else
        return Erase('R');
}

bool CBlockTreeDB::ReadReindexing(bool& fReindexing)
{
    fReindexing = Exists('R');
    return true;
}

bool CBlockTreeDB::ReadLastBlockFile(int& nFile)
{
    return Read('l', nFile);
}

bool CBlockTreeDB::WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo) {
    CDBBatch batch;
    for (std::vector<std::pair<int, const CBlockFileInfo*> >::const_iterator it=fileInfo.begin(); it != fileInfo.end(); it++) {
        batch.Write(std::make_pair('f', it->first), *it->second);
    }
    batch.Write('l', nLastFile);
    for (std::vector<const CBlockIndex*>::const_iterator it=blockinfo.begin(); it != blockinfo.end(); it++) {
        batch.Write(std::make_pair('b', (*it)->GetBlockHash()), CDiskBlockIndex(*it));
    }
    return WriteBatch(batch, true);
}

bool CBlockTreeDB::ReadTxIndex(const uint256& txid, CDiskTxPos& pos)
{
    return Read(std::make_pair('t', txid), pos);
}

bool CBlockTreeDB::WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >& vect)
{
    CDBBatch batch;
    for (std::vector<std::pair<uint256, CDiskTxPos> >::const_iterator it = vect.begin(); it != vect.end(); it++)
        batch.Write(std::make_pair('t', it->first), it->second);
    return WriteBatch(batch);
}

bool CBlockTreeDB::WriteFlag(const std::string& name, bool fValue)
{
    return Write(std::make_pair('F', name), fValue ? '1' : '0');
}

bool CBlockTreeDB::ReadFlag(const std::string& name, bool& fValue)
{
    char ch;
    if (!Read(std::make_pair('F', name), ch))
        return false;
    fValue = ch == '1';
    return true;
}

bool CBlockTreeDB::WriteInt(const std::string& name, int nValue)
{
    return Write(std::make_pair('I', name), nValue);
}

bool CBlockTreeDB::ReadInt(const std::string& name, int& nValue)
{
    return Read(std::make_pair('I', name), nValue);
}

bool CBlockTreeDB::LoadBlockIndexGuts()
{
    boost::scoped_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(std::make_pair('b', UINT256_ZERO));

    // Load mapBlockIndex
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            std::pair<char, uint256> key;

            if (pcursor->GetKey(key) && key.first == 'b') {
                CDiskBlockIndex diskindex;

                if (!pcursor->GetValue(diskindex))
                    return error("%s : failed to read block index value", __func__);

                // Construct block index object
                CBlockIndex* pindexNew = InsertBlockIndex(diskindex.GetBlockHash());
                pindexNew->pprev = InsertBlockIndex(diskindex.hashPrev);
                pindexNew->nHeight = diskindex.nHeight;
                pindexNew->nFile = diskindex.nFile;
                pindexNew->nDataPos = diskindex.nDataPos;
                pindexNew->nUndoPos = diskindex.nUndoPos;
                pindexNew->nVersion = diskindex.nVersion;
                pindexNew->hashMerkleRoot = diskindex.hashMerkleRoot;
                pindexNew->nTime = diskindex.nTime;
                pindexNew->nBits = diskindex.nBits;
                pindexNew->nNonce = diskindex.nNonce;
                pindexNew->nStatus = diskindex.nStatus;
                pindexNew->nTx = diskindex.nTx;

                // Zerocoin
                // is 'nAccumulatorCheckpoint' needed on version 5 blocks?
                // Can we add 'if (pindexNew->nVersion == 4)' here?
                pindexNew->nAccumulatorCheckpoint = diskindex.nAccumulatorCheckpoint;
                pindexNew->mapZerocoinSupply = diskindex.mapZerocoinSupply;

                // Proof Of Stake
                pindexNew->nMoneySupply = diskindex.nMoneySupply;
                pindexNew->nFlags = diskindex.nFlags;
                pindexNew->vStakeModifier = diskindex.vStakeModifier;

                if (pindexNew->nHeight <= Params().GetConsensus().height_last_PoW) {
                    if (!CheckProofOfWork(pindexNew->GetBlockHash(), pindexNew->nBits))
                        return error("LoadBlockIndex() : CheckProofOfWork failed: %s", pindexNew->ToString());
                }

                pcursor->Next();
            } else {
                break; // if shutdown requested or finished loading block index
            }
        } catch (const std::exception& e) {
            return error("%s : Deserialize or I/O error - %s", __func__, e.what());
        }
    }

    return true;
}

/**
 * Zerocoin database (zerocoin/)
 *
 * This build intentionally disables any on-disk Zerocoin DB activity.
 * The underlying LevelDB wrapper is never allocated here, so DATADIR/zerocoin
 * will never be created or opened by this code path. All methods are safe
 * no-ops that return failure/safe defaults so the rest of the node can run
 * and sync normally without Zerocoin validation or disk I/O.
 */

CZerocoinDB::CZerocoinDB(size_t nCacheSize, bool fMemory, bool fWipe)
{
    // Deliberately do not allocate the LevelDB wrapper under any condition.
    // This guarantees no creation or opening of DATADIR/zerocoin from this binary.
    pdb = nullptr;
}

CZerocoinDB::~CZerocoinDB()
{
    // Nothing to delete because we never allocate pdb here.
    pdb = nullptr;
}

/* All operations are intentionally inert. They return safe defaults and
   never touch disk or call into LevelDB. */

bool CZerocoinDB::WriteCoinMintBatch(const std::vector<std::pair<uint256, uint256>>& /*mintInfo*/)
{
    return false;
}

bool CZerocoinDB::ReadCoinMint(const uint256& /*pubcoinHash*/, uint256& /*hashTx*/)
{
    return false;
}

bool CZerocoinDB::ReadCoinMint(const CBigNum& /*bnPubcoin*/, uint256& /*txHash*/)
{
    return false;
}

bool CZerocoinDB::EraseCoinMint(const CBigNum& /*bnPubcoin*/)
{
    return false;
}

bool CZerocoinDB::WriteCoinSpendBatch(const std::vector<std::pair<uint256, uint256>>& /*spendInfo*/)
{
    return false;
}

bool CZerocoinDB::ReadCoinSpend(const CBigNum& /*bnSerial*/, uint256& /*txHash*/)
{
    return false;
}

bool CZerocoinDB::ReadCoinSpend(const uint256& /*hashSerial*/, uint256 &/*txHash*/)
{
    return false;
}

bool CZerocoinDB::EraseCoinSpend(const CBigNum& /*bnSerial*/)
{
    return false;
}

bool CZerocoinDB::WipeCoins(std::string /*strType*/)
{
    return false;
}

/* Legacy accumulator checksum methods — inert as well */

bool CZerocoinDB::WriteAccChecksum(const uint32_t& /*nChecksum*/, const uint8_t /*denom*/, const int /*nHeight*/)
{
    return false;
}

bool CZerocoinDB::ReadAccChecksum(const uint32_t& /*nChecksum*/, const uint8_t /*denom*/, int& /*nHeightRet*/)
{
    return false;
}

bool CZerocoinDB::EraseAccChecksum(const uint32_t& /*nChecksum*/, const uint8_t /*denom*/)
{
    return false;
}

bool CZerocoinDB::WipeAccChecksums()
{
    return false;
}