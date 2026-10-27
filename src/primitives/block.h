// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_PRIMITIVES_BLOCK_H
#define SCHILLINGCOIN_PRIMITIVES_BLOCK_H

#include "primitives/transaction.h"
#include "keystore.h"
#include "serialize.h"
#include "uint256.h"

/** Nodes collect new transactions into a block, hash them into a hash tree,
 * and scan through nonce values to make the block's hash satisfy proof-of-work
 * requirements.  When they solve the proof-of-work, they broadcast the block
 * to everyone and the block is added to the block chain.  The first transaction
 * in the block is a special one that creates a new coin owned by the creator
 * of the block.
 */
class CBlockHeader
{
public:
    // header
    static const int32_t CURRENT_VERSION = 6;     //!> Version 5 marks SchillingCoin reverse-hardfork blocks (Adds: PoS v2, Time Proto, CLTV, etc - Removes: Accumulator)
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint32_t nNonce;
    uint256 nAccumulatorCheckpoint;             // only for version 4.

    CBlockHeader()
    {
        SetNull();
    }

    SERIALIZE_METHODS(CBlockHeader, obj)
    {
        READWRITE(
            obj.nVersion,
            obj.hashPrevBlock,
            obj.hashMerkleRoot,
            obj.nTime,
            obj.nBits,
            obj.nNonce);

        //zerocoin active, header changes to include accumulator checksum
        if (obj.nVersion == 4)
            READWRITE(obj.nAccumulatorCheckpoint);
    }

    void SetNull()
    {
        nVersion = CBlockHeader::CURRENT_VERSION;
        hashPrevBlock.SetNull();
        hashMerkleRoot.SetNull();
        nTime = 0;
        nBits = 0;
        nNonce = 0;
        nAccumulatorCheckpoint.SetNull();
    }

    bool IsNull() const
    {
        return (nBits == 0);
    }

    uint256 GetHash() const;

    int64_t GetBlockTime() const
    {
        return (int64_t)nTime;
    }
};

class CBlock : public CBlockHeader
{
public:
    // network and disk
    std::vector<CTransaction> vtx;

    // Block signature, signed by the owner of the staking output for PoS blocks.
    std::vector<unsigned char> vchBlockSig;

    // memory only
    mutable CScript payee;
    mutable bool fChecked;

    CBlock()
    {
        SetNull();
    }

    CBlock(const CBlockHeader &header)
    {
        SetNull();
        *((CBlockHeader*)this) = header;
    }

    SERIALIZE_METHODS(CBlock, obj)
    {
        READWRITE(AsBase<CBlockHeader>(obj), obj.vtx);

        if (obj.vtx.size() > 1 && obj.vtx[1].IsCoinStake())
            READWRITE(obj.vchBlockSig);
    }

    void SetNull()
    {
        CBlockHeader::SetNull();
        vtx.clear();
        fChecked = false;
        payee = CScript();
        vchBlockSig.clear();
    }

    CBlockHeader GetBlockHeader() const
    {
        CBlockHeader block;
        block.nVersion       = nVersion;
        block.hashPrevBlock  = hashPrevBlock;
        block.hashMerkleRoot = hashMerkleRoot;
        block.nTime          = nTime;
        block.nBits          = nBits;
        block.nNonce         = nNonce;
        if (nVersion == 4)
            block.nAccumulatorCheckpoint = nAccumulatorCheckpoint;
        return block;
    }

    bool IsProofOfStake() const
    {
        return (vtx.size() > 1 && vtx[1].IsCoinStake());
    }

    bool IsProofOfWork() const
    {
        return !IsProofOfStake();
    }

    bool IsZerocoinStake() const;

    std::pair<COutPoint, unsigned int> GetProofOfStake() const
    {
        return IsProofOfStake() ? std::make_pair(vtx[1].vin[0].prevout, nTime) : std::make_pair(COutPoint(), (unsigned int)0);
    }

    std::string ToString() const;
    void print() const;
};

/** Describes a place in the block chain to another node such that if the
 * other node doesn't have the same branch, it can find a recent common trunk.
 * The further back it is, the further before the fork it may be.
 */
struct CBlockLocator
{
    std::vector<uint256> vHave;

    CBlockLocator() {}

    CBlockLocator(const std::vector<uint256>& vHaveIn)
    {
        vHave = vHaveIn;
    }

    SERIALIZE_METHODS(CBlockLocator, obj)
    {
        int nVersion = s.GetVersion();

        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);

        READWRITE(obj.vHave);
    }

    void SetNull()
    {
        vHave.clear();
    }

    bool IsNull()
    {
        return vHave.empty();
    }
};

#endif // SCHILLINGCOIN_PRIMITIVES_BLOCK_H