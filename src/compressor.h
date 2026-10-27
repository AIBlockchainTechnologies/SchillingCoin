// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SCHILLINGCOIN_COMPRESSOR_H
#define SCHILLINGCOIN_COMPRESSOR_H

#include "primitives/transaction.h"
#include "script/script.h"
#include "serialize.h"

bool CompressScript(
    const CScript& script,
    std::vector<unsigned char>& out);

unsigned int GetSpecialScriptSize(unsigned int nSize);

bool DecompressScript(
    CScript& script,
    unsigned int nSize,
    const std::vector<unsigned char>& in);

uint64_t CompressAmount(uint64_t nAmount);
uint64_t DecompressAmount(uint64_t nAmount);

/**
 * Compact formatter for scripts.
 *
 * It detects common script types and encodes them more efficiently.
 */
struct ScriptCompression
{
    static constexpr unsigned int SPECIAL_SCRIPTS = 6;

    template <typename Stream>
    void Ser(Stream& s, const CScript& script)
    {
        std::vector<unsigned char> compressed;

        if (CompressScript(script, compressed)) {
            s << CFlatData(compressed);
            return;
        }

        unsigned int size = script.size() + SPECIAL_SCRIPTS;
        s << VARINT(size);
        s.write(
            reinterpret_cast<const char*>(script.data()),
            script.size());
    }

    template <typename Stream>
    void Unser(Stream& s, CScript& script)
    {
        unsigned int size = 0;
        s >> VARINT(size);

        if (size < SPECIAL_SCRIPTS) {
            std::vector<unsigned char> compressed(
                GetSpecialScriptSize(size),
                0x00);

            s >> REF(CFlatData(compressed));
            DecompressScript(script, size, compressed);
            return;
        }

        size -= SPECIAL_SCRIPTS;

        if (size > MAX_SCRIPT_SIZE) {
            script << OP_RETURN;
            s.ignore(size);
        } else {
            script.resize(size);
            s >> REF(CFlatData(script));
        }
    }
};

/**
 * Formatter for compressed transaction amounts.
 */
struct AmountCompression
{
    template <typename Stream, typename I>
    void Ser(Stream& s, I value)
    {
        uint64_t compressed = CompressAmount(value);
        s << VARINT(compressed);
    }

    template <typename Stream, typename I>
    void Unser(Stream& s, I& value)
    {
        uint64_t compressed = 0;
        s >> VARINT(compressed);
        value = DecompressAmount(compressed);
    }
};

/**
 * Formatter for a compactly serialized CTxOut.
 */
struct TxOutCompression
{
    FORMATTER_METHODS(CTxOut, obj)
    {
        READWRITE(
            Using<AmountCompression>(obj.nValue),
            Using<ScriptCompression>(obj.scriptPubKey));
    }
};

#endif // SCHILLINGCOIN_COMPRESSOR_H