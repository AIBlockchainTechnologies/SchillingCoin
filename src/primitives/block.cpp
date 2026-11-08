// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2015-2019 The PIVX developers
// Copyright (c) 2018-2020, 2026 The SchillingCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"

#include "hash.h"
#include "script/standard.h"
#include "script/sign.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "util.h"
#include "util/system.h"

#include <algorithm>
#include <iterator>
#include <limits>

uint256 CBlockHeader::GetHash() const
{
    if (nVersion < 4)
        return XEVAN(BEGIN(nVersion), END(nNonce));

    //if (nVersion == 4)
    //    return Hash(BEGIN(nVersion), END(nAccumulatorCheckpoint));
}

static void MarkVersionAsMostRecent(
    std::list<int32_t>& last_unique_versions,
    std::list<int32_t>::const_iterator version_it)
{
    if (version_it != last_unique_versions.cbegin()) {
        last_unique_versions.splice(
            last_unique_versions.begin(),
            last_unique_versions,
            version_it,
            std::next(version_it));
    }
}

static void SaveVersionAsMostRecent(
    std::list<int32_t>& last_unique_versions,
    const int32_t version)
{
    last_unique_versions.push_front(version);

    constexpr std::size_t MAX_BACKWARDS_LOOKUPS{7};
    if (last_unique_versions.size() > MAX_BACKWARDS_LOOKUPS) {
        last_unique_versions.pop_back();
    }
}

void CompressibleBlockHeader::Compress(
    const std::vector<CompressibleBlockHeader>& previous_blocks,
    std::list<int32_t>& last_unique_versions)
{
    if (previous_blocks.empty()) {
        SaveVersionAsMostRecent(last_unique_versions, nVersion);
        return;
    }

    const auto version_it = std::find(
        last_unique_versions.cbegin(),
        last_unique_versions.cend(),
        nVersion);

    if (version_it != last_unique_versions.cend()) {
        bit_field.SetVersionOffset(static_cast<uint8_t>(
            std::distance(last_unique_versions.cbegin(), version_it) + 1));
        MarkVersionAsMostRecent(last_unique_versions, version_it);
    } else {
        SaveVersionAsMostRecent(last_unique_versions, nVersion);
    }

    const auto& last_block = previous_blocks.back();

    bit_field.MarkAsCompressed(
        CompressedHeaderBitField::Flag::PREV_BLOCK_HASH);

    const int64_t time_diff =
        static_cast<int64_t>(nTime) -
        static_cast<int64_t>(last_block.nTime);

    if (time_diff <= std::numeric_limits<int16_t>::max() &&
        time_diff >= std::numeric_limits<int16_t>::min()) {
        time_offset = static_cast<int16_t>(time_diff);
        bit_field.MarkAsCompressed(
            CompressedHeaderBitField::Flag::TIMESTAMP);
    }

    if (nBits == last_block.nBits) {
        bit_field.MarkAsCompressed(
            CompressedHeaderBitField::Flag::NBITS);
    }
}

void CompressibleBlockHeader::Uncompress(
    const std::vector<CBlockHeader>& previous_blocks,
    std::list<int32_t>& last_unique_versions)
{
    if (previous_blocks.empty()) {
        SaveVersionAsMostRecent(last_unique_versions, nVersion);
        return;
    }

    const auto& last_block = previous_blocks.back();

    if (bit_field.IsVersionCompressed()) {
        const auto version_offset = bit_field.GetVersionOffset();

        if (version_offset <= last_unique_versions.size()) {
            auto version_it = last_unique_versions.begin();
            std::advance(version_it, version_offset - 1);
            nVersion = *version_it;
            MarkVersionAsMostRecent(last_unique_versions, version_it);
        }
    } else {
        SaveVersionAsMostRecent(last_unique_versions, nVersion);
    }

    if (bit_field.IsCompressed(
            CompressedHeaderBitField::Flag::PREV_BLOCK_HASH)) {
        hashPrevBlock = last_block.GetHash();
    }

    if (bit_field.IsCompressed(
            CompressedHeaderBitField::Flag::TIMESTAMP)) {
        nTime = last_block.nTime + time_offset;
    }

    if (bit_field.IsCompressed(
            CompressedHeaderBitField::Flag::NBITS)) {
        nBits = last_block.nBits;
    }
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        vtx.size());
    for (const auto& tx : vtx) {
        s << "  " << tx->ToString() << "\n";
    }
    return s.str();
}
void CBlock::print() const
{
    LogPrintf("%s", ToString());
}
bool CBlock::IsZerocoinStake() const
{
    return IsProofOfStake() && vtx[1]->HasZerocoinSpendInputs();
}