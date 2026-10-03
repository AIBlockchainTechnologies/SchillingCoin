// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2016-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UNDO_H
#define BITCOIN_UNDO_H

#include "chain.h"
#include "coins.h"
#include "compressor.h"
#include "consensus/consensus.h"
#include "primitives/transaction.h"
#include "serialize.h"

/**
 * Serializer for a spent Coin stored in transaction undo data.
 *
 * This preserves SCH's historical undo-record structure, including the
 * legacy transaction-version field when the height is nonzero.
 */
class TxInUndoSerializer
{
private:
    const Coin* coin;

public:
    explicit TxInUndoSerializer(const Coin* coinIn)
        : coin(coinIn)
    {
    }

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        const unsigned int code =
            coin->nHeight * 4 +
            (coin->fCoinBase ? 2 : 0) +
            (coin->fCoinStake ? 1 : 0);

        unsigned int size =
            ::GetSerializeSize(VARINT(code), nType, nVersion);

        if (coin->nHeight > 0) {
            int nVersionDummy = 0;

            size += ::GetSerializeSize(
                VARINT(nVersionDummy),
                nType,
                nVersion);
        }

        size += ::GetSerializeSize(
            CTxOutCompressor(REF(coin->out)),
            nType,
            nVersion);

        return size;
    }

    template <typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const
    {
        const unsigned int code =
            coin->nHeight * 4 +
            (coin->fCoinBase ? 2 : 0) +
            (coin->fCoinStake ? 1 : 0);

        ::Serialize(s, VARINT(code), nType, nVersion);

        if (coin->nHeight > 0) {
            int nVersionDummy = 0;

            ::Serialize(
                s,
                VARINT(nVersionDummy),
                nType,
                nVersion);
        }

        ::Serialize(
            s,
            CTxOutCompressor(REF(coin->out)),
            nType,
            nVersion);
    }
};

class TxInUndoDeserializer
{
private:
    Coin* coin;

public:
    explicit TxInUndoDeserializer(Coin* coinIn)
        : coin(coinIn)
    {
    }

    template <typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion)
    {
        unsigned int code = 0;

        ::Unserialize(
            s,
            VARINT(code),
            nType,
            nVersion);

        coin->nHeight = code >> 2;
        coin->fCoinBase = (code & 2) != 0;
        coin->fCoinStake = (code & 1) != 0;

        if (coin->nHeight > 0) {
            int nVersionDummy = 0;

            ::Unserialize(
                s,
                VARINT(nVersionDummy),
                nType,
                nVersion);
        }

        ::Unserialize(
            s,
            REF(CTxOutCompressor(coin->out)),
            nType,
            nVersion);
    }
};

static const size_t MAX_INPUTS_PER_BLOCK =
    MAX_BLOCK_SIZE_CURRENT /
    ::GetSerializeSize(
        CTxIn(),
        SER_NETWORK,
        PROTOCOL_VERSION);

/** Undo information for a CTransaction */
class CTxUndo
{
public:
    std::vector<Coin> vprevout;

    unsigned int GetSerializeSize(int nType, int nVersion) const
    {
        uint64_t count = vprevout.size();

        unsigned int size =
            ::GetSerializeSize(
                COMPACTSIZE(REF(count)),
                nType,
                nVersion);

        for (const Coin& prevout : vprevout) {
            size += ::GetSerializeSize(
                TxInUndoSerializer(&prevout),
                nType,
                nVersion);
        }

        return size;
    }

    template <typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const
    {
        uint64_t count = vprevout.size();

        ::Serialize(
            s,
            COMPACTSIZE(REF(count)),
            nType,
            nVersion);

        for (const Coin& prevout : vprevout) {
            ::Serialize(
                s,
                TxInUndoSerializer(&prevout),
                nType,
                nVersion);
        }
    }

    template <typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion)
    {
        uint64_t count = 0;

        ::Unserialize(
            s,
            COMPACTSIZE(count),
            nType,
            nVersion);

        if (count > MAX_INPUTS_PER_BLOCK)
            throw std::ios_base::failure(
                "Too many input undo records");

        vprevout.resize(count);

        for (Coin& prevout : vprevout) {
            ::Unserialize(
                s,
                REF(TxInUndoDeserializer(&prevout)),
                nType,
                nVersion);
        }
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