// Copyright (c) 2026 The SchillingCoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SchillingCoin_SRC_STUBS_ZEROCOIN_LEGACY_TYPES_H
#define SchillingCoin_SRC_STUBS_ZEROCOIN_LEGACY_TYPES_H

#include <list>
#include <vector>
#include "uint256.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "stubs/zerocoin_legacy_consensus.h"

// ---------------------------------------------------------------------------
// Dummy passthrough hash helpers (SCH expects these symbols)
// ---------------------------------------------------------------------------
inline uint256 GetSerialHash(const uint256& v) { return v; }
inline uint256 GetPubCoinHash(const uint256& v) { return v; }

// ---------------------------------------------------------------------------
// FAT STUB: CZerocoinMint (uint256-only version)
// ---------------------------------------------------------------------------
class CZerocoinMint {
public:
    CZerocoinMint() {}

    // Dummy fields matching SCH structure shape
    libzerocoin::CoinDenomination denom = libzerocoin::ZQ_ERROR;
    uint256 value;
    uint256 randomness;
    uint256 serialNumber;
    bool isUsed = false;
    uint8_t nVersion = 1;

    // Required getters
    libzerocoin::CoinDenomination GetDenomination() const { return denom; }
    uint256 GetValue() const { return value; }
    uint256 GetSerialNumber() const { return serialNumber; }
    uint256 GetRandomness() const { return randomness; }

    // Required setters
    void SetValue(const uint256& v) { value = v; }
    void SetSerialNumber(const uint256& s) { serialNumber = s; }
    void SetRandomness(const uint256& r) { randomness = r; }

    // Required hash helper
    uint256 GetPubcoinHash() const { return GetPubCoinHash(value); }

    // Required serialization stubs
    template<typename Stream>
    void Serialize(Stream& s, int, int) const {}

    template<typename Stream>
    void Unserialize(Stream& s, int, int) {}
};

// ---------------------------------------------------------------------------
// FAT STUB: CZerocoinSpend (uint256-only version)
// ---------------------------------------------------------------------------
class CZerocoinSpend {
public:
    CZerocoinSpend() {}

    uint256 coinSerial;
    uint256 pubCoin;
    libzerocoin::CoinDenomination denomination = libzerocoin::ZQ_ERROR;
    unsigned int nAccumulatorChecksum = 0;

    // Required getters
    uint256 GetSerial() const { return coinSerial; }
    uint256 GetPubCoin() const { return pubCoin; }

    // Required serialization stubs
    template<typename Stream>
    void Serialize(Stream& s, int, int) const {}

    template<typename Stream>
    void Unserialize(Stream& s, int, int) {}
};

// ---------------------------------------------------------------------------
// FAT STUB: CDeterministicMint (uint256-only version)
// ---------------------------------------------------------------------------
class CDeterministicMint {
public:
    uint8_t nVersion = 1;
    uint32_t nCount = 0;
    uint256 hashSeed;
    uint256 hashSerial;
    uint256 hashPubcoin;
    uint256 hashStake;

    CDeterministicMint() {}

    uint256 GetPubcoinHash() const { return hashPubcoin; }

    template<typename Stream>
    void Serialize(Stream& s, int, int) const {}

    template<typename Stream>
    void Unserialize(Stream& s, int, int) {}
};

// ---------------------------------------------------------------------------
// Stub PublicCoin class (SCH expects symbol)
// ---------------------------------------------------------------------------
namespace libzerocoin {
    class PublicCoin {
    public:
        PublicCoin() {}
    };
}

// ---------------------------------------------------------------------------
// Stub mint list function (SCH expects symbol)
// ---------------------------------------------------------------------------
inline void BlockToZerocoinMintList(const CBlock& block,
                                    std::list<CZerocoinMint>& listMints,
                                    bool filterInvalid)
{
    // Zerocoin removed — return empty list
}

// ---------------------------------------------------------------------------
// Stub spend list function (SCH expects symbol)
// ---------------------------------------------------------------------------
inline std::list<libzerocoin::CoinDenomination>
ZerocoinSpendListFromBlock(const CBlock& block, bool filterInvalid)
{
    return std::list<libzerocoin::CoinDenomination>();
}

// ---------------------------------------------------------------------------
// Stub TxOut → PublicCoin conversion (SCH expects symbol)
// ---------------------------------------------------------------------------
inline bool TxOutToPublicCoin(const CTxOut& out,
                              libzerocoin::PublicCoin& coin,
                              CValidationState& state)
{
    return false;
}

#endif // SchillingCoin_SRC_STUBS_ZEROCOIN_LEGACY_TYPES_H
