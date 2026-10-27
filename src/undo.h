// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Copyright (c) 2016-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_UNDO_H
#define SCHILLINGCOIN_UNDO_H

#include "chain.h"
#include "coins.h"
#include "compressor.h"
#include "consensus/consensus.h"
#include "primitives/transaction.h"
#include "serialize.h"

/**
 * Formatter for a spent Coin stored in transaction undo data.
 *
 * This preserves SCH's historical undo-record structure, including the
 * legacy transaction-version field when the height is nonzero.
 */
struct TxInUndoFormatter
{
    template <typename Stream>
    void Ser(Stream& s, const Coin& coin)
    {
        const uint32_t code =
            coin.nHeight * 4 +
            (coin.fCoinBase ? 2U : 0U) +
            (coin.fCoinStake ? 1U : 0U);

        ::Serialize(s, VARINT(code));

        if (coin.nHeight > 0) {
            ::Serialize(s, static_cast<unsigned char>(0));
        }

        ::Serialize(s, Using<TxOutCompression>(coin.out));
    }

    template <typename Stream>
    void Unser(Stream& s, Coin& coin)
    {
        uint32_t code = 0;

        ::Unserialize(s, VARINT(code));

        coin.nHeight = code >> 2;
        coin.fCoinBase = (code & 2) != 0;
        coin.fCoinStake = (code & 1) != 0;

        if (coin.nHeight > 0) {
            unsigned int nVersionDummy;
            ::Unserialize(s, VARINT(nVersionDummy));
        }

        ::Unserialize(s, Using<TxOutCompression>(coin.out));
    }
};

/** Undo information for a CTransaction */
class CTxUndo
{
public:
    std::vector<Coin> vprevout;

    SERIALIZE_METHODS(CTxUndo, obj)
    {
        READWRITE(Using<VectorFormatter<TxInUndoFormatter>>(obj.vprevout));
    }
};

/** Undo information for a CBlock */
class CBlockUndo
{
public:
    std::vector<CTxUndo> vtxundo; // for all but the coinbase

    SERIALIZE_METHODS(CBlockUndo, obj)
    {
        READWRITE(obj.vtxundo);
    }

    bool WriteToDisk(CDiskBlockPos& pos, const uint256& hashBlock);
    bool ReadFromDisk(const CDiskBlockPos& pos, const uint256& hashBlock);
};

#endif // SCHILLINGCOIN_UNDO_H