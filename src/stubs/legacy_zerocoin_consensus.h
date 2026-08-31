// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SchillingCoin_SRC_STUBS_LEGACY_ZEROCOIN_CONSENSUS_H
#define SchillingCoin_SRC_STUBS_LEGACY_ZEROCOIN_CONSENSUS_H

#include <stdint.h>
#include <vector>

//
// ============================================================================
// Legacy Zerocoin Stub (SCH)
// ============================================================================
//
//  PURPOSE
//  -------
//  This header provides a *minimal*, consensus‑aware Zerocoin stub for SCH.
//  It exists for one reason:
//
//      ► Keep historical v4 Zerocoin blocks and headers parseable and valid,
//        without providing any live Zerocoin cryptography or privacy features.
//
//  WHAT IS PRESERVED
//  -----------------
//  - The *wire format* and enum values that are consensus‑critical:
//      • libzerocoin::SpendType
//      • libzerocoin::CoinDenomination
//      • libzerocoin::zerocoinDenomList
//      • helpers used by serialize.h and CBlockIndex
//  - Enough structure for Consensus::Params::Zerocoin_Params(...) to compile.
//
//  WHAT IS REMOVED
//  ---------------
//  - All cryptographic primitives (modulus N, groups, commitments, proofs).
//  - CoinSpend, PublicCoin, accumulator logic, serial number handling.
//  - Any ability to mint/spend new Zerocoins or validate Zerocoin proofs.
//
//  RUNTIME BEHAVIOR
//  ----------------
//  - SCH treats historical Zerocoin mints/spends as *opaque scripts*.
//  - Supply and accumulator checkpoints (nAC) are handled at the block/index
//    level (e.g. UpdateZSCHSupply, DataBaseAccChecksum) using legacy fields.
//  - No new Zerocoin transactions are allowed; sporks and mempool checks
//    enforce permanent deactivation.
//
//  IMPORTANT
//  ---------
//  - Do NOT extend this stub to re‑enable Zerocoin privacy.
//  - Do NOT change enum values or ordering; they are consensus‑critical.
//  - This file is intentionally small and boring—its job is to keep old
//    blocks valid while SCH moves forward as a non‑privacy coin.
// ============================================================================
//

namespace libzerocoin {

    //
    // SpendType (consensus‑critical)
    //
    // These values are serialized on‑chain and used in legacy Zerocoin
    // transactions. Changing them would break consensus with existing blocks.
    //
    enum class SpendType : uint8_t {
        SPEND      = 0,   // Regular Zerocoin spend
        STAKE      = 1,   // Zerocoin stake spend
        MN_REWARD  = 2    // Masternode reward spend
    };

    //
    // CoinDenomination (consensus‑critical)
    //
    // These denominations correspond to the historical Zerocoin amounts
    // used by SCH. They are referenced by CBlockIndex, txdb, and supply
    // tracking code via zerocoinDenomList and the helpers below.
    //
    enum CoinDenomination {
        ZQ_ERROR        = 0,
        ZQ_ONE          = 1,
        ZQ_FIVE         = 5,
        ZQ_TEN          = 10,
        ZQ_FIFTY        = 50,
        ZQ_ONE_HUNDRED  = 100,
        ZQ_FIVE_HUNDRED = 500,
        ZQ_ONE_THOUSAND = 1000,
        ZQ_FIVE_THOUSAND= 5000
    };

    //
    // Denomination list (used by CBlockIndex, main.cpp, txdb, etc.)
    //
    // This vector is iterated over when computing legacy Zerocoin supply
    // and accumulator checksum mappings. Its contents and ordering must
    // remain stable to preserve consensus with historical blocks.
    //
    static const std::vector<CoinDenomination> zerocoinDenomList = {
        ZQ_ONE,
        ZQ_FIVE,
        ZQ_TEN,
        ZQ_FIFTY,
        ZQ_ONE_HUNDRED,
        ZQ_FIVE_HUNDRED,
        ZQ_ONE_THOUSAND,
        ZQ_FIVE_THOUSAND
    };

    //
    // Minimal helpers required by serialize.h and CBlockIndex
    //
    // These functions bridge between the enum representation and the
    // integer form used in serialization and some legacy DB/index code.
    // They are intentionally simple and deterministic.
    //

    // Convert a CoinDenomination enum to its integer representation.
    inline int ZerocoinDenominationToInt(CoinDenomination denom)
    {
        return static_cast<int>(denom);
    }

    // Convert an integer back to a CoinDenomination enum.
    // Unknown values map to ZQ_ERROR, which callers should treat as invalid.
    inline CoinDenomination IntToZerocoinDenomination(int denom)
    {
        switch (denom) {
            case 1:    return ZQ_ONE;
            case 5:    return ZQ_FIVE;
            case 10:   return ZQ_TEN;
            case 50:   return ZQ_FIFTY;
            case 100:  return ZQ_ONE_HUNDRED;
            case 500:  return ZQ_FIVE_HUNDRED;
            case 1000: return ZQ_ONE_THOUSAND;
            case 5000: return ZQ_FIVE_THOUSAND;
            default:   return ZQ_ERROR;
        }
    }

    //
    // ZerocoinDenominationToAmount
    //
    // Maps a denomination to its value in base units (COIN).
    // This is used by legacy supply accounting and zSCH tracking logic.
    // Note: COIN is defined locally here to avoid pulling in full monetary
    //       headers; this stub is self‑contained.
    //
    inline int64_t ZerocoinDenominationToAmount(CoinDenomination denom)
    {
        static const int64_t COIN = 100000000LL;

        switch (denom) {
            case ZQ_ONE:          return 1      * COIN;
            case ZQ_FIVE:         return 5      * COIN;
            case ZQ_TEN:          return 10     * COIN;
            case ZQ_FIFTY:        return 50     * COIN;
            case ZQ_ONE_HUNDRED:  return 100    * COIN;
            case ZQ_FIVE_HUNDRED: return 500    * COIN;
            case ZQ_ONE_THOUSAND: return 1000   * COIN;
            case ZQ_FIVE_THOUSAND:return 5000   * COIN;
            default:              return 0;    // ZQ_ERROR or unknown
        }
    }

    //
    // ZerocoinParams stub (for Consensus::Params::Zerocoin_Params)
    //
    // In the original Zerocoin implementation, ZerocoinParams encapsulated
    // the modulus N and other cryptographic parameters. SCH no longer uses
    // those values at runtime, but Consensus::Params still exposes a
    // Zerocoin_Params(...) accessor for historical reasons.
    //
    // This stub class exists purely to satisfy type requirements and keep
    // serialization and consensus code compiling. It carries no secrets and
    // performs no cryptographic work.
    //
    class ZerocoinParams {
    public:
        // Default constructor: no state, no cryptography.
        ZerocoinParams() {}

        // Templated constructor: accepts any legacy parameter type but
        // intentionally ignores it. This allows existing code that passes
        // strings or structs to compile without re‑introducing Zerocoin.
        template <typename T>
        explicit ZerocoinParams(const T&) {}
    };

} // namespace libzerocoin

#endif // SchillingCoin_SRC_STUBS_LEGACY_ZEROCOIN_CONSENSUS_H