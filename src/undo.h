// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2016-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UNDO_H
#define BITCOIN_UNDO_H

#include "chain.h"
#include "compressor.h"
#include "primitives/transaction.h"
#include "serialize.h"

/** Undo information for a CTxIn
 *
 *  Contains the prevout's CTxOut being spent, and if this was the
 *  last output of the affected transaction, its metadata as well
 *  (coinbase or not, height, transaction version)
 */
class CTxInUndo
{
public:
    CTxOut txout;   // the txout data before being spent
    bool fCoinBase; // if the outpoint was the last unspent: whether it belonged to a coinbase
    bool fCoinStake;
    unsigned int nHeight; // if the outpoint was the last unspent: its height
    int nVersion;         // if the outpoint was the last unspent: its version

    CTxInUndo()
        : txout(),
          fCoinBase(false),
          fCoinStake(false),
          nHeight(0),
          nVersion(0)
    {}

    CTxInUndo(const CTxOut& txoutIn, bool fCoinBaseIn = false, bool fCoinStakeIn = false,
              unsigned int nHeightIn = 0, int nVersionIn = 0)
        : txout(txoutIn),
          fCoinBase(fCoinBaseIn),
          fCoinStake(fCoinStakeIn),
          nHeight(nHeightIn),
          nVersion(nVersionIn)
    {}

    // Copy constructor
    CTxInUndo(const CTxInUndo& other)
        : txout(other.txout),
          fCoinBase(other.fCoinBase),
          fCoinStake(other.fCoinStake),
          nHeight(other.nHeight),
          nVersion(other.nVersion)
    {}

    // Move constructor
    CTxInUndo(CTxInUndo&& other) noexcept
        : txout(std::move(other.txout)),
          fCoinBase(other.fCoinBase),
          fCoinStake(other.fCoinStake),
          nHeight(other.nHeight),
          nVersion(other.nVersion)
    {}

    // Copy assignment
    CTxInUndo& operator=(const CTxInUndo& other)
    {
        if (this != &other) {
            txout      = other.txout;
            fCoinBase  = other.fCoinBase;
            fCoinStake = other.fCoinStake;
            nHeight    = other.nHeight;
            nVersion   = other.nVersion;
        }
        return *this;
    }

    // Move assignment
    CTxInUndo& operator=(CTxInUndo&& other) noexcept
    {
        if (this != &other) {
            txout      = std::move(other.txout);
            fCoinBase  = other.fCoinBase;
            fCoinStake = other.fCoinStake;
            nHeight    = other.nHeight;
            nVersion   = other.nVersion;
        }
        return *this;
    }

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        return ::GetSerializeSize(VARINT(nHeight * 4 + (fCoinBase ? 2 : 0) + (fCoinStake ? 1 : 0)), nType, nVersion) +
               (nHeight > 0 ? ::GetSerializeSize(VARINT(this->nVersion), nType, nVersion) : 0) +
               ::GetSerializeSize(CTxOutCompressor(REF(txout)), nType, nVersion);
    }

    template <typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const
    {
        ::Serialize(s, VARINT(nHeight * 4 + (fCoinBase ? 2 : 0) + (fCoinStake ? 1 : 0)), nType, nVersion);
        if (nHeight > 0)
            ::Serialize(s, VARINT(this->nVersion), nType, nVersion);
        ::Serialize(s, CTxOutCompressor(REF(txout)), nType, nVersion);
    }

    template <typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion)
    {
        unsigned int nCode = 0;
        ::Unserialize(s, VARINT(nCode), nType, nVersion);
        nHeight   = nCode >> 2;
        fCoinBase = (nCode & 2) != 0;
        fCoinStake = (nCode & 1) != 0;
        if (nHeight > 0)
            ::Unserialize(s, VARINT(this->nVersion), nType, nVersion);
        ::Unserialize(s, REF(CTxOutCompressor(REF(txout))), nType, nVersion);
    }
};

/** Undo information for a CTransaction */
class CTxUndo
{
public:
    // undo information for all txins
    std::vector<CTxInUndo> vprevout;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(vprevout);
    }
};

/** Undo information for a CBlock */
class CBlockUndo
{
public:
    std::vector<CTxUndo> vtxundo; // for all but the coinbase

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(vtxundo);
    }

    bool WriteToDisk(CDiskBlockPos& pos, const uint256& hashBlock);
    bool ReadFromDisk(const CDiskBlockPos& pos, const uint256& hashBlock);
};

#endif // BITCOIN_UNDO_H