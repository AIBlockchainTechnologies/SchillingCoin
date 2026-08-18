// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2018 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#include "leveldbwrapper.h"
#include "main.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

class CCoins;
class uint256;

//! -dbcache default (MiB)
static const int64_t nDefaultDbCache = 100;
//! max. -dbcache in (MiB)
static const int64_t nMaxDbCache = sizeof(void*) > 4 ? 4096 : 1024;
//! min. -dbcache in (MiB)
static const int64_t nMinDbCache = 4;

struct CDiskTxPos : public CDiskBlockPos {
    unsigned int nTxOffset; // after header

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(*(CDiskBlockPos*)this);
        READWRITE(VARINT(nTxOffset));
    }

    CDiskTxPos(const CDiskBlockPos& blockIn, unsigned int nTxOffsetIn) : CDiskBlockPos(blockIn.nFile, blockIn.nPos), nTxOffset(nTxOffsetIn)
    {
    }

    CDiskTxPos()
    {
        SetNull();
    }

    void SetNull()
    {
        CDiskBlockPos::SetNull();
        nTxOffset = 0;
    }
};

/** CCoinsView backed by the LevelDB coin database (chainstate/) */
class CCoinsViewDB : public CCoinsView
{
protected:
    CLevelDBWrapper db;

public:
    CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetCoins(const uint256& txid, CCoins& coins) const;
    bool HaveCoins(const uint256& txid) const;
    uint256 GetBestBlock() const;
    bool BatchWrite(CCoinsMap& mapCoins, const uint256& hashBlock);
    bool GetStats(CCoinsStats& stats) const;
};

/** Access to the block database (blocks/index/) */
class CBlockTreeDB : public CLevelDBWrapper
{
public:
    CBlockTreeDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

private:
    CBlockTreeDB(const CBlockTreeDB&);
    void operator=(const CBlockTreeDB&);

public:
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo& fileinfo);
    bool ReadLastBlockFile(int& nFile);
    bool WriteReindexing(bool fReindex);
    bool ReadReindexing(bool& fReindex);
    bool ReadTxIndex(const uint256& txid, CDiskTxPos& pos);
    bool WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >& list);
    bool WriteFlag(const std::string& name, bool fValue);
    bool ReadFlag(const std::string& name, bool& fValue);
    bool WriteInt(const std::string& name, int nValue);
    bool ReadInt(const std::string& name, int& nValue);
    bool LoadBlockIndexGuts();
};

/**
 * Zerocoin database (zerocoin/)
 *
 * NOTE: This class previously inherited from CLevelDBWrapper which caused the
 * on-disk LevelDB to be opened during object construction (implicitly creating
 * DATADIR/zerocoin). It has been refactored to contain a pointer to a
 * CLevelDBWrapper (pdb) instead of inheriting it. The underlying LevelDB
 * wrapper is only allocated when explicitly requested (for example, when the
 * constructor is called with fMemory == true or when the zerocoin directory
 * already exists and you choose to open it). When pdb is nullptr, all DB
 * operations are no-ops or return safe defaults to preserve historical DB
 * compatibility without forcing disk I/O or directory creation.
 *
 * The public API is preserved so historical blocks and compatibility code can
 * remain unchanged; implementations must guard calls through pdb.
 */

class CZerocoinDB
{
public:
    // Constructor: nCacheSize is passed to the underlying wrapper if created.
    // If fMemory is true, callers may request an in-memory DB.
    CZerocoinDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);
    ~CZerocoinDB();

private:
    // non-copyable
    CZerocoinDB(const CZerocoinDB&);
    void operator=(const CZerocoinDB&);

    // Pointer to the underlying LevelDB wrapper. May be nullptr if not initialized.
    CLevelDBWrapper* pdb;

public:

    /** --------------------------------------------------------------------
     *  Zerocoin Mint Handling (Historical Only)
     *  --------------------------------------------------------------------
     *  Original SCH code used libzerocoin::PublicCoin. We replace it with
     *  uint256, which stores the hash/commitment of the mint.
     *
     *  NOTE: These functions DO NOT perform Zerocoin validation anymore.
     *        They only preserve DB compatibility for historical blocks.
     */

    bool WriteCoinMintBatch(const std::vector<std::pair<uint256, uint256>>& mintInfo);

    // Read mint by BigNum (legacy) — now treated as uint256 hash
    bool ReadCoinMint(const CBigNum& bnPubcoin, uint256& txHash);

    // Read mint by hash
    bool ReadCoinMint(const uint256& hashPubcoin, uint256& hashTx);

    /** --------------------------------------------------------------------
     *  Zerocoin Spend Handling (Historical Only)
     *  --------------------------------------------------------------------
     *  Original SCH code used libzerocoin::CoinSpend. We replace it with
     *  uint256, which stores the serial hash.
     */

    bool WriteCoinSpendBatch(const std::vector<std::pair<uint256, uint256>>& spendInfo);

    // Read spend by BigNum (legacy)
    bool ReadCoinSpend(const CBigNum& bnSerial, uint256& txHash);

    // Read spend by hash
    bool ReadCoinSpend(const uint256& hashSerial, uint256 &txHash);

    // Erase functions remain for DB cleanup compatibility
    bool EraseCoinMint(const CBigNum& bnPubcoin);
    bool EraseCoinSpend(const CBigNum& bnSerial);

    // Wipe all Zerocoin entries of a given type
    bool WipeCoins(std::string strType);

    /** --------------------------------------------------------------------
     *  Accumulator Checksum Handling
     *  --------------------------------------------------------------------
     *  SCH version‑4 blocks contain non‑zero accumulator checkpoints.
     *  We MUST preserve these DB entries to sync the chain.
     *
     *  libzerocoin::CoinDenomination is replaced with uint8_t.
     *  (All denominations fit safely in 8 bits.)
     */

    bool WriteAccChecksum(const uint32_t& nChecksum, const uint8_t denom, const int nHeight);
    bool ReadAccChecksum(const uint32_t& nChecksum, const uint8_t denom, int& nHeightRet);
    bool EraseAccChecksum(const uint32_t& nChecksum, const uint8_t denom);
    bool WipeAccChecksums();
};

#endif // BITCOIN_TXDB_H